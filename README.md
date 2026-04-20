# lut-synth

Truth-table to XAG logic synthesis library. Extracted from HLQCS.

## Getting Started

One-click setup (clones submodules, builds C++ with tests + tools, creates a conda env with the chemistry-pipeline Python deps):

```bash
git clone --recursive git@github.com:Nozidoali/lut-synth.git
cd lut-synth
./setup.sh
```

Flags:

| Flag | Effect |
|------|--------|
| `--cpp-only` | skip the Python env setup |
| `--python-only` | skip the C++ build |
| `--env NAME` | conda env name (default `lut-synth`) |
| `--no-test` | skip `ctest` |
| `--jobs N` | parallelism for `cmake --build` |

## Manual build

```bash
git submodule update --init --recursive
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

This produces `build/approx-tt`, `build/synth-tt`, and `build/liblut-synth.a`.

## Requirements

- C++17 compiler, CMake >= 3.16
- mockturtle (submodule in `third-party/`)
- Gurobi (optional, auto-detected; enables ILP-based approximation in `approx-tt`)
- Python 3.11, conda (optional, only needed for the chemistry pipeline in `scripts/`)

Python deps (pinned via `setup.sh`): `pyscf`, `numpy<2.3`, `scipy`, `attrs`, `qualtran`, `cirq-core`, `numba<0.62`. The `numpy<2.3` pin is required because `numba` (transitive dep of `qualtran`) does not support numpy >= 2.3 yet.

## Chemistry pipeline

`scripts/qlut_pipeline.py` runs PySCF → THC → qualtran `PrepareTHC` → `.tt` extraction → `approx-tt` ILP → decode → CCSD(T). See `docs/h4-qrom-structure.md` for the 5-register QROM layout and `docs/approximation-methods.md` for the ILP and ResubALS approximation passes.

Example (H4, small rank for a quick smoke test):

```bash
conda activate lut-synth
python scripts/qlut_pipeline.py --system H4 --thc-rank 10 --num-bits 6 \
    --error-bound 1.0 --output-dir results/h4
```

Supported systems in `ccsd_t.get_molecule`: `H4`, `H_chain` (use `--nh`).

## Command-line tools

```bash
build/approx-tt --input  my.tt --output my.approx.tt \
                --error-bound 1.0 --time-limit 60 \
                --h4 --rank 56 --precision 6

build/synth-tt  --input  my.approx.tt --num-random-starts 1
```

`--h4` is a convenience mode that derives `--registers 1,1,⌈log₂R⌉,⌈log₂R⌉,P` and locks every bit except the `keep` register (see `docs/h4-qrom-structure.md`).
