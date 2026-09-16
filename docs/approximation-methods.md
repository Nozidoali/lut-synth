# Approximation Methods

lut-synth provides two complementary approximation approaches for reducing AND gate count (multiplicative complexity) in XAG networks, each operating at a different abstraction level.

## ILP-Based Truth Table Approximation

Operates at the truth table level before synthesis. Formulates an Integer Linear Program (ILP) that selects which output bits to flip to minimize AND gate cost, subject to an error bound.

### Formulation

For an m-output function with n inputs, introduce binary flip variables d_{i,x} for each output i and input pattern x. The ILP minimizes one of two objectives:

- **GlobalANF**: sum of degree->=2 ANF monomials across all outputs (fast proxy for AND cost)
- **SSCofactor**: exact Select-Swap AND cost using the cofactor product tree model (more accurate, tries all k values)

Subject to the constraint:

    weighted_mean_integer_error <= error_bound

where the integer error is `|f(x) - f'(x)|` treating the multi-bit output as an unsigned integer (MSB first).

### Per-Register Error

When outputs represent multiple hardware registers (e.g., `--registers 1,1,6,6,6`), the error is computed per register rather than across the full concatenated output. Each register's integer error is normalized by its range and the overall error is the mean across registers.

### Variable Pruning

A heuristic pre-processing step fixes flip variables to 0 when flipping a particular bit cannot reduce the objective. This reduces the ILP size significantly for large functions.

### Usage

```
approx-tt --input <file.tt> --output <file.tt> \
          --error-bound <val> \
          --time-limit <seconds> \
          --registers 1,1,6,6,6 \
          --verbose
```

Output is JSON with fields: `solved`, `bits_flipped`, `worst_case_error`, `weighted_mean_error`, `error_rate`, `monomials_before`, `monomials_after`, `ss_and_estimate`, `ss_and_actual`.

Requires Gurobi.

## ResubALS (Resubstitution-based Approximate Logic Synthesis)

Operates at the network level after synthesis. Iteratively substitutes nodes in an XAG network with simpler replacements (Local Approximate Changes) within an error budget.

### Algorithm

Three phases:

1. **Multiple selection** (knapsack): enumerate all candidate LACs, then solve a 0-1 knapsack to select the subset maximizing total size gain within the error budget. Apply selected LACs.
2. **Iterative single selection**: greedily apply the best remaining LAC (by benefit ratio = gain/error) until the error budget is exhausted.
3. **Cleanup**: remove dangling nodes.

### Local Approximate Changes (LACs)

Each LAC replaces a target node with a simpler function:

| Type | Replacement |
|------|-------------|
| Const0 | Constant 0 |
| Const1 | Constant 1 |
| Single | A single existing divisor signal |
| TwoInput | A two-input function (AND, OR, XOR, NAND, NOR, XNOR) of two divisors |

The size gain is the MFFC (Maximum Fanout-Free Cone) size of the removed node minus any new nodes introduced.

### Error Estimators

Four estimator backends, selectable via `EstimatorType`:

| Estimator | Method | Trade-off |
|-----------|--------|-----------|
| Simulation | Random pattern simulation | Fast, statistical |
| VECBEE | Vector-based Boolean error estimation | Balanced |
| Miter | Miter circuit Boolean difference | Exact for patterns |
| Integer | Weighted absolute integer error | For multi-bit integer outputs |

### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `metric` | ER | Error metric (ER, MED, NMED, MSE, MHD, NMHD, INTEGER) |
| `error_bound` | 0.05 | Maximum allowed error |
| `num_patterns` | 102400 | Simulation patterns for estimation |
| `max_divisors` | 150 | Maximum divisors per window |
| `max_lac_size` | 2 | Resub level (0 = const, 1 = single, 2 = two-input) |
| `estimator` | Simulation | Estimator backend |
| `use_knapsack` | true | Use knapsack in phase 1 |

## Error Metrics

Both approximation methods use the same set of error metrics:

| Metric | Definition |
|--------|-----------|
| **ER** | Error rate: fraction of input patterns with any wrong output bit |
| **MED** | Mean error distance: average `|f(x) - f'(x)|` treating outputs as integers |
| **NMED** | Normalized MED: MED / (2^m - 1) |
| **MSE** | Mean squared error: average `(f(x) - f'(x))^2` |
| **MHD** | Mean Hamming distance: average number of differing output bits |
| **NMHD** | Normalized MHD: MHD / m |
| **INTEGER** | Weighted mean absolute integer error with per-input weights |

## Typical Workflow

1. Start with exact truth tables
2. Apply ILP-based approximation (`approx-tt`) to flip output bits within an error budget
3. Synthesize the approximated truth tables to an XAG network (`synth-tt`)
4. Optionally apply ResubALS to further reduce AND gates within the remaining error budget
5. Run resynthesis (AnySyn) for exact optimization on top
