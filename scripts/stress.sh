#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make
mkdir -p results
mpi=(mpirun --bind-to none)
if [[ -n "${MPI_HOSTS:-}" ]]; then mpi+=(--host "$MPI_HOSTS"); fi
if [[ -n "${MPI_IFACE:-}" ]]; then mpi+=(--mca btl_tcp_if_include "$MPI_IFACE" --mca oob_tcp_if_include "$MPI_IFACE"); fi
limit=${STRESS_LIMIT:-120}
log="results/stress_$(date +%Y%m%d_%H%M%S).txt"
printf 'Stress limit: %s seconds\nStarted: %s\n' "$limit" "$(date -Is)" > "$log"
printf 'Command: timeout --signal=TERM --kill-after=10s %ss ' "$limit" >> "$log"
printf '%q ' "${mpi[@]}" -np "${MPI_RANKS:-2}" ./greenhouse --mode hybrid-dynamic --jobs 1000000000 --threads "${OMP_THREADS:-2}" --horizon 720 --workload skewed --chunk 128 --no-verify >> "$log"
printf '\n' >> "$log"
start=$SECONDS
set +e
timeout --signal=TERM --kill-after=10s "${limit}s" "${mpi[@]}" -np "${MPI_RANKS:-2}" ./greenhouse --mode hybrid-dynamic --jobs 1000000000 --threads "${OMP_THREADS:-2}" --horizon 720 --workload skewed --chunk 128 --no-verify >> "$log" 2>&1
status=$?
set -e
printf '\nExit status: %s\nActual elapsed seconds: %s\n' "$status" "$((SECONDS-start))" >> "$log"
if [[ "$status" == 124 || "$status" == 137 ]]; then
    printf 'Outcome: DNF, stopped by configured timeout. Inspect earlier output to confirm computation started.\n' >> "$log"
elif [[ "$status" == 0 ]]; then
    printf 'Outcome: completed; this is not a DNF stress case.\n' >> "$log"
else
    printf 'Outcome: runtime error; investigate before claiming a valid stress result.\n' >> "$log"
fi
cat "$log"
