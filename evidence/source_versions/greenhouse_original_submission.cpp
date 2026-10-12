// SIT315 M4.T1D: greenhouse scenario planning with MPI and OpenMP.
// I compare four watering plans for each set of sensor readings.
// I use GREENHOUSE_LOCAL for CPU development tests, not distributed evidence.
#ifndef GREENHOUSE_LOCAL
#define OMPI_SKIP_MPICXX 1
#define MPICH_SKIP_MPICXX 1
#include <mpi.h>
#endif
#include <omp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// I keep the run settings together so all four modes use the same inputs.
struct Options {
    std::string mode = "sequential", workload = "uniform", input;
    long long jobs = 64;
    int threads = 2, chunk = 32, horizon = 48;
    bool verify = true, help = false;
};
struct Reading { double soil, temperature, humidity; };
struct Result {
    long long id;
    int plan, scenarios;
    double score, water;
};
// I count both windows and forecast steps because equal window counts can hide unequal work.
struct Stats {
    long long jobs = 0, steps = 0, plans[4] = {0,0,0,0};
    unsigned long long checksum = 0;
    double score = 0, water = 0;
};

// I reject partially parsed numbers, such as 12abc, instead of accepting a misleading setting.
static long long integer(const std::string& text, const std::string& name) {
    std::size_t used = 0;
    long long value;
    try { value = std::stoll(text, &used); }
    catch (...) { throw std::runtime_error("Invalid integer for " + name + ": " + text); }
    if (used != text.size()) throw std::runtime_error("Invalid integer for " + name);
    return value;
}
// I check the arguments before starting a long run.
// Full verification stores every result, so I limit it and use --no-verify for the stress case.
static Options parse(int argc, char** argv) {
    Options o;
    for (int i=1; i<argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") { o.help = true; continue; }
        if (key == "--no-verify") { o.verify = false; continue; }
        if (i+1 == argc) throw std::runtime_error("Missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--mode") o.mode = value;
        else if (key == "--workload") o.workload = value;
        else if (key == "--input") o.input = value;
        else if (key == "--jobs") o.jobs = integer(value, key);
        else if (key == "--threads" || key == "--chunk" || key == "--horizon") {
            const auto n = integer(value,key);
            if (n < 1 || n > 100000) throw std::runtime_error("Out of range: " + key);
            if (key == "--threads") o.threads = static_cast<int>(n);
            if (key == "--chunk") o.chunk = static_cast<int>(n);
            if (key == "--horizon") o.horizon = static_cast<int>(n);
        } else throw std::runtime_error("Unknown option: " + key);
    }
    if (o.mode != "sequential" && o.mode != "mpi" && o.mode != "hybrid-static" && o.mode != "hybrid-dynamic")
        throw std::runtime_error("Mode must be sequential, mpi, hybrid-static or hybrid-dynamic");
    if (o.workload != "uniform" && o.workload != "skewed")
        throw std::runtime_error("Workload must be uniform or skewed");
    if (o.jobs < 1 || o.jobs > 1000000000LL) throw std::runtime_error("Jobs must be between 1 and 1000000000");
    if (o.threads > 64 || o.chunk > 65536 || o.horizon > 720) throw std::runtime_error("Maximum threads/chunk/horizon: 64/65536/720");
    if (o.verify && o.jobs > 200000) throw std::runtime_error("Full verification limit is 200000 jobs; use --no-verify for the stress test");
    return o;
}
static void usage() {
    std::cout << "Usage: greenhouse --mode sequential|mpi|hybrid-static|hybrid-dynamic\n"
              << "  --jobs N --threads T --chunk C --horizon HOURS\n"
              << "  --workload uniform|skewed [--input readings.csv] [--no-verify]\n"
              << "Defaults: 64 jobs, 2 threads, chunk 32, horizon 48, full verification\n"
              << "CSV header: soil_pct,temperature_c,humidity_pct\n";
}
static double real(const std::string& text) {
    std::size_t used = 0;
    const double v = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(v)) throw std::runtime_error("Invalid numeric CSV cell");
    return v;
}
// I read a small set of seed readings and reject missing, non-finite or out-of-range values.
static std::vector<Reading> readCSV(const std::string& path) {
    if (path.empty()) return {};
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open input: " + path);
    std::string line;
    if (!std::getline(in,line)) throw std::runtime_error("Empty CSV");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "soil_pct,temperature_c,humidity_pct") throw std::runtime_error("CSV header must be soil_pct,temperature_c,humidity_pct");
    std::vector<Reading> readings;
    int row = 1;
    while (std::getline(in,line)) {
        ++row;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string a,b,c,extra;
        if (!std::getline(ss,a,',') || !std::getline(ss,b,',') || !std::getline(ss,c,',') || std::getline(ss,extra,','))
            throw std::runtime_error("CSV needs three cells at row " + std::to_string(row));
        Reading r;
        try { r = {real(a),real(b),real(c)}; }
        catch (...) { throw std::runtime_error("Invalid CSV number at row " + std::to_string(row)); }
        if (r.soil < 0 || r.soil > 100 || r.temperature < -20 || r.temperature > 70 || r.humidity < 0 || r.humidity > 100)
            throw std::runtime_error("Sensor reading out of range at row " + std::to_string(row));
        readings.push_back(r);
        if (readings.size() > 10000) throw std::runtime_error("Input limit: 10000 seed readings");
    }
    if (readings.empty()) throw std::runtime_error("CSV has no sensor readings");
    return readings;
}
// I mix IDs into repeatable values so the generated inputs do not depend on rank or thread order.
static std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static Reading readingFor(long long id, const std::vector<Reading>& seeds) {
    if (!seeds.empty()) return seeds[static_cast<std::size_t>(id) % seeds.size()];
    // I use the job ID so every mode receives the same synthetic reading.
    const auto x = mix(static_cast<std::uint64_t>(id));
    return {25.0 + static_cast<double>(x % 5000)/100.0,
            18.0 + static_cast<double>((x >> 16) % 1700)/100.0,
            35.0 + static_cast<double>((x >> 32) % 5000)/100.0};
}
static int scenarioCount(long long id, const Options& o) {
    // I put the expensive windows first to expose the imbalance in a fixed split.
    return o.workload == "skewed" && id < (o.jobs+3)/4 ? 64 : 8;
}
// I keep this calculation identical in every execution mode.
// These model coefficients create a repeatable workload; they are not calibrated crop advice.
static Result forecast(long long id, const Options& o, const std::vector<Reading>& seeds) {
    const Reading base = readingFor(id,seeds);
    const int scenarios = scenarioCount(id,o);
    Result best{id,0,scenarios,std::numeric_limits<double>::infinity(),0};
    // I compare no watering against 2, 4 or 6 moisture points every 12 hours.
    for (int plan=0; plan<4; ++plan) {
        double totalScore=0, totalWater=0;
        for (int s=0; s<scenarios; ++s) {
            double soil = base.soil, water = 0, penalty = 0;
            const auto hash = mix(static_cast<std::uint64_t>(id)*131ULL + static_cast<unsigned>(s));
            // I reuse the same uncertainty for a given window and scenario when comparing the plans.
            const double uncertainty = 0.75 + static_cast<double>(hash % 1000)/2000.0;
            for (int hour=0; hour<o.horizon; ++hour) {
                const int clock = hour % 24;
                const double daylight = std::max(0.0,1.0-std::abs(clock-12)/6.0);
                const double temperature = base.temperature + 6.0*daylight - 2.0;
                const double humidity = std::clamp(base.humidity - 12.0*daylight,0.0,100.0);
                const double loss = (0.15 + 0.022*std::max(0.0,temperature-10.0)) *
                                    (1.25-humidity/200.0) * uncertainty;
                if (hour % 12 == 0) { soil += 2.0*plan; water += 2.0*plan; }
                soil = std::clamp(soil-loss,0.0,100.0);
                // I penalise dry and overly wet soil, then include watering cost in the final score.
                const double dry = std::max(0.0,40.0-soil), wet = std::max(0.0,soil-75.0);
                penalty += dry*dry + 0.5*wet*wet;
            }
            totalScore += penalty/o.horizon + 0.20*water;
            totalWater += water;
        }
        // I average the scenario scores and keep the plan with the lowest combined cost.
        const double score = totalScore/scenarios;
        if (score < best.score) { best.plan=plan; best.score=score; best.water=totalWater/scenarios; }
    }
    return best;
}
static void add(Stats& st, const Result& r, int horizon) {
    ++st.jobs;
    st.steps += static_cast<long long>(r.scenarios)*4*horizon;
    ++st.plans[r.plan];
    st.score += r.score; st.water += r.water;
    // I add fingerprints so the checksum does not depend on processing order.
    st.checksum += mix(static_cast<std::uint64_t>(r.id)) ^
                   mix(static_cast<std::uint64_t>(std::llround(r.score*1e6))) ^
                   mix(static_cast<std::uint64_t>(r.plan+1));
}
static void merge(Stats& a, const Stats& b) {
    a.jobs+=b.jobs; a.steps+=b.steps; a.checksum+=b.checksum;
    a.score+=b.score; a.water+=b.water;
    for(int i=0;i<4;++i) a.plans[i]+=b.plans[i];
}
static void process(long long begin, long long end, const Options& o, const std::vector<Reading>& seeds,
                    Stats& stats, std::vector<Result>& results, std::vector<long long>& threadJobs,
                    bool parallel, bool dynamic) {
    // I allocate result slots before the parallel loop, so each iteration writes to its own slot.
    const auto offset = results.size();
    if(o.verify) results.resize(offset+static_cast<std::size_t>(end-begin));
    const int threads = parallel ? o.threads : 1;
    // I give each thread separate statistics to avoid racing on a shared running total.
    std::vector<Stats> perThread(static_cast<std::size_t>(threads));
    // I use the same loop with either static assignment or one-window dynamic scheduling.
    omp_set_schedule(dynamic ? omp_sched_dynamic : omp_sched_static, dynamic ? 1 : 0);
    #pragma omp parallel num_threads(threads) if(parallel)
    {
        const int tid = omp_get_thread_num();
        #pragma omp for schedule(runtime)
        for(long long id=begin; id<end; ++id) {
            const auto r = forecast(id,o,seeds);
            add(perThread[tid],r,o.horizon);
            if(o.verify) results[offset+static_cast<std::size_t>(id-begin)] = r;
        }
    }
    // I merge the thread totals after the parallel region has finished.
    for(int i=0;i<threads;++i) { merge(stats,perThread[i]); threadJobs[i]+=perThread[i].jobs; }
}
#ifndef GREENHOUSE_LOCAL
// I describe the result fields explicitly and resize the datatype for C++ structure padding.
static MPI_Datatype resultType() {
    Result r{};
    MPI_Aint base, offsets[5];
    MPI_Get_address(&r,&base);
    MPI_Get_address(&r.id,&offsets[0]); MPI_Get_address(&r.plan,&offsets[1]);
    MPI_Get_address(&r.scenarios,&offsets[2]); MPI_Get_address(&r.score,&offsets[3]);
    MPI_Get_address(&r.water,&offsets[4]);
    for(auto& x:offsets) x-=base;
    int sizes[5]={1,1,1,1,1};
    MPI_Datatype types[5]={MPI_LONG_LONG_INT,MPI_INT,MPI_INT,MPI_DOUBLE,MPI_DOUBLE}, raw,type;
    MPI_Type_create_struct(5,sizes,offsets,types,&raw);
    MPI_Type_create_resized(raw,0,sizeof(Result),&type);
    MPI_Type_commit(&type); MPI_Type_free(&raw);
    return type;
}
#endif
// I allow a small relative tolerance when comparing floating-point results.
static bool close(double a,double b) { return std::abs(a-b) <= 1e-9*std::max({1.0,std::abs(a),std::abs(b)}); }

