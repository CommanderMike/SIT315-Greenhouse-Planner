# Original runtime notes

The working cross-VM tests and benchmark were launched after exporting `OMPI_MCA_osc=pt2pt` on the master. This shell setting is not printed in the original benchmark or stress command logs. Reproduction commands in the updated README explicitly include it.

Master: 192.168.20.7; worker: 192.168.20.8; interface: enp0s3.
Host CPU: Intel Core i5-8265U, 4 physical cores and 8 logical processors.
Host RAM: 7.8 GiB reported by PowerShell; OS: Windows 11 Pro 10.0.26200.
Both guests: Ubuntu 24.04.4 LTS, 2 vCPUs, about 1.9 GiB RAM, zero guest swap.

SHA-256 of the original benchmarked C++ source: `497444ef2d5f4aa63ebf5cd14212cba4ea63ed637f14ded54161e4bfd62e899c`.
SHA-256 of the commented submission source: `48b43610c2f7e1d4611791542ba9bc447492405891387f82d19837df18ef047f`.

First-person explanatory comments were added after the benchmark. Every non-comment source line is identical to the original. The algorithms, constants, compiler options and execution scripts have not changed. The original benchmark and stress logs are preserved. The supplied code listing shows the commented submission version.


# Follow-up runtime notes: 13 October 2026

The revised source was built on both VMs in `/home/micky/M4T1D_Followup`. The working TCP selection was `192.168.20.0/24` for both `btl_tcp_if_include` and `oob_tcp_if_include`, with the same host addresses and the `pt2pt` OSC backend. The runtime manifest records these settings.

The original comments-only source equivalence described above applies to the earlier submission. The current source adds functional support for seeded random workloads and extra diagnostics. The completed follow-up records source SHA-256 `ada0277139c321a6c52e2f391aa35c586e1aa039b4c137b4ca288328592bb7e9`, confirmed on the worker, and master binary SHA-256 `6e504a148abd1f86431bdeea4483d09476cd0b44a9b2d81a8000e65eb2663408`. A worker binary hash was not recorded.

The first attempted study stopped on its fourth measured launch with exit status 124 after about 92.28 command-wall seconds, without printing the application's startup message. The master desktop became unresponsive and was powered off. The root cause was not established. After restarting it, a fresh study completed all 126 planned measured runs in `results/followup_20261013_060245`. The interrupted attempt's three completed measurements were not mixed into the finished dataset.

Seven configuration warm-ups are saved separately and excluded. The final 126-run dataset contains every completed measured run, including the 15.874779-second MPI pipeline result for random seed 317. No slow successful measurement was removed. Pipeline timings are separate from command-wall timings and the untimed sequential reference check.

The two system snapshots showed available guest memory, but do not establish memory or CPU conditions throughout the runs. VM clocks were not used for timing comparisons; pipeline time uses `omp_get_wtime` and command-wall time uses Python's monotonic timer. Both VMs share the same physical laptop, so the evidence does not demonstrate scaling on independent physical servers.
