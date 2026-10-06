#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results
make
mpi=(mpirun --bind-to none)
if [[ -n "${MPI_HOSTS:-}" ]]; then mpi+=(--host "$MPI_HOSTS"); fi
if [[ -n "${MPI_IFACE:-}" ]]; then mpi+=(--mca btl_tcp_if_include "$MPI_IFACE" --mca oob_tcp_if_include "$MPI_IFACE"); fi
export OMP_DYNAMIC=FALSE
./greenhouse --mode sequential --jobs 65 --workload skewed --input data/example_readings.csv > results/smoke_sequential.txt
for mode in mpi hybrid-static hybrid-dynamic; do
    "${mpi[@]}" -np "${MPI_RANKS:-2}" ./greenhouse --mode "$mode" --jobs 65 --threads 2 --chunk 8 --workload skewed --input data/example_readings.csv > "results/smoke_${mode}.txt"
done
python3 - <<'PY'
from pathlib import Path
import re
logs = sorted(Path('results').glob('smoke_*.txt'))
checksums = set()
for path in logs:
    text = path.read_text()
    assert 'Verification: PASSED (all windows)' in text, path
    checksums.add(re.search(r'Result checksum: (\d+)',text).group(1))
    print(path.name, 'PASSED')
assert len(logs) == 4 and len(checksums) == 1, 'Execution modes disagree'
print('All four modes agree, including the uneven final chunk.')
PY
