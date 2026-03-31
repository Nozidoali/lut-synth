# lut-synth

Truth-table to XAG logic synthesis library. Extracted from HLQCS.

## Getting Started

```bash
git clone --recursive <url>
cd lut-synth
cmake -B build -DBUILD_TESTS=ON && cmake --build build -j8
ctest --test-dir build --output-on-failure
```

## Requirements

- C++17 compiler
- CMake >= 3.16
- mockturtle (submodule, in `third-party/`)
- Gurobi (optional, for ILP-based approximation)