int main(int argc,char** argv) {
    int rank=0, ranks=1;
#ifndef GREENHOUSE_LOCAL
    int provided=0;
    // I request FUNNELED support because only the main thread makes MPI calls.
    MPI_Init_thread(&argc,&argv,MPI_THREAD_FUNNELED,&provided);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank); MPI_Comm_size(MPI_COMM_WORLD,&ranks);
    if(provided < MPI_THREAD_FUNNELED) { if(rank==0) std::cerr<<"MPI_THREAD_FUNNELED unavailable\n"; MPI_Abort(MPI_COMM_WORLD,1); }
#endif
    try {
        const Options o = parse(argc,argv);
        if(o.help) { if(rank==0) usage();
#ifndef GREENHOUSE_LOCAL
            MPI_Finalize();
#endif
            return 0;
        }
        if(o.mode=="sequential" && ranks!=1) throw std::runtime_error("Sequential mode requires exactly one MPI rank");
        std::vector<Reading> seeds;
        int inputOK=1; std::string error;
        // I let rank zero read the file and share its success status so every rank follows the same path.
        if(rank==0) { try { seeds=readCSV(o.input); } catch(const std::exception& e) { inputOK=0; error=e.what(); } }
#ifndef GREENHOUSE_LOCAL
        MPI_Bcast(&inputOK,1,MPI_INT,0,MPI_COMM_WORLD);
#endif
        if(!inputOK) {
            if(rank==0) std::cerr<<"Input error: "<<error<<'\n';
#ifndef GREENHOUSE_LOCAL
            MPI_Finalize();
#endif
            return 1;
        }
#ifndef GREENHOUSE_LOCAL
        int seedCount=static_cast<int>(seeds.size());
        MPI_Bcast(&seedCount,1,MPI_INT,0,MPI_COMM_WORLD);
        // I send explicit doubles so C++ structure padding cannot affect the input.
        std::vector<double> packed(static_cast<std::size_t>(seedCount)*3);
        if(rank==0) for(int i=0;i<seedCount;++i) { packed[3*i]=seeds[i].soil; packed[3*i+1]=seeds[i].temperature; packed[3*i+2]=seeds[i].humidity; }
        MPI_Bcast(packed.data(),seedCount*3,MPI_DOUBLE,0,MPI_COMM_WORLD);
        if(rank!=0) { seeds.resize(seedCount); for(int i=0;i<seedCount;++i) seeds[i]={packed[3*i],packed[3*i+1],packed[3*i+2]}; }
#endif
        const bool parallel=o.mode=="hybrid-static" || o.mode=="hybrid-dynamic";
        const int threads=parallel ? o.threads : 1;
        omp_set_dynamic(0);
        Stats local,total; std::vector<Result> results;
        std::vector<long long> threadJobs(static_cast<std::size_t>(threads),0);
        char hostname[256]="local-test";
#ifndef GREENHOUSE_LOCAL
        int hostLength=0; MPI_Get_processor_name(hostname,&hostLength);
#endif
        if(rank==0) std::cout<<"Starting "<<o.mode<<": "<<o.jobs<<" windows, "<<ranks<<" ranks, "<<threads<<" threads/rank"<<std::endl;
#ifndef GREENHOUSE_LOCAL
        MPI_Barrier(MPI_COMM_WORLD);
#endif
        // I start timing after input setup and include the scheduling and gathering work below.
        const double start=omp_get_wtime();
        double workSeconds=0;
        if(o.mode=="hybrid-dynamic") {
#ifndef GREENHOUSE_LOCAL
            // I claim chunks on the main thread so the OpenMP threads never call MPI.
            long long* counter=nullptr; MPI_Win win;
            // I expose the counter only on rank zero, which also takes part in the calculation.
            MPI_Win_allocate(rank==0 ? sizeof(long long) : 0,sizeof(long long),MPI_INFO_NULL,MPI_COMM_WORLD,&counter,&win);
            MPI_Win_lock_all(0,win);
            // I publish the initial zero and wait for every rank before allowing chunk claims.
            if(rank==0) { *counter=0; MPI_Win_sync(win); }
            MPI_Barrier(MPI_COMM_WORLD);
            while(true) {
                long long begin=0, amount=o.chunk;
                // I atomically add the chunk size and receive the old value as this rank's starting ID.
                MPI_Fetch_and_op(&amount,&begin,MPI_LONG_LONG_INT,0,0,MPI_SUM,win);
                // I complete the claim before reading its returned value or calculating the chunk.
                MPI_Win_flush(0,win);
                if(begin>=o.jobs) break;
                // I clip the final chunk so I never calculate beyond the requested window count.
                process(begin,std::min(o.jobs,begin+amount),o,seeds,local,results,threadJobs,true,true);
            }
            workSeconds=omp_get_wtime()-start;
            MPI_Win_unlock_all(win); MPI_Win_free(&win);
#else
            for(long long begin=0;begin<o.jobs;begin+=o.chunk)
                process(begin,std::min(o.jobs,begin+o.chunk),o,seeds,local,results,threadJobs,true,true);
            workSeconds=omp_get_wtime()-start;
#endif
        } else {
            // For the fixed split, I distribute leftover windows as well as the evenly divided part.
            const long long begin=(o.jobs/ranks)*rank+std::min<long long>(rank,o.jobs%ranks);
            const long long count=o.jobs/ranks+(rank<o.jobs%ranks ? 1 : 0);
            process(begin,begin+count,o,seeds,local,results,threadJobs,parallel,false);
            workSeconds=omp_get_wtime()-start;
        }
        std::vector<long long> rankJobs(ranks),rankSteps(ranks),allThreadJobs(static_cast<std::size_t>(ranks)*threads);
        std::vector<double> rankSeconds(ranks);
        std::vector<char> hostnames(static_cast<std::size_t>(ranks)*256);
#ifndef GREENHOUSE_LOCAL
        // I reduce counts and sums for the final totals, then gather the per-rank diagnostics separately.
        long long localInts[6]={local.jobs,local.steps,local.plans[0],local.plans[1],local.plans[2],local.plans[3]},globalInts[6]={};
        double localDoubles[2]={local.score,local.water},globalDoubles[2]={};
        MPI_Reduce(localInts,globalInts,6,MPI_LONG_LONG_INT,MPI_SUM,0,MPI_COMM_WORLD);
        MPI_Reduce(localDoubles,globalDoubles,2,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
        MPI_Reduce(&local.checksum,&total.checksum,1,MPI_UNSIGNED_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
        if(rank==0) { total.jobs=globalInts[0]; total.steps=globalInts[1]; for(int i=0;i<4;++i) total.plans[i]=globalInts[i+2]; total.score=globalDoubles[0]; total.water=globalDoubles[1]; }
        MPI_Gather(&local.jobs,1,MPI_LONG_LONG_INT,rankJobs.data(),1,MPI_LONG_LONG_INT,0,MPI_COMM_WORLD);
        MPI_Gather(&local.steps,1,MPI_LONG_LONG_INT,rankSteps.data(),1,MPI_LONG_LONG_INT,0,MPI_COMM_WORLD);
        MPI_Gather(&workSeconds,1,MPI_DOUBLE,rankSeconds.data(),1,MPI_DOUBLE,0,MPI_COMM_WORLD);
        MPI_Gather(threadJobs.data(),threads,MPI_LONG_LONG_INT,allThreadJobs.data(),threads,MPI_LONG_LONG_INT,0,MPI_COMM_WORLD);
        MPI_Gather(hostname,256,MPI_CHAR,hostnames.data(),256,MPI_CHAR,0,MPI_COMM_WORLD);
        // Dynamic ranks can finish different numbers of windows, so I gather variable-sized result arrays.
        std::vector<int> counts(ranks),offsets(ranks); const int resultCount=static_cast<int>(results.size());
        std::vector<Result> allResults;
        if(o.verify) {
            MPI_Gather(&resultCount,1,MPI_INT,counts.data(),1,MPI_INT,0,MPI_COMM_WORLD);
            if(rank==0) { int n=0; for(int i=0;i<ranks;++i) { offsets[i]=n; n+=counts[i]; } allResults.resize(n); }
            auto type=resultType();
            MPI_Gatherv(results.data(),resultCount,type,allResults.data(),counts.data(),offsets.data(),type,0,MPI_COMM_WORLD);
            MPI_Type_free(&type);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        // I use the longest rank time after the completion barrier; the reference check is timed separately.
        const double elapsed=omp_get_wtime()-start;
        double pipelineSeconds=0;
        MPI_Reduce(&elapsed,&pipelineSeconds,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
#else
        total=local; rankJobs[0]=local.jobs; rankSteps[0]=local.steps; rankSeconds[0]=workSeconds;
        allThreadJobs=threadJobs; std::copy(hostname,hostname+256,hostnames.begin());
        auto allResults=results;
        const double pipelineSeconds=omp_get_wtime()-start;
#endif
        int verified=1;
        double referenceSeconds=0;
        if(rank==0 && o.verify) {
            // I sort by ID so verification checks every window, regardless of which rank claimed it.
            std::sort(allResults.begin(),allResults.end(),[](const Result& a,const Result& b){ return a.id<b.id; });
            const double refStart=omp_get_wtime();
            if(allResults.size()!=static_cast<std::size_t>(o.jobs) || total.jobs!=o.jobs) verified=0;
            for(long long id=0;verified && id<o.jobs;++id) {
                // I compare against the same calculation run sequentially, including plan, scenarios, score and water.
                const auto expected=forecast(id,o,seeds);
                const auto& actual=allResults[static_cast<std::size_t>(id)];
                if(actual.id!=id || actual.plan!=expected.plan || actual.scenarios!=expected.scenarios ||
                   !close(actual.score,expected.score) || !close(actual.water,expected.water)) {
                    verified=0; std::cerr<<"Verification mismatch at job "<<id<<'\n';
                }
            }
            referenceSeconds=omp_get_wtime()-refStart;
        }
        // I print one summary on rank zero, with host and thread counts as evidence of participation.
        if(rank==0) {
            std::cout<<std::fixed<<std::setprecision(6)
                     <<"Mode: "<<o.mode<<"\nWorkload: "<<o.workload<<"\nInput: "<<(o.input.empty()?"deterministic synthetic sensor windows":o.input)
                     <<"\nWindows: "<<o.jobs<<"\nMPI ranks: "<<ranks<<"\nThreads per rank: "<<threads
                     <<"\nChunk size: "<<o.chunk<<"\nForecast horizon hours: "<<o.horizon
                     <<"\nForecast steps: "<<total.steps<<"\nPipeline seconds: "<<pipelineSeconds
                     <<"\nSequential reference seconds: "<<referenceSeconds
                     <<"\nVerification: "<<(o.verify?(verified?"PASSED (all windows)":"FAILED"):"NOT RUN (--no-verify)")
                     <<"\nResult checksum: "<<total.checksum<<"\nMean risk score: "<<total.score/o.jobs
                     <<"\nMean irrigation points: "<<total.water/o.jobs<<'\n';
            for(int i=0;i<4;++i) std::cout<<"Plan "<<i<<" windows: "<<total.plans[i]<<'\n';
            std::cout<<"Per-rank workload (work seconds include queue setup for dynamic mode):\n";
            for(int i=0;i<ranks;++i) {
                std::cout<<"Rank "<<i<<" host "<<(hostnames.data()+256*i)<<" windows "<<rankJobs[i]<<" steps "<<rankSteps[i]<<" work_seconds "<<rankSeconds[i]<<" thread_windows";
                for(int t=0;t<threads;++t) std::cout<<' '<<allThreadJobs[static_cast<std::size_t>(i)*threads+t];
                std::cout<<'\n';
            }
            if(o.verify) {
                std::cout<<"First five recommendations (plan, mean score, irrigation points):\n";
                for(std::size_t i=0;i<std::min<std::size_t>(5,allResults.size());++i) {
                    const auto& r=allResults[i]; std::cout<<"Window "<<r.id<<": "<<r.plan<<' '<<r.score<<' '<<r.water<<'\n';
                }
            }
#ifdef GREENHOUSE_LOCAL
            std::cout<<"Build: local CPU test; distributed MPI has not been tested by this build\n";
#endif
        }
#ifndef GREENHOUSE_LOCAL
        // I share the verification outcome so every rank returns a consistent success or failure status.
        MPI_Bcast(&verified,1,MPI_INT,0,MPI_COMM_WORLD); MPI_Finalize();
#endif
        return verified ? 0 : 2;
    // I report argument or runtime errors and abort the MPI group rather than leave other ranks waiting.
    } catch(const std::exception& e) {
        if(rank==0) std::cerr<<"Error: "<<e.what()<<'\n';
#ifndef GREENHOUSE_LOCAL
        MPI_Abort(MPI_COMM_WORLD,1);
#endif
        return 1;
    }
}
