#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make
out="results/bench_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$out"
mpi=(mpirun --bind-to none)
if [[ -n "${MPI_HOSTS:-}" ]]; then mpi+=(--host "$MPI_HOSTS"); fi
if [[ -n "${MPI_IFACE:-}" ]]; then mpi+=(--mca btl_tcp_if_include "$MPI_IFACE" --mca oob_tcp_if_include "$MPI_IFACE"); fi
ranks=${MPI_RANKS:-2}
threads=${OMP_THREADS:-2}
repeats=${REPEATS:-3}
horizon=${HORIZON:-168}
read -ra sizes <<< "${SIZES:-2000 10000 50000}"
export OMP_DYNAMIC=FALSE
{
    date -Is
    uname -a
    lscpu
    free -h
    df -h .
    mpirun --version
    mpicxx --version
    printf 'Ranks=%s Threads=%s Repeats=%s Horizon=%s Sizes=%s Hosts=%s\n' "$ranks" "$threads" "$repeats" "$horizon" "${sizes[*]}" "${MPI_HOSTS:-local}"
} > "$out/system_master.txt"
"${mpi[@]}" -np "$ranks" hostname > "$out/rank_hosts.txt"
# One warm-up uses a small input and is excluded from the saved timings.
"${mpi[@]}" -np "$ranks" ./greenhouse --mode hybrid-dynamic --jobs 64 --threads "$threads" --horizon "$horizon" > "$out/warmup.txt"
for workload in uniform skewed; do
    for size in "${sizes[@]}"; do
        for ((repeat=1; repeat<=repeats; repeat++)); do
            # Rotate the mode order between repeats to reduce order bias.
            modes=(sequential mpi hybrid-static hybrid-dynamic)
            for ((j=0;j<4;j++)); do
                mode=${modes[$(((j+repeat-1)%4))]}
                log="$out/${workload}_${size}_${mode}_${repeat}.txt"
                args=(./greenhouse --mode "$mode" --jobs "$size" --threads "$threads" --horizon "$horizon" --chunk "${CHUNK:-32}" --workload "$workload")
                printf 'Testing %s, %s windows, %s, repeat %s\n' "$workload" "$size" "$mode" "$repeat"
                if [[ "$mode" == sequential ]]; then
                    timeout "${RUN_LIMIT:-180}s" "${args[@]}" > "$log" 2>&1
                else
                    timeout "${RUN_LIMIT:-180}s" "${mpi[@]}" -np "$ranks" "${args[@]}" > "$log" 2>&1
                fi
            done
        done
    done
done
python3 tools/summarise.py "$out"
printf 'Results saved in %s\n' "$out"
