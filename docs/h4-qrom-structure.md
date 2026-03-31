# H4 QROM Structure in PrepareTHC

The PrepareTHC circuit for H4 (square hydrogen molecule, cc-pVTZ basis, nmo=56, thc_rank=56) uses two QROM nodes to implement alias sampling for quantum state preparation.

## Alias Sampling Overview

The THC decomposition produces coefficients: a 56x56 symmetric matrix `zeta` (pair interactions) and a 28-element vector `t_l` (one-body eigenvalues). Flattened, this gives ~1652 coefficients with varying magnitudes and signs.

The quantum algorithm prepares a state whose amplitudes encode these coefficients. Alias sampling does this by mapping each coefficient index to a lookup table entry containing: the coefficient's sign, a keep probability, and an alias partner to redirect to when the "keep" path is not taken.

## QROAMClean_0 (Forward State Preparation)

**11 input bits** select one of 2^11 = 2048 possible entries.

These address the flattened Hamiltonian coefficients:

- Indices 0-1595: upper triangle of the `zeta` matrix (56x56 symmetric)
- Indices 1596-1623: `t_l` eigenvalues (28 spatial orbitals)
- Plus padding: **1652 valid entries**, remaining 396 are don't-cares

Input address `s` means "give me the alias sampling data for coefficient s."

**20 output bits** across 5 registers `[1, 1, 6, 6, 6]`:


| Register  | Bits | Meaning                            |
| --------- | ---- | ---------------------------------- |
| theta     | 1    | Sign of coefficient s              |
| alt_theta | 1    | Sign of the alias partner          |
| alt_mu    | 6    | Row index of alias partner         |
| alt_nu    | 6    | Column index of alias partner      |
| keep      | 6    | Keep probability (0-63 out of 2^6) |


## QROM_1 (Adjoint / Uncomputation)

**8 input bits** select one of 2^8 = 256 possible entries.

Each address selects a **batch of 8 coefficients** simultaneously: ceil(1652/8) = 207 valid rows, 49 don't-cares. Input address `r` returns alias data for coefficients `8r, 8r+1, ..., 8r+7` all at once.

**160 output bits** as 8 parallel copies of the same 5 registers:


| Group                      | Bits         | Content                  |
| -------------------------- | ------------ | ------------------------ |
| theta_0 .. theta_7         | 8 x 1b = 8b  | Signs for 8 coefficients |
| alt_theta_0 .. alt_theta_7 | 8 x 1b = 8b  | Alias partner signs      |
| alt_mu_0 .. alt_mu_7       | 8 x 6b = 48b | Alias row indices        |
| alt_nu_0 .. alt_nu_7       | 8 x 6b = 48b | Alias column indices     |
| keep_0 .. keep_7           | 8 x 6b = 48b | Keep probabilities       |


The 8-way parallelism is a circuit optimization: instead of 1652 sequential lookups, 207 lookups each return 8 coefficients.

## Summary


|                     | QROAMClean_0               | QROM_1                        |
| ------------------- | -------------------------- | ----------------------------- |
| Inputs              | 11 bits (1 coefficient)    | 8 bits (batch of 8)           |
| Care entries        | 1652 / 2048                | 207 / 256                     |
| Outputs             | 20 bits (1 copy of 5 regs) | 160 bits (8 copies of 5 regs) |
| Coefficients served | 1652                       | 207 x 8 = 1656                |
| Direction           | Forward (state prep)       | Adjoint (uncomputation)       |


## How Rank and Precision Affect Dimensions

### Formulas

The number of flattened Hamiltonian coefficients determines the QROM size:

```
N_coeff = rank * (rank + 1) / 2  +  nmo / 2
```

The 5 register bitwidths are:

```
theta     = 1                       (always)
alt_theta = 1                       (always)
alt_mu    = ceil(log2(rank))        (index into zeta rows)
alt_nu    = ceil(log2(rank))        (index into zeta columns)
keep      = num_bits_state_prep     (probability precision)
```

QROAMClean dimensions:

```
inputs  = ceil(log2(N_coeff))
outputs = 1 + 1 + bw_mu + bw_nu + num_bits_state_prep
care    = N_coeff
```

QROM dimensions (batched adjoint):

The QROM batches multiple coefficients into each row. The **columns** value is
the parallelism factor (how many coefficients per lookup), not the number of
truth table inputs. It multiplies the output width:

```
bw_per_coeff = 1 + 1 + bw_mu + bw_nu + num_bits_state_prep
columns      = ceil(N_coeff / 2^inputs_QROM)     (coefficients per row)
rows         = ceil(N_coeff / columns)            (care entries in TT)
inputs (PIs) = ceil(log2(rows))                   (address bits for rows)
outputs(POs) = columns * bw_per_coeff             (all columns concatenated)
```

For example with rank=56, bits=6: each row stores 8 coefficients of 20 bits
each, giving 160 output bits. The 8 input bits address 207 valid rows.

### Effect of Rank (H4, nmo=56, bits=6)

Rank drives the number of coefficients quadratically and the index bitwidths logarithmically.

| rank | N_coeff | QROAMClean inputs | QROAMClean outputs | QROM columns | QROM inputs | QROM outputs |
| ---- | ------- | ----------------- | ------------------ | ------------ | ----------- | ------------ |
| 4    | 14      | 4                 | 11                 | 1            | 4           | 11           |
| 10   | 59      | 6                 | 16                 | 2            | 5           | 32           |
| 56   | 1,652   | 11                | 20                 | 8            | 8           | 160          |
| 100  | 5,106   | 13                | 21                 | 16           | 9           | 336          |
| 200  | 20,156  | 15                | 22                 | 32           | 10          | 704          |
| 400  | 80,256  | 17                | 23                 | 64           | 11          | 1,472        |

### Effect of Precision (H4, nmo=56, rank=56)

Precision only affects the `keep` register width. All other dimensions stay fixed.

| bits | keep bw | QROAMClean outputs | QROM outputs |
| ---- | ------- | ------------------ | ------------ |
| 4    | 4       | 18                 | 144          |
| 6    | 6       | 20                 | 160          |
| 8    | 8       | 22                 | 176          |
| 10   | 10      | 24                 | 192          |
| 14   | 14      | 28                 | 224          |
| 16   | 16      | 30                 | 240          |


