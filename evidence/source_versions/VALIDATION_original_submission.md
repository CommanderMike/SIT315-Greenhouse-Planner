# Validation and evidence

## Recorded Ubuntu VM evidence, 7 October 2026

- A 65-window synthetic CSV smoke test passed in all four modes on one VM.
- The same four-mode smoke test passed across master and worker with an uneven final chunk. All four checksums were 4464113005703204511.
- All 72 benchmark files in `results/bench_20261007_034644` passed full result verification.
- The audited logs had the expected window/scenario-step totals, matching checksums for each input case, complete rank/thread work accounting, and both VM hostnames in every distributed run.
- Summary medians, ranges and speedups were independently recalculated from the logs and matched both saved CSV files.
- The controlled stress test recorded startup and a 120-second timeout (exit code 124). It did not produce a completed stress result.

## Development-only checks

The earlier local CPU fallback tests exercised window counts 1, 3, 65 and 257, uneven chunks, invalid arguments, missing input and a NaN reading. A hand-checkable one-hour moist-soil input selected plan zero with zero penalty and watering. These checks support the forecast and scheduling logic; their timings are not VM benchmark evidence.

## Limits

The sequential reference calls the same forecast function, so full verification tests distribution and scheduling consistency rather than independent scientific correctness. There is no crop calibration, live sensor capture, independent physical cluster, empirical chunk-size optimum or guaranteed grade claim. Measured runtime variability is retained, not removed.

## Submission comment check

After adding explanatory comments, all non-comment source lines were compared with the benchmarked version and matched exactly. A local fallback syntax check also passed. This is a documentation change to the source, not a new MPI algorithm or another measured benchmark.
