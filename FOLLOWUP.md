# Follow-up after feedback

I added this evaluation after feedback that my original clustered workload favoured dynamic scheduling. The original 72-run evidence remains in `results/bench_20261007_034644`. This follow-up uses the revised source and is saved separately in `results/followup_20261013_060245`.

## What changed

I added `--workload random` and `--seed`. A deterministic mix of the window ID and seed assigns 64 scenarios to approximately one quarter of the windows and 8 to the rest. Every mode uses the same assignment for a given seed. The sensor readings and forecast equations are unchanged. This is random distribution of job costs, not live sensor input or a calibrated greenhouse model.

The expensive windows were spread across all quarters of the input:

| Seed | IDs 0-7499 | IDs 7500-14999 | IDs 15000-22499 | IDs 22500-29999 | Total expensive windows |
|---|---:|---:|---:|---:|---:|
| 315 | 1852 | 1811 | 1895 | 1909 | 7467 |
| 316 | 1894 | 1946 | 1893 | 1874 | 7607 |
| 317 | 1873 | 1844 | 1839 | 1858 | 7414 |

These counts can be reproduced from `scenarioCount` and `mix` in the source. The seed changes which windows are expensive and their total count, so each seed has its own sequential baseline.

## Experiment

I used 30,000 windows, a 168-hour horizon, two MPI ranks on the two VMs and two threads per hybrid rank. MPI-only has one thread per rank; sequential has one rank and one thread. The seven configurations were sequential, MPI-only, hybrid static and hybrid dynamic with MPI chunks of 8, 32, 128 and 512. The OpenMP dynamic chunk inside each MPI batch remains one window.

Three random seeds with five repeats of seven configurations gave 105 runs. Three repeats of the seven configurations on the original clustered workload gave 21 more: 126 measured runs in total. Seven small configuration warm-ups were excluded. The runner shuffled cases and configurations with a fixed ordering seed. Every completed measured run is retained, including the slowest MPI run. Each run passed full verification, and every parallel log names both hosts.

An earlier interrupted attempt stopped at a launch timeout. After restarting the master, I ran the full study in a fresh folder. Its three completed measurements are not combined with this dataset.

The timer and build flags are unchanged. Pipeline time includes calculation, scheduling setup, diagnostics, reductions, gathering and the completion barrier. It excludes MPI launch, input setup, result sorting and the sequential verification calculation. `Command wall seconds` is a separate end-to-end measure and is not used as the speedup baseline.

## Results

Median pipeline seconds. Random cases have five repeats each; the clustered case has three.

| Configuration | Random 315 | Random 316 | Random 317 | Clustered |
|---|---:|---:|---:|---:|
| sequential | 5.479 | 5.166 | 5.707 | 6.252 |
| mpi | 3.536 | 3.234 | 3.906 | 5.945 |
| hybrid-static | 2.346 | 2.247 | 2.295 | 4.459 |
| hybrid-dynamic, chunk 8 | 7.950 | 6.342 | 6.593 | 7.820 |
| hybrid-dynamic, chunk 32 | 3.741 | 4.774 | 4.379 | 4.192 |
| hybrid-dynamic, chunk 128 | 3.003 | 3.449 | 4.101 | 3.342 |
| hybrid-dynamic, chunk 512 | 3.195 | 3.236 | 3.384 | 3.112 |

Hybrid static was fastest for all three random seeds, with speedups of 2.34x, 2.30x and 2.49x over their matching sequential medians. The fixed rank split was already close to balanced: the forecast-step ratios were 1.024, 1.012 and 1.003, where 1 means equal work. There was therefore less imbalance for the shared counter to correct. In contrast, the clustered workload still gave the fixed ranks a 4.5-to-1 split in forecast steps. Hybrid dynamic with chunk 512 was fastest there, taking 3.112 seconds compared with 4.459 seconds for hybrid static. Its median time was about 30.2% lower, with a 2.01x speedup over sequential execution.

The chunk sweep also changed my interpretation. Chunk 8 was slower than sequential execution in every tested workload. The lowest dynamic median used chunk 128 for random seed 315 and chunk 512 for seeds 316 and 317 and the clustered case. Larger batches reduce counter requests and entries into the OpenMP work loop, which is consistent with the improvement over chunk 8. I did not time those costs separately, so I cannot say exactly how much each one contributed. These results identify the best of the four tested chunks for each case, not a generally optimal chunk size.

