#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export OMPI_MCA_osc=pt2pt OMP_DYNAMIC=FALSE
make
python3 tools/run_followup.py
