#!/usr/bin/env python3
"""Use measured pipeline times, grouped by workload and size, for speedup."""
import csv
import re
import statistics
import sys
from pathlib import Path

def field(text, label):
    match = re.search(r'^' + re.escape(label) + r': (.+)$', text, re.MULTILINE)
    if not match:
        raise ValueError(f'Missing {label}')
    return match.group(1)

def summarise(directory):
    runs = []
    for path in sorted(directory.glob('*.txt')):
        if not re.match(r'^(uniform|skewed)_\d+_.+_\d+\.txt$', path.name):
            continue
        text = path.read_text()
        if field(text, 'Verification') != 'PASSED (all windows)':
            raise ValueError(f'Unverified result: {path.name}')
        seconds = float(field(text, 'Pipeline seconds'))
        if seconds <= 0:
            raise ValueError(f'Invalid timing: {path.name}')
        runs.append(dict(workload=field(text,'Workload'), windows=int(field(text,'Windows')),
            mode=field(text,'Mode'), ranks=int(field(text,'MPI ranks')),
            threads=int(field(text,'Threads per rank')), chunk=int(field(text,'Chunk size')),
            horizon=int(field(text,'Forecast horizon hours')), seconds=seconds,
            checksum=field(text,'Result checksum'), log=path.name))
    if not runs:
        raise ValueError('No verified benchmark logs found')
    with (directory/'runs.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(runs[0])); writer.writeheader(); writer.writerows(runs)
    groups={}
    for r in runs:
        key=(r['workload'],r['windows'],r['horizon'],r['mode'],r['ranks'],r['threads'],r['chunk'])
        groups.setdefault(key,[]).append(r)
    # All modes must be computing identical results for a given workload.
    for workload,size,horizon in {(r['workload'],r['windows'],r['horizon']) for r in runs}:
        values={r['checksum'] for r in runs if (r['workload'],r['windows'],r['horizon'])==(workload,size,horizon)}
        if len(values)!=1:
            raise ValueError(f'Checksums disagree for {workload}, {size}, horizon {horizon}')
    baseline={}
    for key,values in groups.items():
        if key[3]=='sequential':
            baseline[key[:3]]=statistics.median(v['seconds'] for v in values)
    summary=[]
    for key,values in sorted(groups.items()):
        median=statistics.median(v['seconds'] for v in values)
        if key[:3] not in baseline:
            raise ValueError('Matching sequential baseline missing')
        summary.append(dict(workload=key[0],windows=key[1],horizon=key[2],mode=key[3],ranks=key[4],
            threads=key[5],chunk=key[6],repeats=len(values),median_seconds=median,
            min_seconds=min(v['seconds'] for v in values),max_seconds=max(v['seconds'] for v in values),
            speedup=baseline[key[:3]]/median))
    with (directory/'summary.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(summary[0])); writer.writeheader(); writer.writerows(summary)
    for r in summary:
        print(f"{r['workload']:7} {r['windows']:7} {r['mode']:14} median {r['median_seconds']:.6f}s speedup {r['speedup']:.3f}x ({r['repeats']} runs)")
    print('Speedup uses a separate sequential pipeline median, not the in-run verification time.')

if __name__ == '__main__':
    if len(sys.argv)!=2:
        raise SystemExit('Usage: python3 tools/summarise.py results/bench_TIMESTAMP')
    summarise(Path(sys.argv[1]))
