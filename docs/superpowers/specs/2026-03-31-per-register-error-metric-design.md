# Per-Register Error Metric for ILP Truth Table Approximation

## Problem

The ILP in `tt-approximation.cpp` computes integer error treating all m output bits as a single m-bit integer:

```
delta(x) = sum_{i=0}^{m-1} (1 - 2*f_i(x)) * 2^(m-1-i) * d_i(x)
```

QLUT outputs are actually multiple independent registers. For QROAMClean_0 (m=20, registers `[1,1,6,6,6]`), the top bit is a 1-bit flag but gets penalized as `2^19 = 524,288` instead of 1. For QROM_1 (m=160), `1 << 159` overflows `int32_t`, making the ILP numerically broken.

Result: the ILP returns 0 bits flipped and 0 AND savings across all practical error bounds on H4.

## Solution

Replace the single-integer error with a sum of per-register absolute integer errors.

### Current error constraint (single integer)

For each minterm x, one absolute-value variable `e(x)`:

```
delta(x) = sum_{i=0}^{m-1} coeff_i * d_{i,x}    where coeff_i = (1-2*f_i(x)) * 2^(m-1-i)
e(x) >= delta(x)
e(x) >= -delta(x)
```

### New error constraint (per-register)

Given register bitsizes `[bw_0, bw_1, ..., bw_{R-1}]` where `sum(bw_r) = m`:

For each minterm x and each register r, introduce `e_r(x)`:

```
bit_offset_r = sum_{s=0}^{r-1} bw_s
delta_r(x) = sum_{j=0}^{bw_r-1} (1-2*f_{bit_offset_r+j}(x)) * 2^(bw_r-1-j) * d_{bit_offset_r+j, x}
e_r(x) >= delta_r(x)
e_r(x) >= -delta_r(x)
```

Total error: `e(x) = sum_r e_r(x)`

Error bound constraint unchanged: `sum_x weight(x) * e(x) <= error_bound`

### Impact on variable count

- Old: m * num_minterms `e` variables (actually just num_minterms)
- New: R * num_minterms `e_r` variables (R = number of registers, typically 5)
- For QROAMClean_0: 5 * 2048 = 10,240 continuous variables (negligible for Gurobi)

## Changes

### 1. `tt-approximation.hpp` - TTApproxParams

Add field:
```cpp
std::vector<uint32_t> register_bitsizes;  // empty = legacy single-integer mode
```

### 2. `tt-approximation.cpp` - ILP error constraints

In `solve_ilp_ss_for_k()`, replace the single-integer error block (lines 488-502) with:

- If `register_bitsizes` is empty: keep current behavior (backward compatible)
- If populated: create per-register `e_r` variables, compute per-register deltas, sum them into `e(x)`, apply same error bound constraint

The `coeff` computation changes from `(1 << (m-1-i))` to `(1 << (bw_r-1-j))` where j is the bit position within register r. This eliminates the int32_t overflow for large m.

### 3. `tools/approx-tt.cpp` - CLI

Add `--registers` flag accepting comma-separated bitsizes:
```
approx-tt --input f.tt --output out.tt --error-bound 500 --registers 1,1,6,6,6
```

Parse into `params.register_bitsizes`. Assert sum equals number of output bits in the truth table.

### 4. `pipeline/qlut_pipeline.py` - Pass register info

In `approximate_qluts()`, read `target_bitsizes` from extraction metadata and pass `--registers` to `approx-tt`. For multi-column registers (QROM_1), expand `target_bitsizes` by column count.

For QROM_1 with `target_bitsizes=[1,1,6,6,6]` and `data_shapes=[[207,8],...]`:
- Each column is independent, so `tt_bitsizes=[8,8,48,48,48]`
- Pass `--registers 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,6,6,6,6,6,6,6,6,...` (expanded per-column)

Alternatively: pass `--registers` matching `tt_bitsizes` directly: `8,8,48,48,48`. Each register's error is computed independently. This is simpler and still correct since each tt_bitsize group encodes multiple columns of the same physical register.

**Decision**: use `tt_bitsizes` directly (simpler, each group is an independent integer block in the truth table).

## Backward Compatibility

- No `--registers` flag: current single-integer behavior preserved
- GlobalANF objective: unaffected (has no error constraint interaction)
- Existing tests: unchanged

## Testing

Add a test in `test/` that verifies:
- Per-register error is computed correctly for a known small truth table
- Register bitsizes summing to m produces same result as single-integer when R=1
- ILP finds non-trivial approximations when registers allow cheaper MSB flips
