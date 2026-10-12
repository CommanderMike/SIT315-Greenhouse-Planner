# Validation record

## Original experiment

The original submission's validation notes are preserved in `evidence/source_versions/VALIDATION_original_submission.md`. Its 72-run evidence remains in `results/bench_20261007_034644`. The original commented submission source is archived separately. Adding explanatory comments was the only source change for that earlier submission.

## Revised source

The revised source adds seeded random scenario counts, seed parsing and workload diagnostics. It retains the forecast equations, MPI/OpenMP scheduling method, result verification and timer boundaries. Before delivery, local CPU checks covered all four modes, each tested chunk and seed, original-mode numerical agreement and invalid arguments. Those development checks were not distributed performance evidence.

## Recorded two-VM follow-up

The completed user-run experiment is `results/followup_20261013_060245`. Its manifest specifies 126 measured runs, 30,000 windows, horizon 168, two ranks, two hybrid threads per rank, the explicit IPv4 subnet and the revised source and master executable hashes.

All 126 planned logs are present with exit status zero and full verification. Every parallel log identifies `sit315-master` and `sit315-worker`. Rank and thread window counts sum to 30,000. Forecast-step totals and result checksums agree within each workload and seed across every mode and chunk.

The summariser regenerated `summary.csv`, `runs.csv`, `rank_runs.csv` and `analysis.txt` byte-for-byte identically to the uploaded versions. A separate raw-log audit checked the 28 groups' medians, extrema, means, sample standard deviations, CV values and seed-specific speedups, and independently recreated the random cost counts from the 64-bit ID/seed mapping.

The initial interrupted attempt is not part of the completed dataset. Slow successful runs within the completed dataset were retained. The logs establish agreement with the program's sequential reference, not scientific validity of the chosen greenhouse model or a measured cause for every timing delay.

## Findings

Hybrid static had the lowest median for every random seed. Hybrid dynamic, chunk 512, had the lowest median for the clustered workload. Chunk 8 was slower than sequential in all four cases. The best tested dynamic chunk varied by seed; it is not a proven global optimum. Both VMs share one laptop, and the sample sizes are limited.
