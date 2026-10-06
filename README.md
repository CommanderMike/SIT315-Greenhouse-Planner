# Hybrid Greenhouse Scenario Planner

SIT315 M4.T1D project. This extends the idea of my earlier SIT111 smart-plant monitor with a C++ scenario-planning engine. It compares four irrigation policies and evaluates sequential, MPI, hybrid static and hybrid dynamic execution. The new concurrency method is an MPI RMA atomic chunk counter combined with OpenMP scheduling.

## Scope and model

A job represents one independent sensor window with soil moisture, temperature and humidity. Each of four watering policies is forecast across a set of deterministic uncertainty scenarios. The policy with the smallest combined dry-soil, wet-soil and water-cost score is selected.

The model is a teaching model with chosen coefficients, not a calibrated crop model. Irrigation units are moisture percentage points, not litres. The benchmark input and `data/example_readings.csv` are synthetic. There is no claim of live Arduino capture, actuator control or real greenhouse validation in this version. The old hardware project supplies the idea; this engine is a new implementation.

## Execution modes

| Mode | MPI ranks | Threads per rank | Work assignment |
|---|---:|---:|---|
| sequential | 1 | 1 | All windows on the master |
| mpi | 2 in recorded tests | 1 | Fixed contiguous ranges |
| hybrid-static | 2 in recorded tests | 2 | Fixed ranges with static OpenMP scheduling |
| hybrid-dynamic | 2 in recorded tests | 2 | MPI atomic chunk claims with dynamic OpenMP scheduling |

The dynamic counter is exposed by rank zero, but rank zero also calculates windows. All MPI calls run on the main thread using `MPI_THREAD_FUNNELED`. Full verification gathers and sorts results by ID and compares every policy, scenario count, score and water value with a sequential reference. A separate checksum allows comparison across saved runs.

## Build

Both VMs need Open MPI, g++, make and the project at the same path. The recorded path is `/home/micky/M4T1D_Greenhouse`. Build on both VMs:

```bash
cd ~/M4T1D_Greenhouse
make
```

The Makefile uses:

```bash
mpicxx -O2 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic src/greenhouse.cpp -o greenhouse
```

## Recorded two-VM configuration

Run the following on the master in each new terminal:

```bash
cd ~/M4T1D_Greenhouse
export MPI_HOSTS=192.168.20.7:1,192.168.20.8:1
export MPI_RANKS=2 MPI_IFACE=enp0s3 OMP_THREADS=2
export OMPI_MCA_osc=pt2pt
```

The worker must be running and reachable by SSH. These addresses and interface are specific to the recorded setup. Confirm or change them when reproducing the project elsewhere. Open MPI 4.1.6 was recorded on the master. The `pt2pt` OSC backend was required for the cross-VM RMA test; it is a version-specific runtime setting, not a guarantee that the same component exists in later Open MPI versions.

Check both hosts, including the network flags:

```bash
timeout --kill-after=5s 30s mpirun --host "$MPI_HOSTS" --bind-to none -np 2 --mca btl_tcp_if_include "$MPI_IFACE" --mca oob_tcp_if_include "$MPI_IFACE" hostname
```

Run the correctness checks:

```bash
timeout --kill-after=5s 45s bash scripts/smoke_test.sh
```

For a direct demonstration:

```bash
mpirun --host "$MPI_HOSTS" --bind-to none -np 2 --mca osc pt2pt --mca btl_tcp_if_include "$MPI_IFACE" --mca oob_tcp_if_include "$MPI_IFACE" ./greenhouse --mode hybrid-dynamic --jobs 65 --threads 2 --chunk 8 --workload skewed --input data/example_readings.csv
```

## Actual evaluation evidence

Recorded results are in `results/bench_20261007_034644`. The experiment used 2,000, 10,000 and 30,000 windows, a 168-hour horizon, two workload types, four modes and three repeats: 72 measured runs. Every run passed full verification. Mode order rotated between repeats. One small warm-up was excluded.

The uneven workload gives the first quarter of windows 64 scenarios instead of 8, making equal window counts an unequal division of forecast work. The largest uniform case favoured hybrid static (median 0.679406 s; 4.403787x measured speedup). The largest uneven case favoured hybrid dynamic (median 4.610541 s; 1.850986x measured speedup). Observed ranges are included in `summary.csv`; timings are not perfectly stable.

To reproduce the measured experiment with the configuration above:

