#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export OMPI_MCA_osc=pt2pt OMP_DYNAMIC=FALSE
hosts=${MPI_HOSTS:-192.168.20.7:1,192.168.20.8:1}
iface=${MPI_IFACE:-enp0s3}
ranks=${MPI_RANKS:-2}
mkdir -p results/smoke_followup
mpi=(mpirun --host "$hosts" --bind-to none -np "$ranks" --mca osc pt2pt --mca btl_tcp_if_include "$iface" --mca oob_tcp_if_include "$iface")
timeout --kill-after=5s 30s "${mpi[@]}" hostname
./greenhouse --mode sequential --jobs 65 --workload random --seed 315 > results/smoke_followup/sequential.txt
for mode in mpi hybrid-static hybrid-dynamic; do
    timeout --kill-after=5s 45s "${mpi[@]}" ./greenhouse --mode "$mode" --jobs 65 --threads 2 --chunk 8 --workload random --seed 315 > "results/smoke_followup/${mode}.txt" 2>&1
done
python3 - <<'PY'
from pathlib import Path
import re
checksums=set()
for name in ['sequential','mpi','hybrid-static','hybrid-dynamic']:
    text=(Path('results/smoke_followup')/(name+'.txt')).read_text()
    if 'Verification: PASSED (all windows)' not in text:
        raise SystemExit('Verification failed: '+name)
    checksums.add(re.search(r'^Result checksum: (\d+)$',text,re.M).group(1))
    print(name, 'PASSED')
if len(checksums)!=1: raise SystemExit('Checksums disagree')
print('All four modes agree on the same seeded random workload.')
PY
