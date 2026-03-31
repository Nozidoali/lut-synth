# Per-Register Error Metric Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace single-integer error constraint in the ILP with per-register error so the ILP can cheaply flip bits in low-bitwidth registers, enabling AND gate savings on H4 QLUTs.

**Architecture:** Add `register_bitsizes` to `TTApproxParams`. Both ILP functions (`solve_ilp` and `solve_ilp_ss_for_k`) get a shared helper that builds per-register error constraints. The `approx-tt` CLI gains a `--registers` flag. The pipeline reads `tt_bitsizes` from extraction metadata and passes it through.

**Tech Stack:** C++17, Gurobi ILP, Python 3, kitty, Catch2

---

### Task 1: Add register_bitsizes to TTApproxParams

**Files:**
- Modify: `src/lut-synth/approximate/tt-approximation/tt-approximation.hpp:19-27`

- [ ] **Step 1: Add the field**

In `TTApproxParams`, add `register_bitsizes` after the existing fields:

```cpp
struct TTApproxParams {
    double error_bound = 1.0;
    std::vector<double> weights;
    double time_limit = 60.0;
    bool enable_pruning = true;
    bool verbose = false;
    TTApproxObjective objective = TTApproxObjective::SSCofactor;
    int fixed_k = -1;
    std::vector<uint32_t> register_bitsizes; /*!< Per-register bitwidths (empty = single integer) */
};
```

- [ ] **Step 2: Build to verify no breakage**

Run: `cmake --build build -j8`
Expected: clean build (new field default-initializes to empty vector)

- [ ] **Step 3: Commit**

```bash
git add src/lut-synth/approximate/tt-approximation/tt-approximation.hpp
git commit -m "Add register_bitsizes field to TTApproxParams"
```

---

### Task 2: Add per-register error helper and update solve_ilp (GlobalANF)

**Files:**
- Modify: `src/lut-synth/approximate/tt-approximation/tt-approximation.cpp:152-213,280-320`

This task modifies three locations in the file:
1. The pruning function `compute_pruning_mask` (line 152) — change bit_weight from global to per-register
2. The `e_vars` creation block (line 282) — upper bounds change per-register
3. The error constraint block (lines 306-320) — replace with per-register deltas

- [ ] **Step 1: Add helper function to compute per-register bit weight**

Add this helper inside the anonymous namespace, before `compute_pruning_mask` (before line 152):

```cpp
double bit_weight_for_output(uint32_t output_index, uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes) {
    if (register_bitsizes.empty()) {
        return static_cast<double>(1u << (m - 1 - output_index));
    }
    uint32_t offset = 0;
    for (uint32_t bw : register_bitsizes) {
        if (output_index < offset + bw) {
            uint32_t j = output_index - offset;
            return static_cast<double>(1u << (bw - 1 - j));
        }
        offset += bw;
    }
    assert(false);
    return 1.0;
}

double max_error_per_minterm(uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes) {
    if (register_bitsizes.empty()) {
        return static_cast<double>((1u << m) - 1);
    }
    double total = 0.0;
    for (uint32_t bw : register_bitsizes) {
        total += static_cast<double>((1u << bw) - 1);
    }
    return total;
}
```

- [ ] **Step 2: Update compute_pruning_mask to use helper**

Replace line 169:
```cpp
double bit_weight = static_cast<double>(1u << (m - 1 - i));
```
with:
```cpp
double bit_weight = bit_weight_for_output(i, m, register_bitsizes);
```

And update `compute_pruning_mask` signature to accept the register_bitsizes parameter. Change:
```cpp
PruningMask compute_pruning_mask(
    ANFData const& anf,
    uint32_t m,
    std::vector<double> const& weights,
    double error_bound) {
```
to:
```cpp
PruningMask compute_pruning_mask(
    ANFData const& anf,
    uint32_t m,
    std::vector<double> const& weights,
    double error_bound,
    std::vector<uint32_t> const& register_bitsizes) {
```

- [ ] **Step 3: Update the call site in solve_ilp (line ~240)**