```bash
SIZES="2000 10000 30000" REPEATS=3 HORIZON=168 RUN_LIMIT=90 timeout --kill-after=10s 20m bash scripts/benchmark.sh
```

`Pipeline seconds` includes calculation, scheduling/window setup, reductions, rank diagnostics, result gathering and completion synchronization. It excludes CSV loading/broadcast, MPI startup, printing, result sorting and sequential reference verification. The elapsed maximum across ranks is used, measured before the final timing reduction. Speedup uses the separate sequential pipeline median, not the in-run reference timer.

Chunk size was fixed at 32 for the measured comparisons. It has not been proven optimal. Further chunk tuning and runs on independent physical machines are future work.

## Required stress case

```bash
STRESS_LIMIT=120 bash scripts/stress.sh
```

The recorded stress log is `results/stress_20261007_040317.txt`. It requested one billion windows, horizon 720, chunk 128 and two threads on each of two ranks. The program printed its startup message and the command ended with exit status 124 after 120 seconds. The outcome was DNF under a configured time budget, not a completed billion-window calculation or a demonstrated out-of-memory event. Full result verification was disabled; results were not retained for every stress window.

## Machines used

The physical host was Windows 11 Pro (10.0.26200), Intel Core i5-8265U at a reported 1.60 GHz, 4 physical cores, 8 logical processors and approximately 7.8 GiB RAM reported by the supplied PowerShell query. Both Ubuntu 24.04.4 LTS VMs had 2 exposed vCPUs and about 1.9 GiB guest RAM, with no guest swap. The VMs shared the same physical laptop and do not represent independent physical servers. The master used Open MPI 4.1.6 and g++ 13.3.0.

## Files

- `src/greenhouse.cpp`: the complete C++ implementation.
- `Makefile`: MPI/OpenMP build and optional local CPU build.
- `scripts/`: smoke tests, benchmarks and bounded stress runner.
- `tools/summarise.py`: reads saved logs and calculates medians/ranges/speedup; it does not run the simulation.
- `data/example_readings.csv`: synthetic seed readings.
- `results/`: unedited recorded logs and CSV summaries.
- `evidence/`: supplied screenshots and runtime notes.
- `docs/`: the project report and benchmark chart.

`make local` is a development fallback without MPI. It cannot provide cross-VM or RMA evidence. Use `make` and `greenhouse` for the actual demonstration.

## Research

- MPI Forum, atomic operations: https://www.mpi-forum.org/docs/mpi-4.1/mpi41-report/node320.htm
- MPI Forum, RMA synchronization examples: https://www.mpi-forum.org/docs/mpi-5.0/mpi50-report/node344.htm
- Open MPI, thread support: https://www.open-mpi.org/doc/v4.1/man3/MPI_Init_thread.3.php
- OpenMP, loop scheduling: https://www.openmp.org/spec-html/5.1/openmpsu48.html
- Open MPI, runtime parameters: https://docs.open-mpi.org/en/main/mca.html


## Submission files

OnTrack's project-code slot takes `src/greenhouse.cpp` as a C++ source file. The entire application is in that source file. The Makefile and scripts automate building and testing; they do not contain additional application code. Upload the report PDF to the report slot.

The report includes this repository’s URL. Supporting source, scripts and recorded evidence are available here.

To build a downloaded standalone copy named `greenhouse.cpp`:

```bash
mpicxx -O2 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic greenhouse.cpp -o greenhouse
```

The built-in generated inputs work without a CSV. To reproduce the CSV test, benchmarks and saved evaluation, use the complete repository and its README instructions.

## Reproducing the report chart

The chart is drawn with Python Matplotlib from the saved C++ benchmark summary. It uses ordinary linear-axis grouped bars, with minimum-to-maximum whiskers. Bar labels show the median. The axes use different time ranges for uniform and uneven work; compare modes within the same panel.

With Matplotlib installed, run:

```bash
python3 tools/plot_results.py results/bench_20261007_034644/summary.csv
```

The output is `docs/figures/pipeline_times.png`. No simulator runs are repeated. This Python tool is only for graphing the recorded results.

## Commented submission source

First-person comments were expanded after the benchmark to explain input validation, forecast scoring, thread-private statistics, MPI data layout, atomic chunk claims, timers and verification. Every non-comment source line matches the original benchmarked source. Both source hashes are recorded in `evidence/runtime_notes.md`; the saved measurement logs remain unchanged.
