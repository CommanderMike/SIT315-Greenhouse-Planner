#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
worker=${MPI_WORKER:-micky@192.168.20.8}
export OMPI_MCA_osc=pt2pt OMP_DYNAMIC=FALSE
make
# I send only the build files to the worker in one SSH connection.
tar -cf - Makefile src/greenhouse.cpp | ssh -x -o ConnectTimeout=10 -o ServerAliveInterval=15 -o ServerAliveCountMax=2 "$worker" '
set -eu
mkdir -p ~/M4T1D_Followup/src
cd ~/M4T1D_Followup
tar -xf -
make
sha256sum src/greenhouse.cpp
'
sha256sum src/greenhouse.cpp
bash scripts/smoke_followup.sh