Change:
```cpp
pmask = compute_pruning_mask(anf, m, weights, params.error_bound);
```
to:
```cpp
pmask = compute_pruning_mask(anf, m, weights, params.error_bound,
                             params.register_bitsizes);
```

- [ ] **Step 4: Update e_vars upper bound in solve_ilp (line ~285)**

Replace:
```cpp
double ub = static_cast<double>((1u << m) - 1);
```
with:
```cpp
double ub = max_error_per_minterm(m, params.register_bitsizes);
```

- [ ] **Step 5: Replace error constraint block in solve_ilp (lines 306-320)**

Replace the block:
```cpp
    for (uint32_t x = 0; x < num_minterms; ++x) {
        GRBLinExpr delta = 0;
        for (uint32_t i = 0; i < m; ++i) {
            int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
            delta += coeff * d_vars[i][x];
        }
        model.addConstr(e_vars[x] >= delta, "abs_pos_" + std::to_string(x));
        model.addConstr(e_vars[x] >= -delta, "abs_neg_" + std::to_string(x));
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound, "error_bound");
```

with:
```cpp
    if (params.register_bitsizes.empty()) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr delta = 0;
            for (uint32_t i = 0; i < m; ++i) {
                int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
                delta += coeff * d_vars[i][x];
            }
            model.addConstr(e_vars[x] >= delta, "abs_pos_" + std::to_string(x));
            model.addConstr(e_vars[x] >= -delta, "abs_neg_" + std::to_string(x));
        }
    } else {
        uint32_t R = params.register_bitsizes.size();
        std::vector<std::vector<GRBVar>> er_vars(R);
        for (uint32_t r = 0; r < R; ++r) {
            er_vars[r].reserve(num_minterms);
            double ub = static_cast<double>((1u << params.register_bitsizes[r]) - 1);
            for (uint32_t x = 0; x < num_minterms; ++x) {
                er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
                    "er_" + std::to_string(r) + "_" + std::to_string(x)));
            }
        }

        uint32_t bit_offset = 0;
        for (uint32_t r = 0; r < R; ++r) {
            uint32_t bw = params.register_bitsizes[r];
            for (uint32_t x = 0; x < num_minterms; ++x) {
                GRBLinExpr delta_r = 0;
                for (uint32_t j = 0; j < bw; ++j) {
                    uint32_t i = bit_offset + j;
                    int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (bw - 1 - j));
                    delta_r += coeff * d_vars[i][x];
                }
                model.addConstr(er_vars[r][x] >= delta_r,
                    "abs_pos_r" + std::to_string(r) + "_" + std::to_string(x));
                model.addConstr(er_vars[r][x] >= -delta_r,
                    "abs_neg_r" + std::to_string(r) + "_" + std::to_string(x));
            }
            bit_offset += bw;
        }

        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr sum_er = 0;
            for (uint32_t r = 0; r < R; ++r) {
                sum_er += er_vars[r][x];
            }
            model.addConstr(e_vars[x] >= sum_er,
                "e_sum_" + std::to_string(x));
        }
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound, "error_bound");
```

- [ ] **Step 6: Build to verify**

Run: `cmake --build build -j8`
Expected: clean build

- [ ] **Step 7: Commit**

```bash
git add src/lut-synth/approximate/tt-approximation/tt-approximation.cpp
git commit -m "Add per-register error constraints to GlobalANF ILP"
```

---

### Task 3: Update solve_ilp_ss_for_k (SSCofactor) with per-register error

**Files:**
- Modify: `src/lut-synth/approximate/tt-approximation/tt-approximation.cpp:396-502`

The SSCofactor function has its own pruning and error constraint blocks that need the same treatment.

- [ ] **Step 1: Update pruning in solve_ilp_ss_for_k (line ~403)**

