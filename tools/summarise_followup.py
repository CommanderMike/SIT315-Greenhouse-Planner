#!/usr/bin/env python3
"""I keep workload seeds separate and report spread without discarding slow runs."""
import csv,json,math,re,statistics,sys
from pathlib import Path

def field(text,label):
    m=re.search(r'^'+re.escape(label)+r': (.+)$',text,re.M)
    if not m:raise ValueError('Missing '+label)
    return m.group(1)
def write_csv(path,rows):
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def summarise(directory):
    manifest=json.loads((directory/'manifest.json').read_text());runs=[];rank_rows=[]
    for item in manifest['plan']:
        path=directory/item['log'];text=path.read_text()
        if field(text,'Run exit status')!='0' or field(text,'Verification')!='PASSED (all windows)':
            raise ValueError('Failed or unverified run: '+path.name)
        mode=field(text,'Mode');workload=field(text,'Workload');windows=int(field(text,'Windows'))
        seed=int(field(text,'Workload seed'));chunk=int(field(text,'Chunk size'))
        horizon=int(field(text,'Forecast horizon hours'));steps=int(field(text,'Forecast steps'))
        expensive=int(field(text,'Expensive windows'));seconds=float(field(text,'Pipeline seconds'))
        ranks=int(field(text,'MPI ranks'));threads=int(field(text,'Threads per rank'))
        expected_ranks=1 if mode=='sequential' else manifest['ranks']
        expected_threads=manifest['threads'] if mode.startswith('hybrid') else 1
        if (mode,workload,windows,seed,chunk)!=(item['mode'],item['workload'],item['size'],item['seed'],item['chunk']):
            raise ValueError('Log does not match experiment plan: '+path.name)
        if horizon!=manifest['horizon'] or ranks!=expected_ranks or threads!=expected_threads:
            raise ValueError('Configuration mismatch: '+path.name)
        if steps!=(8*windows+56*expensive)*4*horizon or not math.isfinite(seconds) or seconds<=0:
            raise ValueError('Invalid work count or timing: '+path.name)
        rows=[]
        for m in re.finditer(r'^Rank (\d+) host (\S+) windows (\d+) steps (\d+) work_seconds ([\d.]+) thread_windows ([\d ]+)$',text,re.M):
            row=dict(log=path.name,rank=int(m[1]),host=m[2],windows=int(m[3]),steps=int(m[4]),work_seconds=float(m[5]),thread_windows=m[6].strip())
            if sum(map(int,row['thread_windows'].split()))!=row['windows']:raise ValueError('Thread counts disagree')
            rows.append(row)
        if len(rows)!=ranks or len({x['rank'] for x in rows})!=ranks or sum(x['windows'] for x in rows)!=windows or sum(x['steps'] for x in rows)!=steps:
            raise ValueError('Rank counts disagree: '+path.name)
        counts=[x['steps'] for x in rows];balance=max(counts)/min(counts) if min(counts)>0 else math.inf
        times=[x['work_seconds'] for x in rows];time_balance=max(times)/min(times) if min(times)>0 else math.inf
        runs.append(dict(workload=workload,windows=windows,seed=seed,mode=mode,chunk=chunk,repeat=item['repeat'],horizon=horizon,
            ranks=ranks,threads=threads,expensive_windows=expensive,forecast_steps=steps,pipeline_seconds=seconds,
            command_wall_seconds=float(field(text,'Command wall seconds')),checksum=field(text,'Result checksum'),
            rank_step_ratio=balance,rank_work_time_ratio=time_balance,log=path.name))
        rank_rows.extend(rows)
    groups={};cohorts={}
    for r in runs:
        cohort=(r['workload'],r['windows'],r['seed'],r['horizon']);key=cohort+(r['mode'],r['chunk'])
        groups.setdefault(key,[]).append(r);cohorts.setdefault(cohort,[]).append(r)
    for cohort,values in cohorts.items():
        if len({(v['checksum'],v['forecast_steps'],v['expensive_windows']) for v in values})!=1:
            raise ValueError('Modes or chunks compute different work: '+str(cohort))
    baselines={key[:4]:statistics.median(v['pipeline_seconds'] for v in values) for key,values in groups.items() if key[4]=='sequential'}
    summary=[]
    for key,values in sorted(groups.items()):
        times=[v['pipeline_seconds'] for v in values];median=statistics.median(times);mean=statistics.mean(times)
        sd=statistics.stdev(times) if len(times)>1 else 0
        quartiles=statistics.quantiles(times,n=4,method='inclusive') if len(times)>1 else [median]*3
        summary.append(dict(workload=key[0],windows=key[1],seed=key[2],horizon=key[3],mode=key[4],chunk=key[5],repeats=len(times),
            expensive_windows=values[0]['expensive_windows'],forecast_steps=values[0]['forecast_steps'],median_seconds=median,
            min_seconds=min(times),max_seconds=max(times),mean_seconds=mean,sample_sd_seconds=sd,cv_percent=100*sd/mean,
            q1_seconds=quartiles[0],q3_seconds=quartiles[2],iqr_seconds=quartiles[2]-quartiles[0],speedup=baselines[key[:4]]/median,
            median_rank_step_ratio=statistics.median(v['rank_step_ratio'] for v in values),
            median_rank_work_time_ratio=statistics.median(v['rank_work_time_ratio'] for v in values)))
    write_csv(directory/'runs.csv',runs);write_csv(directory/'rank_runs.csv',rank_rows);write_csv(directory/'summary.csv',summary)
    lines=[f'Completed measured runs: {len(runs)} / {len(manifest["plan"])}',
           'Every measured run passed full verification. Checksums and forecast-step totals match within each seed and workload.',
           'Speedup uses the separate sequential median for the same workload, size and seed.',
           'Warm-ups are excluded. No slow measured run is removed.',
           'CV is sample standard deviation divided by mean. Five repeats describe observed spread, not a precise confidence interval.','']
    for cohort,values in sorted(cohorts.items()):
        rows=[r for r in summary if (r['workload'],r['windows'],r['seed'],r['horizon'])==cohort]
        lines.append(f'{cohort[0]}: {cohort[1]} windows, seed {cohort[2]}, expensive windows {values[0]["expensive_windows"]}')
        for r in rows:
            lines.append(f'{r["mode"]:15} chunk {r["chunk"]:4}: median {r["median_seconds"]:.6f}s, range {r["min_seconds"]:.6f}-{r["max_seconds"]:.6f}s, CV {r["cv_percent"]:.1f}%, speedup {r["speedup"]:.3f}x, rank step ratio {r["median_rank_step_ratio"]:.3f}')
        fastest=min(rows,key=lambda r:r['median_seconds']);dyn=min((r for r in rows if r['mode']=='hybrid-dynamic'),key=lambda r:r['median_seconds'])
        lines.append(f'Fastest observed: {fastest["mode"]}, chunk {fastest["chunk"]}. Lowest dynamic median among tested chunks: {dyn["chunk"]}.')
        lines.append('')
    lines.extend(['Interpretation checks:',
        'Compare the clustered fixed-split rank step ratio with the random fixed-split ratio before attributing a timing difference to load imbalance.',
        'Changing the seed changes which jobs are expensive and may change their total count. Keep seed-specific timing comparisons separate.',
        'A smaller chunk makes more atomic claims and OpenMP team entries; a larger chunk may leave less work to redistribute.',
        'Both VMs share one laptop. System scheduling, memory/cache effects and MPI/SSH activity are possible timing influences; these tests do not isolate their causes.',
        'Rank work elapsed includes setup and queue delays. It is not a direct measure of CPU utilisation.',
        'The best tested chunk is not a proven global optimum. Do not generalise the fastest seed or repeat to every workload.'])
    report='\n'.join(lines)+'\n';(directory/'analysis.txt').write_text(report);print(report)

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('Usage: python3 tools/summarise_followup.py results/followup_TIMESTAMP')
    summarise(Path(sys.argv[1]))
