#!/usr/bin/env bash
#
# One-click environment setup for lut-synth.
#
# Default: builds the C++ library + tools, runs ctest, then sets up a
# conda environment with the Python dependencies needed for the quantum
# chemistry pipeline (scripts/qlut_pipeline.py).
#
# Flags:
#   --cpp-only       skip the Python environment setup
#   --python-only    skip the C++ build
#   --env NAME       conda env name to create/reuse (default: lut-synth)
#   --no-test        skip ctest
#   --jobs N         parallelism for cmake --build (default: nproc)
#
# Requirements: git, cmake >= 3.16, a C++17 compiler.
# Optional: Gurobi (auto-detected via cmake/FindGUROBI.cmake, enables ILP).
# Optional: conda (for the Python environment).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

ENV_NAME="lut-synth"
DO_CPP=1
DO_PY=1
DO_TEST=1
JOBS="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cpp-only)    DO_PY=0; shift ;;
    --python-only) DO_CPP=0; shift ;;
    --no-test)     DO_TEST=0; shift ;;
    --env)         ENV_NAME="$2"; shift 2 ;;
    --jobs)        JOBS="$2"; shift 2 ;;
    -h|--help)
      sed -n '3,20p' "$0"; exit 0 ;;
    *)
      echo "Unknown flag: $1" >&2; exit 1 ;;
  esac
done

log() { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }

log "Initializing git submodules"
git submodule update --init --recursive

if [[ "$DO_CPP" == 1 ]]; then
  log "Configuring cmake (BUILD_TESTS=ON, BUILD_TOOLS=ON)"
  cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON

  log "Building (jobs=$JOBS)"
  cmake --build build -j"$JOBS"

  if [[ "$DO_TEST" == 1 ]]; then
    log "Running ctest"
    ctest --test-dir build --output-on-failure
  fi
fi

if [[ "$DO_PY" == 1 ]]; then
  if ! command -v conda >/dev/null 2>&1; then
    log "conda not found, skipping Python env setup"
    log "Install conda (or skip with --cpp-only) if you need the chemistry pipeline"
    exit 0
  fi

  source "$(conda info --base)/etc/profile.d/conda.sh"

  if conda env list | awk '{print $1}' | grep -qx "$ENV_NAME"; then
    log "Reusing existing conda env '$ENV_NAME'"
  else
    log "Creating conda env '$ENV_NAME' (python=3.11)"
    conda create -y -n "$ENV_NAME" python=3.11
  fi

  conda activate "$ENV_NAME"

  log "Installing conda-forge packages (pyscf, numpy<2.3, scipy, attrs)"
  conda install -y -c conda-forge \
    "pyscf>=2.12" \
    "numpy<2.3" \
    "scipy" \
    "attrs"

  log "Installing pip packages (qualtran, cirq-core)"
  python -m pip install --quiet --upgrade pip
  python -m pip install --quiet "qualtran" "cirq-core>=1.4" "numba<0.62"

  log "Sanity check: importing pipeline deps"
  python - <<'PY'
import numpy, pyscf, qualtran, cirq
from qualtran.bloqs.chemistry.thc import PrepareTHC
print(f"  numpy    {numpy.__version__}")
print(f"  pyscf    {pyscf.__version__}")
print(f"  cirq     {cirq.__version__}")
print("  qualtran OK, PrepareTHC importable")
PY

  log "Python env '$ENV_NAME' ready. Activate with: conda activate $ENV_NAME"
fi

log "Setup complete"