Replace:
```cpp
            if (params.enable_pruning) {
                double bit_weight = static_cast<double>(1u << (m - 1 - i));
                if (weights[x] * bit_weight > params.error_bound) {
                    ub = 0.0;
                }
            }
```
with:
```cpp
            if (params.enable_pruning) {
                double bit_weight = bit_weight_for_output(i, m, params.register_bitsizes);
                if (weights[x] * bit_weight > params.error_bound) {
                    ub = 0.0;
                }
            }
```

- [ ] **Step 2: Replace error constraint block in solve_ilp_ss_for_k (lines 488-502)**

Replace the block:
```cpp
    for (uint32_t x = 0; x < num_minterms; ++x) {
        GRBLinExpr delta = 0;
        for (uint32_t i = 0; i < m; ++i) {
            int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
            delta += coeff * d_vars[i][x];
        }
        model.addConstr(e_vars[x] >= delta);
        model.addConstr(e_vars[x] >= -delta);
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound);
```

with the same per-register logic as Task 2 Step 5 (identical code — both functions need the same error constraint pattern):

```cpp
    if (params.register_bitsizes.empty()) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr delta = 0;
            for (uint32_t i = 0; i < m; ++i) {
                int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
                delta += coeff * d_vars[i][x];
            }
            model.addConstr(e_vars[x] >= delta);
            model.addConstr(e_vars[x] >= -delta);
        }
    } else {
        uint32_t R = params.register_bitsizes.size();
        std::vector<std::vector<GRBVar>> er_vars(R);
        for (uint32_t r = 0; r < R; ++r) {
            er_vars[r].reserve(num_minterms);
            double ub = static_cast<double>((1u << params.register_bitsizes[r]) - 1);
            for (uint32_t x = 0; x < num_minterms; ++x) {
                er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
                    "er_" + std::to_string(r) + "_" + std::to_string(x)));
            }
        }

        uint32_t bit_offset = 0;
        for (uint32_t r = 0; r < R; ++r) {
            uint32_t bw = params.register_bitsizes[r];
            for (uint32_t x = 0; x < num_minterms; ++x) {
                GRBLinExpr delta_r = 0;
                for (uint32_t j = 0; j < bw; ++j) {
                    uint32_t i = bit_offset + j;
                    int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (bw - 1 - j));
                    delta_r += coeff * d_vars[i][x];
                }
                model.addConstr(er_vars[r][x] >= delta_r);
                model.addConstr(er_vars[r][x] >= -delta_r);
            }
            bit_offset += bw;
        }

        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr sum_er = 0;
            for (uint32_t r = 0; r < R; ++r) {
                sum_er += er_vars[r][x];
            }
            model.addConstr(e_vars[x] >= sum_er);
        }
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound);
```

- [ ] **Step 3: Update e_vars upper bound in solve_ilp_ss_for_k**

Find the e_vars creation (around line 441-447 in solve_ilp_ss_for_k):
```cpp
    std::vector<GRBVar> e_vars;
    e_vars.reserve(num_minterms);
    for (uint32_t x = 0; x < num_minterms; ++x) {
        double ub = static_cast<double>((1u << m) - 1);
        e_vars.push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
            "e_" + std::to_string(x)));
    }
```

Replace `static_cast<double>((1u << m) - 1)` with:
```cpp
        double ub = max_error_per_minterm(m, params.register_bitsizes);
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build build -j8`
Expected: clean build

- [ ] **Step 5: Commit**

```bash
git add src/lut-synth/approximate/tt-approximation/tt-approximation.cpp
git commit -m "Add per-register error constraints to SSCofactor ILP"
```

---

### Task 4: Add --registers flag to approx-tt CLI

**Files:**
- Modify: `tools/approx-tt.cpp`

- [ ] **Step 1: Add registers field to Args and parsing**

Replace the entire `Args` struct and `parse_args` function:

