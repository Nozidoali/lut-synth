# QCE 2026 Paper Design — Unified Error-Budget Allocation for FTQC Circuits

- **Target venue:** IEEE International Conference on Quantum Computing and Engineering (QCE) 2026, regular paper track, 8-page IEEE double-column.
- **Working title:** "Beyond Exact Boolean: Unified Error-Budget Allocation Across Quantum Arithmetic, Rotation Synthesis, and Magic State Production."
- **Code home:** `git@github.com:Nozidoali/lut-synth.git` (this repo); benchmark corpus from `git@github.com:Nozidoali/hlqcs.git`.
- **Status:** Design finalized; follow-up implementation plan and experiment runs tracked under `writing-plans` after user reviews this spec.

## Thesis

FTQC circuit compilation conventionally treats Boolean/arithmetic components (LUTs, adders, reversible permutations) as **exact**, spending the global logical-error budget `ε_total` entirely on continuous components — rotation synthesis (`ε_U`) and magic state distillation (`ε_T`). We challenge this orthodoxy. We show that allocating a fraction `ε_B > 0` of the budget to Boolean components — and approximating them via multiplicative-complexity-targeted approximate logic synthesis (MC-ALS) — reduces total space-time cost `V_factor` in realistic hardware regimes (`p_phys ≥ 3×10⁻³`). On Shor's windowed modular exponentiation we further find that **per-component budget allocation matters 2–3× on factoring success probability, with winning strategies determined by circuit-level arithmetic structure (`gcd(2^w, r)` in the modexp case)**.

## Contributions

1. **Orthodoxy challenge (Section 1, 3):** first systematic argument and empirical demonstration that `ε_B > 0` beats `ε_B = 0` in FTQC regimes of practical interest.
2. **MC-targeted ALS toolchain (Section 4):** three new/adapted tools that re-target ALS optimization from total gate count to multiplicative complexity (AND count) aligned with T-count:
   - `approx-tt` — ILP formulation giving the MC-optimal reference on small truth tables (`n ≤ 12`);
   - `narrow_and_resub` — per-AND greedy heuristic with a 5-candidate local LAC set (`const0/const1/fanin0/fanin1/XOR(fanin0,fanin1)`), scaling to chained `w ≤ 7` windowed arithmetic;
   - ResubALS with MC re-weighting (`size_gain` counts new AND gates only) — retained as a general divisor-window baseline.
3. **Unified budget framework (Section 3):** formalization of Boolean + rotation + T-state primitives with `(t_i, τ_i)` cost-depth metrics per component; joint optimization with closed-form continuous-side split and 1-D outer sweep on `ε_B`.
4. **Shor's windowed modexp case study (Section 5.2):** experimental demonstration that (a) `ε_B > 0` nets 10–20% `V_factor` savings at `p_phys = 3×10⁻³`; (b) per-window allocation beats uniform by 2–3× P(ok); (c) winning allocation strategy depends on `gcd(2^w, r)`.

## Paper structure

### Section 1 — Introduction

Observation: FTQC error-budget accounting (Fowler 2012, Litinski 2019, Gidney-Ekerå 2021) places all `ε_total` on rotation synthesis + magic state fidelity; Boolean arithmetic is implemented as reversible permutation (exact). Is this the only viable allocation?

State contributions (as above).

### Section 2 — Background & Related Work

