# Runtime notes

The working cross-VM tests and benchmark were launched after exporting `OMPI_MCA_osc=pt2pt` on the master. This shell setting is not printed in the original benchmark or stress command logs. Reproduction commands in the updated README explicitly include it.

Master: 192.168.20.7; worker: 192.168.20.8; interface: enp0s3.
Host CPU: Intel Core i5-8265U, 4 physical cores and 8 logical processors.
Host RAM: 7.8 GiB reported by PowerShell; OS: Windows 11 Pro 10.0.26200.
Both guests: Ubuntu 24.04.4 LTS, 2 vCPUs, about 1.9 GiB RAM, zero guest swap.

SHA-256 of the original benchmarked C++ source: `497444ef2d5f4aa63ebf5cd14212cba4ea63ed637f14ded54161e4bfd62e899c`.
SHA-256 of the commented submission source: `48b43610c2f7e1d4611791542ba9bc447492405891387f82d19837df18ef047f`.

First-person explanatory comments were added after the benchmark. Every non-comment source line is identical to the original. The algorithms, constants, compiler options and execution scripts have not changed. The original benchmark and stress logs are preserved. The repository source contains the commented submission version.