```cpp
struct Args {
    std::string input;
    std::string output;
    double error_bound = 1.0;
    double time_limit = 60.0;
    bool verbose = false;
    std::vector<uint32_t> registers;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            args.input = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            args.output = argv[++i];
        } else if ((arg == "--error-bound" || arg == "-e") && i + 1 < argc) {
            args.error_bound = std::stod(argv[++i]);
        } else if ((arg == "--time-limit" || arg == "-t") && i + 1 < argc) {
            args.time_limit = std::stod(argv[++i]);
        } else if (arg == "--registers" && i + 1 < argc) {
            std::string val(argv[++i]);
            std::istringstream ss(val);
            std::string token;
            while (std::getline(ss, token, ',')) {
                args.registers.push_back(
                    static_cast<uint32_t>(std::stoul(token)));
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        }
    }
    return args;
}
```

- [ ] **Step 2: Add sstream include at top of file**

Add after the existing includes:
```cpp
#include <sstream>
```

- [ ] **Step 3: Pass registers to params in main()**

After `params.verbose = args.verbose;` (line ~59), add:
```cpp
    params.register_bitsizes = args.registers;
```

- [ ] **Step 4: Update print_usage**

Replace:
```cpp
void print_usage() {
    std::cerr << "Usage: approx-tt --input <file> --output <file> "
              << "[--error-bound <val>] [--time-limit <sec>] [--verbose]\n";
}
```
with:
```cpp
void print_usage() {
    std::cerr << "Usage: approx-tt --input <file> --output <file> "
              << "[--error-bound <val>] [--time-limit <sec>] "
              << "[--registers 1,1,6,6,6] [--verbose]\n";
}
```

- [ ] **Step 5: Build and smoke-test**

Run: `cmake --build build -j8`
Expected: clean build

- [ ] **Step 6: Commit**

```bash
git add tools/approx-tt.cpp
git commit -m "Add --registers flag to approx-tt CLI"
```

---

### Task 5: Update pipeline to pass register info

**Files:**
- Modify: `pipeline/qlut_pipeline.py:363-395`

- [ ] **Step 1: Add metadata_path parameter to approximate_qluts**

Replace the function signature and body:

```python
def approximate_qluts(
    tt_dir: Path,
    error_bound: float,
    approx_tt_binary: str | Path,
    time_limit: float = 60.0,
) -> dict[str, Any]:
    """Stage 4: Approximate each .tt file using the C++ approx-tt tool."""
    approx_dir = tt_dir / "approx"
    approx_dir.mkdir(parents=True, exist_ok=True)

    metadata_path = tt_dir / "extraction_metadata.json"
    node_bitsizes: dict[str, list[int]] = {}
    if metadata_path.exists():
        with open(metadata_path) as f:
            meta = json.load(f)
        for node in meta.get("nodes", []):
            node_bitsizes[node["filename"]] = node.get("tt_bitsizes", [])

    tt_files = sorted(tt_dir.glob("*.tt"))
    results = []

    for tt_file in tt_files:
        approx_file = approx_dir / tt_file.name
        cmd = [
            str(approx_tt_binary),
            "--input", str(tt_file),
            "--output", str(approx_file),
            "--error-bound", str(error_bound),
            "--time-limit", str(time_limit),
        ]
        bitsizes = node_bitsizes.get(tt_file.name, [])
        if bitsizes:
            cmd.extend(["--registers", ",".join(str(b) for b in bitsizes)])

        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"Warning: approx-tt failed on {tt_file.name}: {proc.stderr}")
            continue

        stats = json.loads(proc.stdout.strip())
        stats["filename"] = tt_file.name
        results.append(stats)

    _save_json(approx_dir / "approx_stats.json", results)
    return {"approx_dir": str(approx_dir), "results": results}
```

- [ ] **Step 2: Commit**

```bash
git add pipeline/qlut_pipeline.py
git commit -m "Pass register bitsizes from extraction metadata to approx-tt"
```

---

### Task 6: Build, run tests, verify end-to-end

**Files:**
- No new files

- [ ] **Step 1: Full build with tests**

Run: `cmake -B build -DBUILD_TESTS=ON && cmake --build build -j8`
Expected: clean build

- [ ] **Step 2: Run existing tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all existing tests pass (per-register is backward-compatible via empty vector)

- [ ] **Step 3: Commit**

No commit needed if tests pass. If fixes were required, commit them.