### Variation in the follow-up timings

I reported the minimum, maximum, sample standard deviation, interquartile range and coefficient of variation as well as the median in summary.csv. The coefficient of variation is the sample standard deviation divided by the mean, expressed as a percentage. The examples below show why I would not judge a mode from one run.

| Case | Configuration | Repeats | Median (s) | Minimum to maximum (s) | CV |
|---|---|---:|---:|---:|---:|
| Random seed 315 | Dynamic, chunk 8 | 5 | 7.950 | 5.366 to 13.877 | 36.5% |
| Random seed 317 | MPI-only | 5 | 3.906 | 3.629 to 15.875 | 82.2% |
| Random seed 317 | Hybrid static | 5 | 2.295 | 2.180 to 2.759 | 10.4% |
| Clustered | Dynamic, chunk 512 | 3 | 3.112 | 2.831 to 3.361 | 8.5% |

The MPI-only run for seed 317 reached 15.875 seconds even though its forecast work was split almost equally between ranks. That run still passed verification, and I kept it in the results. This shows that unequal forecast-step counts cannot explain all of the timing variation. Both VMs share one laptop, so operating-system scheduling, competition for CPU and memory/cache resources, and MPI communication delays are possible influences. The saved rank timings and system snapshots help describe the conditions, but I did not collect a trace that proves the cause of that particular delay. Smaller dynamic chunks also add more scheduling operations, which may make their times more sensitive to overhead.

Dynamic claiming did not make the forecast-step totals equal between ranks. For example, with random seed 315 and chunk 512, the run with the median pipeline time assigned about 322.963 million steps to the master and 119.315 million to the worker. Their recorded work elapsed times were about 3.160 and 3.167 seconds. Those times include queue setup and waiting, so similar elapsed times do not prove equal CPU use. I therefore treat the counter as a way to adapt work assignment, not as a guarantee of equal step counts or better performance. Five random repeats and three clustered repeats describe the observed spread, but do not provide a precise confidence interval or establish performance on independent physical servers.

The full 28-row summary also reports means, sample standard deviations, inclusive quartiles, IQR, CV, speedups and median rank ratios. Raw measurements are in `runs.csv`; rank observations are in `rank_runs.csv`; the original per-run logs are alongside them. The summary can be regenerated without running the simulator.

## Reproduce

The recorded directory on both VMs was `/home/micky/M4T1D_Followup`. Both VMs need the same source, Makefile and built executable at that path. Run on the master:

```bash
cd ~/M4T1D_Followup
make
export MPI_HOSTS=192.168.20.7:1,192.168.20.8:1
export MPI_RANKS=2 OMP_THREADS=2
export MPI_IFACE=192.168.20.0/24
export OMPI_MCA_osc=pt2pt
bash scripts/smoke_followup.sh
bash scripts/run_followup.sh
```

The default runner settings reproduce the 126-run experiment. It creates a new timestamped folder. For other machines, change the addresses, subnet and worker account and ensure the remote directory and executable match. `scripts/prepare_two_vm.sh` builds and copies the source for this recorded directory.

To regenerate the saved summaries:

```bash
python3 tools/summarise_followup.py results/followup_20261013_060245
```

Python launches the C++ executable and reads its logs; it does not calculate the greenhouse forecasts. The existing Python Matplotlib chart describes the original 72-run experiment. The table here describes the new follow-up; the two datasets are not combined.

## Source versions

`src/greenhouse.cpp` is the revised application, with source SHA-256 `ada0277139c321a6c52e2f391aa35c586e1aa039b4c137b4ca288328592bb7e9`. It matches both the follow-up manifest and the worker's source hash. The master executable hash is also in the manifest; a worker executable hash was not captured.

The original commented submission is retained as `evidence/source_versions/greenhouse_original_submission.cpp`, SHA-256 `48b43610c2f7e1d4611791542ba9bc447492405891387f82d19837df18ef047f`. Its pre-comment benchmark source hash is documented in the original runtime notes. The new random mapping, option parsing and diagnostic output are functional additions, not just comment changes. The forecast, scheduling method, verification and timer boundaries were left intact.

The original demonstration is https://youtu.be/DV-pvnEoMfI. It predates this follow-up and explains the project and the original experiment.
