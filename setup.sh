#!/usr/bin/env bash
#
# One-click C++ setup for lut-synth: clones submodules, configures and
# builds with tests + tools, optionally runs ctest.
#
# Flags:
#   --no-test    skip ctest
#   --jobs N     parallelism for cmake --build (default: nproc)
#
# Requirements: git, cmake >= 3.16, a C++17 compiler.
# Optional: Gurobi (auto-detected via cmake/FindGUROBI.cmake, enables ILP
# in approx-tt).
#
# For the experiments harness (Python, conda, chemistry pipeline,
# Shor's flow), see third-party/approx_qlut_simulation/setup.sh.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

DO_TEST=1
JOBS="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-test) DO_TEST=0; shift ;;
    --jobs)    JOBS="$2"; shift 2 ;;
    -h|--help) sed -n '3,15p' "$0"; exit 0 ;;
    *) echo "Unknown flag: $1" >&2; exit 1 ;;
  esac
done

log() { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }

log "Initializing git submodules"
git submodule update --init --recursive

log "Configuring cmake (BUILD_TESTS=ON, BUILD_TOOLS=ON)"
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON

log "Building (jobs=$JOBS)"
cmake --build build -j"$JOBS"

if [[ "$DO_TEST" == 1 ]]; then
  log "Running ctest"
  ctest --test-dir build --output-on-failure
fi

log "Setup complete"
