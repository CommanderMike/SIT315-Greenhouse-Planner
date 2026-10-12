#!/usr/bin/env python3
"""I run the compiled C++ application and retain every measured log."""
import datetime, hashlib, json, os, random, shlex, subprocess, sys, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
def integers(name,default):
    values=[int(x) for x in os.environ.get(name,default).split()]
    if not values or any(x<1 for x in values):raise ValueError(name+' must contain positive integers')
    return values
def run():
    sizes=integers('SIZES','30000'); seeds=integers('SEEDS','315 316 317')
    chunks=integers('CHUNKS','8 32 128 512')
    repeats=int(os.environ.get('REPEATS','5')); clustered=int(os.environ.get('CLUSTERED_REPEATS','3'))
    horizon=int(os.environ.get('HORIZON','168'));limit=int(os.environ.get('RUN_LIMIT','90'))
    ranks=int(os.environ.get('MPI_RANKS','2'));threads=int(os.environ.get('OMP_THREADS','2'))
    if min(repeats,clustered,horizon,limit,ranks,threads)<1:raise ValueError('Settings must be positive')
    out=Path(os.environ.get('RESULT_DIR','results/followup_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')))
    out.mkdir(parents=True,exist_ok=True)
    source=hashlib.sha256((ROOT/'src/greenhouse.cpp').read_bytes()).hexdigest()
    binary=hashlib.sha256((ROOT/'greenhouse').read_bytes()).hexdigest()
    configs=[('sequential',32),('mpi',32),('hybrid-static',32)]+[('hybrid-dynamic',c) for c in chunks]
    cases=[(size,'random',seed,repeats) for size in sizes for seed in seeds]
    cases += [(size,'skewed',seeds[0],clustered) for size in sizes]
    order=random.Random(3152026);plan=[]
    for rep in range(1,max(repeats,clustered)+1):
        block=cases.copy();order.shuffle(block)
        for size,workload,seed,n in block:
            if rep>n:continue
            choices=configs.copy();order.shuffle(choices)
            for mode,chunk in choices:
                name=f'{workload}_n{size}_s{seed}_{mode}_c{chunk}_r{rep}.txt'
                plan.append(dict(size=size,workload=workload,seed=seed,mode=mode,chunk=chunk,repeat=rep,log=name))
    manifest=dict(source_sha256=source,binary_sha256=binary,horizon=horizon,ranks=ranks,threads=threads,
                  hosts=os.environ.get('MPI_HOSTS','192.168.20.7:1,192.168.20.8:1'),
                  iface=os.environ.get('MPI_IFACE','enp0s3'),run_limit=limit,plan=plan)
    mf=out/'manifest.json'
    if mf.exists() and json.loads(mf.read_text())!=manifest:
        raise ValueError('This result folder belongs to a different build or experiment. Use a new RESULT_DIR.')
    mf.write_text(json.dumps(manifest,indent=2)+'\n')
    mpi=['mpirun','--host',manifest['hosts'],'--bind-to','none','-np',str(ranks),
         '--mca','osc','pt2pt','--mca','btl_tcp_if_include',manifest['iface'],
         '--mca','oob_tcp_if_include',manifest['iface']]
    print(f'Results: {out}\nPlanned measured runs: {len(plan)}',flush=True)
    if not (out/'system_master.txt').exists():
        with (out/'system_master.txt').open('w') as f:
            f.write(datetime.datetime.now().isoformat()+'\n');f.flush()
            for cmd in [['uname','-a'],['lscpu'],['free','-h'],['df','-h','.'],['mpirun','--version'],['mpicxx','--version']]:
                subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
        # I save guest load and memory information; these snapshots cannot explain every timing fluctuation.
        with (out/'system_worker.txt').open('w') as f:
            subprocess.run(['ssh','-x','-o','ConnectTimeout=10','-o','ServerAliveInterval=15','-o','ServerAliveCountMax=2',
                os.environ.get('MPI_WORKER','micky@192.168.20.8'),
                'uname -a\nlscpu\nfree -h\nuptime\nsha256sum ~/M4T1D_Followup/src/greenhouse.cpp'],stdout=f,stderr=subprocess.STDOUT,check=True)
        with (out/'rank_hosts.txt').open('w') as f:
            subprocess.run(['timeout','--kill-after=5s','30s']+mpi+['hostname'],stdout=f,stderr=subprocess.STDOUT,check=True)
        for mode,chunk in configs:
            args=['./greenhouse','--mode',mode,'--jobs','128','--threads',str(threads),'--horizon',str(horizon),
                  '--chunk',str(chunk),'--workload','random','--seed',str(seeds[0])]
            command=args if mode=='sequential' else mpi+args
            with (out/f'warmup_{mode}_c{chunk}.txt').open('w') as f:
                subprocess.run(['timeout','--kill-after=5s',f'{limit}s']+command,stdout=f,stderr=subprocess.STDOUT,check=True)
    for index,item in enumerate(plan,1):
        log=out/item['log']
        if log.exists() and 'Run exit status: 0\n' in log.read_text() and 'Verification: PASSED (all windows)' in log.read_text():
            print(f'[{index}/{len(plan)}] retained verified run {log.name}',flush=True);continue
        args=['./greenhouse','--mode',item['mode'],'--jobs',str(item['size']),'--threads',str(threads),
              '--horizon',str(horizon),'--chunk',str(item['chunk']),'--workload',item['workload'],'--seed',str(item['seed'])]
        command=args if item['mode']=='sequential' else mpi+args
        print(f'[{index}/{len(plan)}] {item["workload"]}, seed {item["seed"]}, {item["mode"]}, chunk {item["chunk"]}, repeat {item["repeat"]}',flush=True)
        with log.open('w') as f:
            f.write('Command: '+shlex.join(command)+'\n');f.flush();start=time.monotonic()
            result=subprocess.run(['timeout','--kill-after=5s',f'{limit}s']+command,stdout=f,stderr=subprocess.STDOUT)
            f.write(f'\nRun exit status: {result.returncode}\nCommand wall seconds: {time.monotonic()-start:.6f}\n')
        if result.returncode or 'Verification: PASSED (all windows)' not in log.read_text():
            raise RuntimeError(f'Run stopped; inspect {log}. Partial logs are retained. Do not report this as a completed study.')
    with (out/'system_after.txt').open('w') as f:
        for cmd in [['free','-h'],['uptime']]:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/summarise_followup.py'),str(out)],check=True)
    print(f'Finished. Send the complete {out} folder, especially analysis.txt, summary.csv and runs.csv.',flush=True)

if __name__=='__main__':
    try:run()
    except Exception as exc:raise SystemExit(str(exc))
