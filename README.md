<p align="center">
  <img src="docs/assets/logo.svg" width="128" height="128" alt="lut-synth logo">
</p>

# lut-synth

Part of the [High-Level Quantum Circuit Synthesis Toolkit](https://github.com/Nozidoali/q-hls).

[![build](https://github.com/Nozidoali/lut-synth/actions/workflows/ci.yml/badge.svg)](https://github.com/Nozidoali/lut-synth/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Logic synthesis for XAG networks, minimising multiplicative complexity
(AND count). The repository ships two families of command-line tools:

- `lut-synth-<method>` reads a truth table and writes a Verilog netlist.
- `lut-resyn-<method>` reads a Verilog netlist and writes an optimised one.

## Getting started

```bash
git clone --recursive https://github.com/Nozidoali/lut-synth.git
cd lut-synth
./setup.sh
```

`setup.sh` initialises submodules, builds the library and tools, and runs
the tests. Flags: `--no-test`, `--jobs N`.

To build by hand:

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

Requires a C++17 compiler and CMake 3.16 or newer. Options: `BUILD_TOOLS`
and `BUILD_TESTS`, both `ON` by default.

## Synthesis: truth table to Verilog

Every tool takes `--input <file.tt>`, optionally `--output <file.v>`, and
prints a one-line JSON report. Run any of them with `--help` for the full
flag list.

| Tool | Method |
|---|---|
| `lut-synth-ss` | Select-swap: Shannon decomposition over `k` selector bits plus an ANF cover of each cofactor. Best quality, and the only method that exploits don't-cares. |
| `lut-synth-davio` | Positive Davio decomposition. Fast, suits functions with XOR structure. |
| `lut-synth-dsd` | Disjoint support decomposition. Compact when the support decomposes. |
| `lut-synth-exact` | SAT-based exact synthesis. Minimum AND count, but exponential; practical up to roughly six inputs. |

A `.tt` file holds one line per output, each line `2^n` characters drawn
from `0`, `1` and `X` (don't-care), indexed so that character `i` is the
output for input assignment `i`.

```bash
$ printf '0110100110010110\n' > parity.tt
$ build/lut-synth-ss --input parity.tt --output parity.v
{"method":"ss","num_inputs":4,"num_outputs":1,"has_dont_cares":false,"size":3,"and_count":0}
```

## Resynthesis: Verilog to Verilog

| Tool | Method | Exact |
|---|---|---|
| `lut-resyn-anysyn` | AnySyn cost-generic optimisation: alternating cost-generic resubstitution, minmc cut rewriting and LUT-based perturbation under a time budget. | yes |
| `lut-resyn-narrow` | For each AND gate, try the five substitutions expressible from its own fanins (constant 0/1, either fanin, their XOR) and greedily apply those that fit the error budget. Solver-free. | no |
| `lut-resyn-resubals` | Three-phase approximate resubstitution over an enumerated divisor window, with a choice of error estimator. | no |

The two approximate tools take `--error-bound`; `lut-resyn-narrow` also
takes `--lock a,b,...` to pin output bits that must be reproduced exactly.

```bash
$ build/lut-resyn-anysyn --input parity.v --output parity.opt.v --timeout 10
{"method":"anysyn","num_inputs":4,"num_outputs":1,"size_before":3,"size_after":3,"and_before":0,"and_after":0}
```

## Layout

```
lut-synth/
├── src/lut-synth/
│   ├── synthesis/        # truth table to XAG
│   ├── resynthesis/      # exact XAG optimisation
│   └── approximate/      # error-bounded XAG optimisation
├── tools/                # one source file per binary
├── test/                 # Catch2 unit tests
├── third-party/
│   ├── mockturtle/       (submodule, build dependency)
│   └── qlut-benchmarks/  (submodule, benchmark corpus)
└── docs/
    └── approximation-methods.md
```

## Experiments

End-to-end experiments -- chemistry pipeline (PySCF, THC, qualtran
`PrepareTHC`, CCSD(T)), Shor's algorithm flow, random truth-table
benchmarks -- live in
[approx_qlut_simulation](https://github.com/WanHsuanLin/approx_qlut_simulation),
which drives this repository's binaries through the `LUT_SYNTH_ROOT`
environment variable:

```bash
git clone https://github.com/WanHsuanLin/approx_qlut_simulation.git
cd approx_qlut_simulation
LUT_SYNTH_ROOT=/path/to/lut-synth ./setup.sh
```

## Citation

If you use this work, please cite
[AnySyn](https://arxiv.org/abs/2311.14721) ([CITATION.bib](CITATION.bib)):

```bibtex
@article{wang2023anysyn,
  title   = {AnySyn: A Cost-Generic Logic Synthesis Framework with Customizable Cost Functions},
  author  = {Wang, Hanyu and Lee, Siang-Yun and De Micheli, Giovanni},
  journal = {arXiv preprint arXiv:2311.14721},
  year    = {2023},
  url     = {https://arxiv.org/abs/2311.14721}
}
```

## License

MIT -- see [LICENSE](LICENSE). Built against
[mockturtle](https://github.com/lsils/mockturtle), which is also MIT-licensed.