- **2.1 Classical ALS.** SALSA, SASIMI, ABACUS; ResubALS (divisor-window + multiple error estimators); approx-tt (ILP flip-bits). Note these optimize **total gate count**, not MC/T-count.
- **2.2 Rotation synthesis.** Ross-Selinger: `t(ε) ≈ 3 log₂(1/ε) + c₀` T gates per rotation; Solovay-Kitaev universality.
- **2.3 Approximate QFT.** Coppersmith 1994 uniform truncation (drop `R_k` for `k > log₂ n`); Barenco 1996 diamond-norm analysis.
- **2.4 Magic state distillation / cultivation.** Bravyi-Kitaev 2005; Litinski 2019 (factory scheduling, closed-form area × time cost); Gidney 2024 cultivation (in-path distillation).
- **2.5 Windowed quantum arithmetic (Gidney 2019).** `k = ⌈m/w⌉` QROMs per modexp exponentiation; **existing literature implements these QROMs as exact**.
- **2.6 FTQC resource models.** Fowler-Mariantoni-Martinis-Cleland surface code; Litinski lattice surgery; Gidney-Ekerå 2021 RSA-2048 (20M qubits, 8 hours). **All assume `ε_B = 0`**.
- **2.7 Gap and positioning.** Four-part claim that no prior work (a) targets ALS at MC, (b) adds a Boolean budget dimension to quantum compilation, (c) removes the `ε_B = 0` assumption, or (d) couples all three primitives under a unified budget constraint.

### Section 3 — Unified Budget Allocation Framework (main contribution)

- **3.1 Problem statement.** Three-way decomposition of `ε_total` into `(ε_B, ε_U, ε_T)`; lift `ε_B` from the fixed-0 default to a free variable.
- **3.2 Cost metrics.** Per-component T count `t_i(ε)` and T depth `τ_i(ε)`; aggregated `T_total`, `T_depth_total`.
- **3.3 Per-primitive cost-error curves.** Boolean empirical (via MC-ALS of Section 4); rotation analytic (Ross-Selinger); T-state analytic (Litinski / Gidney).
- **3.4 FTQC total cost.** `d` is the smallest odd integer satisfying `Q · T_depth · d · ε_cycle(d) ≤ ε_run` where `ε_cycle(d) = A (p_phys/p_th)^((d+1)/2)`; `V_factor = E[trials] · Q · T_depth · d³`.
- **3.5 Joint optimization.** Outer 1-D scan over `ε_B`; inner continuous split `(ε_U, ε_T)` closed-form via Lagrangian.
- **3.6 Recursive sub-allocation.** Primitive-level framework abstracts over internal structure; concrete sub-allocation strategies (e.g. per-LUT in Boolean side) are deferred to experimental sections.

### Section 4 — MC-Targeted Approximate Logic Synthesis

- **4.1 MC cost function.** `gain = #AND_removed − #AND_added` (XOR gates have zero T-cost in XAG).
- **4.2 `approx-tt` (ILP, MC-optimal reference).** Variables `d_{i,x}` and `k_{i,S}` (high-degree ANF monomial selectors). Parity constraint `k_{i,S} ≡ Σ_{T⊆S} d_{i,T} (mod 2)`. Objective `min Σ k_{i,S}` for `|S| ≥ 2`. Gurobi. Scales to `n ≤ 12` truth tables.
- **4.3 `narrow_and_resub` (greedy heuristic).** Per-AND 5-candidate LAC set: `{const0, const1, fanin0, fanin1, XOR(fanin0, fanin1)}`; greedy pick by `gain / integer_err`; IntegerEstimator re-simulation between iterations; exhaustive 2^n simulation for small inputs (key scalability win).
- **4.4 ResubALS with MC re-weight.** Keep existing divisor-window LAC enumeration and Integer/Simulation/VECBEE/Miter error estimators, but restrict `new_nodes` counting to AND gates. Usable baseline when input has don't-cares.
- **4.5 Positioning table.** Three tools span three regimes:
  - `approx-tt` — optimal, small inputs, baseline for narrow's near-optimality claims.
  - `narrow_and_resub` — scalable, runs on chained windowed modexp up to `m=18`.
  - ResubALS-MC — dense TTs with don't-cares (e.g., chemistry QROM), not modexp benchmarks.

### Section 5 — Evaluation

#### 5.1 Random TT micro-benchmark

**Benchmark corpus (from HLQCS repo):** 31 random TTs at `(n, m) ∈ {(4,4), (4,6), (4,8), (8,4), (8,6), (8,8), (12,4), (12,6), (12,8)}` with 3 seeds each, plus `gf_mult4`, `modexp_{2,15,4,4}`, `modexp_{3,221,4,8}`, `modexp_{3,35,8,6}`.

