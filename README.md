# lut-synth

Truth-table to XAG logic synthesis library. Extracted from HLQCS.

## Build

```bash
git clone --recursive <url>
cd lut-synth && mkdir build && cd build
cmake .. && make -j
```

## Dependencies

- mockturtle (submodule)
- Gurobi (optional, for ILP-based approximation)
