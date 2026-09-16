# lut-synth

[![build](https://github.com/Nozidoali/lut-synth/actions/workflows/ci.yml/badge.svg)](https://github.com/Nozidoali/lut-synth/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Truth-table to XAG logic synthesis library: exact and approximate.

## Getting Started

```bash
git clone --recursive https://github.com/Nozidoali/lut-synth.git
cd lut-synth
./setup.sh
```

`./setup.sh` clones submodules, builds the C++ library + CLI tools,
and runs ctest. Flags: `--no-test`, `--jobs N`.

## Manual build

```bash
git submodule update --init --recursive
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

This produces `build/approx-tt`, `build/synth-tt`, `build/approx-xag`,
and `build/liblut-synth.a`.

## Requirements

- C++17 compiler, CMake >= 3.16
- mockturtle (submodule in `third-party/`)
- Gurobi (optional, auto-detected; enables ILP-based approximation in
  `approx-tt`)

## Command-line tools

```bash
build/approx-tt  --input my.tt --output my.approx.tt \
                 --error-bound 1.0 --time-limit 60 \
                 --registers 1,1,6,6,6 --lock 0,1,2,3,4,5

build/approx-xag --input my.tt --output my.xag \
                 --method narrow --error-bound 1.0

build/synth-tt   --input my.approx.tt --num-random-starts 1
```

`--registers` is a comma-separated list of per-register bit-widths;
`--lock` lists the output bit indices that must be reproduced exactly
(no don't-cares).

## Experiments

End-to-end experiments -- chemistry pipeline (PySCF -> THC -> qualtran
`PrepareTHC` -> CCSD(T)), Shor's algorithm flow, random-TT benchmark --
live in a separate repository,
[approx_qlut_simulation](https://github.com/WanHsuanLin/approx_qlut_simulation).
It calls back into this repo's CLI tools via the `LUT_SYNTH_ROOT`
environment variable:

```bash
git clone https://github.com/WanHsuanLin/approx_qlut_simulation.git
cd approx_qlut_simulation
LUT_SYNTH_ROOT=/path/to/lut-synth ./setup.sh
```

## Layout

```
lut-synth/
├── src/lut-synth/        # library: synthesis/, resynthesis/, approximate/
├── tools/                # CLI: approx-tt, approx-xag, synth-tt
├── test/                 # Catch2 unit tests
├── third-party/
│   ├── mockturtle/        (submodule, build dependency)
│   └── qlut-benchmarks/   (submodule, benchmark corpus)
└── docs/
    └── approximation-methods.md
```

## License

MIT -- see [LICENSE](LICENSE). This project builds against
[mockturtle](https://github.com/lsils/mockturtle), which is also MIT-licensed.