**Methods compared:** Resynthesis (ResubALS, prior art) vs Truncation (bit-drop baseline) vs `approx-tt` (ILP) vs `narrow_and_resub` (heuristic, new fourth column).

**Error-bound sweep:** `ε ∈ {0.5, 1.0, 2.0, 4.0}` (integer-error metric).

**Metrics:** AND-count ratio (vs exact), mean absolute integer error, runtime.

**Headline findings (HLQCS-tabulated prior data; narrow and MC-re-weighted ResubALS are new):**
- ResubALS (default configuration, size-gain target) AND ratio ≡ 1.000 across all 31 × 4 = 124 benchmark-ε pairs — the classical ALS target does not fire on these TTs.
- `approx-tt` (ILP) achieves 0.955 → 0.747 geomean AND ratio across ε = 0.5 → 4.0.
- `narrow_and_resub` (to be measured): target is ≈ 95 % of `approx-tt`'s optimum at 10²–10³× lower runtime, scaling to `n ∈ {16, 20}` where `approx-tt` times out.
- ResubALS with MC re-weighting (to be measured as a new data point): expected to fire on some benchmarks but still dominated by `approx-tt` on optimality and by `narrow` on runtime — measurement confirms or refutes our characterization of Section 4.4.

**Scalability extension:** `n = 16, 20` random TTs showing narrow runs where `approx-tt` Gurobi exceeds timeout and ResubALS makes no progress.

#### 5.2 Shor's windowed modexp case study

**Cases:** `(base, N, w, m)` ∈ {`(5,33,4,12)`, `(3,35,4,12)`, `(7,143,5,15)`, `(7,221,4,16)`, `(2,323,5,18)`}. Pre-existing data under `results/shor_chained_s4fixed/` (4-start synthesis, honest baseline) and `results/shor_stress_completed/` (the `(2,323,5,18)` stress case).

**FTQC parameters:** `p_phys ∈ {10⁻⁴, 10⁻³, 3×10⁻³, 10⁻²}`, `p_th = 10⁻²`, `ε_run = 0.1`, `Q = 2⌈log₂ N⌉ + 4`.

**Three experiments:**

- **Exp A — Orthodoxy challenge.** `ε_B = 0` baseline (all budget on continuous side) vs optimized `(ε_B, ε_U, ε_T)` partition. Report `V_factor` ratio per `p_phys`.
- **Exp B — Per-window allocation.** Fixed `ε_B`, allocate across `k` windows as `{uniform, front_heavy, back_heavy, linear_up, linear_down}`. Report P(ok), E[trials] per strategy. Explicitly contrast `(5,33, r=10, gcd(2^w, r)=2)` where back-heavy wins with `(7,221, r=48, gcd=16)` where front-heavy wins.
- **Exp C — Joint vs per-primitive.** Joint 3-way optimization vs fixing `ε_B = 0` vs fixing continuous-side to Coppersmith-uniform. Show incremental savings from each unlocked dimension.

**Figures (reuse from `results/final/`):**
- **Fig 1** — Conceptual: current FTQC `ε_B = 0` allocation vs our three-way decomposition (schematic, new).
- **Fig 2** — `narrow_and_resub` 5-candidate illustration (new).
- **Fig 3** — Chained windowed modexp architecture (new).
- **Fig 4** — Random TT micro-benchmark AND-ratio comparison (extend from HLQCS `comparison_barplot.png`).
- **Fig 5** — Per-window allocation P(ok) bars across strategies (existing `fig4_per_window_eb.png`).
- **Fig 6** — `V_factor` vs `ε_B` across `p_phys` (existing `fig5_ftqc_regime.png`).
- **Fig 7** — Period damage three-panel on `(3,35)` (existing `fig6_period_damage.png`).

### Section 6 — Discussion & Limitations

