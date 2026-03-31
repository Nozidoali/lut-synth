# H4 Keep-Only Approximation Mode

## Goal

Add an `--h4` CLI mode to `approx-tt` that automatically derives register layout and output locking from rank and precision, so that only the `keep` register is approximated with integer distance error.

## Background

In the PrepareTHC QROM for H4, the 20-bit output `[theta(1), alt_theta(1), alt_mu(6), alt_nu(6), keep(6)]` encodes alias sampling metadata. Integer distance is a meaningful error metric only for the `keep` register (probability). For `alt_mu`/`alt_nu` (matrix indices) and `theta`/`alt_theta` (signs), integer distance has no physical meaning. The correct approach is to lock those registers and only approximate `keep`.

## Design

### Scope

Only `tools/approx-tt.cpp` changes. No C++ library changes needed -- `locked_outputs` and `register_bitsizes` in `TTApproxParams` already provide the required functionality.

### New CLI flags

| Flag | Type | Description |
|------|------|-------------|
| `--h4` | bool | Enable H4 mode |
| `--rank` | uint32 | THC rank (required with `--h4`) |
| `--precision` | uint32 | Keep register bitwidth (required with `--h4`) |

### Derived parameters

When `--h4` is active:

```
bw_mu = bw_nu = ceil(log2(rank))
registers = [1, 1, bw_mu, bw_nu, precision]
locked_bits = 2 + 2 * bw_mu   (first 4 registers)
locked_outputs[0..locked_bits-1] = true
```

### Error bound semantics

The `--error-bound` value is the mean absolute integer error of the keep register:

```
E[|keep(x) - keep'(x)|]
```

Range: 0 to `2^precision - 1`. No numerical conversion needed -- locking the other registers makes their ILP error contribution zero, so the ILP constraint naturally reduces to keep-only error.

### Mutual exclusion

`--h4` is mutually exclusive with `--registers` and `--lock`. If both are specified, print an error and exit.

### Validation

- `--h4` requires both `--rank` and `--precision`
- `rank` must be >= 2
- `precision` must be >= 1
- Total output bits `(2 + 2*ceil(log2(rank)) + precision)` must equal the number of truth table outputs in the input file

### Output

JSON output unchanged. The existing fields (`bits_flipped`, `error_rate`, `ss_and_estimate`, etc.) all still apply. The `bits_flipped` count will only include keep register bits since others are locked.