- Gate-level noise not modeled; encapsulated only through `ε_run` budget in the FTQC cost formula.
- T-state cost uses Litinski 2019 as default; Gidney 2024 cultivation noted as alternative (plug-in replacement within the framework).
- `narrow_and_resub` is Boolean-only by construction; generalization to continuous gates (rotation synthesis) handled by the existing Ross-Selinger primitive, not by narrow itself.
- Per-window allocation rule tied to `gcd(2^w, r)` proven empirically only for Shor's modexp; theoretical characterization for other arithmetic circuits is future work.
- Monolithic modexp benchmarks (`shor_e2e.py` output) run on much smaller exponent widths than chained-windowed counterparts; they serve as calibration, not primary results.

### Section 7 — Conclusion

Reiterate: unified budget allocation, the `ε_B > 0` regime, and structural per-window allocation are three complementary degrees of freedom ignored by the conventional FTQC stack. On realistic NISQ-to-early-FTQC hardware, activating all three yields 10–20 % space-time savings without sacrificing factoring success. The `lut-synth` codebase (this submission) provides the MC-ALS toolchain, joint-optimization driver, and reproducible Shor's benchmark harness.

## Symbols (paper-wide)

| Symbol | Meaning |
|---|---|
| `ε_B`, `ε_U`, `ε_T` | Boolean, rotation-synthesis, T-state error budgets |
| `ε_total` | Global logical error budget |
| `t_i`, `τ_i` | T count and T depth of component i |
| `T_total`, `T_depth_total` | Circuit totals |
| `d` | Surface-code distance |
| `Q` | Active logical qubit count |
| `V_factor` | Total space-time volume per factoring attempt |
| `p_phys`, `p_th`, `ε_run` | FTQC hardware parameters |
| `k`, `w`, `m` | Windowed modexp: number of windows, window width, total exponent width |
| `r`, `gcd(2^w, r)` | Shor's hidden period and its alignment with window size |

## Follow-up experiments required (to be tracked in writing-plans)

1. **`narrow_and_resub` on 31 HLQCS random-TT benchmarks at ε ∈ {0.5, 1.0, 2.0, 4.0}.** Insert as fourth column of the existing comparison table. Runtime budget: ~1 CPU-hour.
2. **Scalability run at `n = 16, 20` random TTs** for narrow vs ResubALS-MC vs `approx-tt` (expecting `approx-tt` timeout). Runtime: ~2 CPU-hours.
3. **Orthodoxy-challenge experiment (Exp A)**: explicit `ε_B = 0` baseline on all five Shor's cases, then sweep `ε_B > 0` and compute `V_factor(ε_B)` for each `p_phys`. Runtime: ~1 CPU-hour (existing numerical framework).
4. **Joint optimization output tables** for Shor's cases at each `p_phys`. Runtime: minutes (closed-form inner loop).
5. **Fig 1 and Fig 2 creation** (schematics; tikz/matplotlib).

## Open decisions to flag during implementation plan

- Whether to cite cultivation (Gidney 2024) as the T-state primitive throughout or relegate to an appendix note.
- Whether Section 5.1's "ResubALS AND ratio ≡ 1.000" claim is presented as-is or accompanied by an explanatory paragraph on why ResubALS's `size_gain = mffc_size − new_nodes` fails to fire.
- Whether to include a short theoretical bound linking `gcd(2^w, r)` to optimal per-window allocation (speculative; belongs in future work if not derivable by submission).
- Formal name of the framework (we currently call it "unified three-primitive budget"; may want a snappier brand).

## What this spec does not cover

- The actual paper prose. This spec is a design for the paper's structure and claims; drafting text belongs to the implementation plan phase.
- Formal proofs for the `gcd(2^w, r)` rule. Paper will present it as an empirical finding with a partial mechanistic explanation (stride structure of corrupted x-classes vs period alignment).
- Artifact evaluation submission for QCE. If applicable, a separate artifact-evaluation plan will be produced after the paper draft is stable.
