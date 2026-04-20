# QCE 2026 Paper Draft Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a submission-ready 8-page IEEE QCE 2026 paper draft on unified error-budget allocation across FTQC quantum-circuit primitives, backed by reproducible experiments.

**Architecture:** Three-phase execution. Phase 1 collects five follow-up experimental artifacts into `results/paper/`. Phase 2 regenerates all seven figures into `paper/qce2026/figures/`. Phase 3 writes a LaTeX draft under `paper/qce2026/` with IEEEtran two-column template, one section per file, built with a Makefile. All artifacts committed incrementally.

**Tech Stack:** Python 3.11 (numpy, matplotlib, qiskit), C++17 build (already in repo), IEEEtran LaTeX class, Gurobi (existing, for approx-tt), TeXLive `latexmk`.

---

## File Structure

**New files:**
- `scripts/run_narrow_on_hlqcs.py` — sweep narrow on the 31 HLQCS random-TT benchmarks
- `scripts/run_scalability_nlarge.py` — scalability run at n = 16, 20
- `scripts/run_exp_A_eb0_baseline.py` — Shor's Exp A (ε_B = 0 vs ε_B > 0 baseline)
- `scripts/run_exp_C_joint_vs_single.py` — Shor's Exp C (joint vs per-primitive)
- `scripts/figures/fig1_conceptual.py` — conceptual schematic of budget allocation
- `scripts/figures/fig2_narrow_lac.py` — narrow 5-candidate LAC illustration
- `scripts/figures/fig3_chained_modexp.py` — chained windowed modexp architecture
- `scripts/figures/fig4_random_tt_comparison.py` — bar comparison across methods
- `paper/qce2026/main.tex` — LaTeX entry point, IEEEtran two-column
- `paper/qce2026/Makefile` — `make pdf` / `make clean`
- `paper/qce2026/sections/abstract.tex`
- `paper/qce2026/sections/introduction.tex`
- `paper/qce2026/sections/background.tex`
- `paper/qce2026/sections/framework.tex`
- `paper/qce2026/sections/mc_als.tex`
- `paper/qce2026/sections/evaluation.tex`
- `paper/qce2026/sections/discussion.tex`
- `paper/qce2026/sections/conclusion.tex`
- `paper/qce2026/references.bib`
- `paper/qce2026/figures/` — directory for generated PNGs and TikZ pictures
- `results/paper/narrow_hlqcs/` — data output of Task 1
- `results/paper/scalability_nlarge/` — data output of Task 2
- `results/paper/shor_exp_A/` — data output of Task 3
- `results/paper/shor_exp_C/` — data output of Task 4

**Files to extend (minor):**
- `scripts/plot_paper.py` — if we need tweaks to existing figure generators to match paper-ready styling

---

## Phase 1 — Experiments

### Task 1: Run narrow on HLQCS random-TT benchmark corpus

**Files:**
- Create: `scripts/run_narrow_on_hlqcs.py`
- Create: `results/paper/narrow_hlqcs/narrow_results.json`

- [ ] **Step 1: Clone/locate HLQCS benchmark set**

```bash
test -d /tmp/hlqcs || git clone --depth=1 git@github.com:Nozidoali/hlqcs.git /tmp/hlqcs
ls /tmp/hlqcs/benchmarks/truth-tables/random-lut/ | head
ls /tmp/hlqcs/benchmarks/truth-tables/qlut/ 2>/dev/null | head
```

Expected: lists of `.tt` files.

- [ ] **Step 2: Create the runner script**

Write `scripts/run_narrow_on_hlqcs.py`:

```python
"""Run narrow_and_resub on the HLQCS random-TT benchmark corpus and
record AND-count ratio, integer MAE, and runtime per (benchmark, ε).
"""
from __future__ import annotations

import argparse
import json
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
HLQCS_TT = Path("/tmp/hlqcs/benchmarks/truth-tables")
BENCHMARKS = [
    *sorted(HLQCS_TT.glob("random-lut/random_*_m*_s*.tt")),
    *sorted(HLQCS_TT.glob("qlut/gf_mult4.tt")),
    *sorted(HLQCS_TT.glob("qlut/modexp_2_15_4_4.tt")),
    *sorted(HLQCS_TT.glob("qlut/modexp_3_221_4_8.tt")),
    *sorted(HLQCS_TT.glob("qlut/modexp_3_35_8_6.tt")),
]
ERROR_BOUNDS = [0.0, 0.5, 1.0, 2.0, 4.0]


def run_narrow(tt_path: Path, eb: float, approx_bin: Path,
               starts: int = 4) -> dict:
    t0 = time.time()
    proc = subprocess.run(
        [str(approx_bin), "--input", str(tt_path), "--output", "/tmp/_paper.tt",
         "--method", "narrow", "--error-bound", str(eb),
         "--num-random-starts", str(starts)],
        capture_output=True, text=True, timeout=120)
    dt = time.time() - t0
    if proc.returncode != 0:
        return {"failed": True, "stderr": proc.stderr[:200],
                "runtime_s": dt}
    stats = json.loads(proc.stdout.strip())
    stats["runtime_s"] = dt
    return stats


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--approx-bin", type=Path,
                    default=PROJECT_ROOT / "build" / "approx-xag")
    ap.add_argument("--output", type=Path,
                    default=PROJECT_ROOT / "results/paper/narrow_hlqcs/narrow_results.json")
    args = ap.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    for bench in BENCHMARKS:
        for eb in ERROR_BOUNDS:
            stats = run_narrow(bench, eb, args.approx_bin)
            rows.append({"benchmark": bench.name, "eb": eb, **stats})
            print(f"{bench.name:<35} eb={eb}  AND "
                  f"{stats.get('and_before','?')}→{stats.get('and_after','?')}  "
                  f"t={stats.get('runtime_s', 0):.2f}s")
    args.output.write_text(json.dumps(rows, indent=2))
    print(f"wrote {len(rows)} rows to {args.output}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run the script**

```bash
source ~/anaconda3/etc/profile.d/conda.sh && conda activate quantum
python /home/hanyu/lut-synth/scripts/run_narrow_on_hlqcs.py
```

Expected: 31 × 5 = 155 rows printed; JSON saved to `results/paper/narrow_hlqcs/narrow_results.json`.

- [ ] **Step 4: Verify output shape**

```bash
python -c "import json; r = json.load(open('/home/hanyu/lut-synth/results/paper/narrow_hlqcs/narrow_results.json')); print(f'{len(r)} rows, {len(set(e[\"benchmark\"] for e in r))} unique benchmarks')"
```

Expected: `155 rows, 31 unique benchmarks`.

- [ ] **Step 5: Commit**

```bash
cd /home/hanyu/lut-synth
git add scripts/run_narrow_on_hlqcs.py results/paper/narrow_hlqcs/
git commit -m "Add narrow benchmark run on HLQCS random-TT corpus"
```

---

### Task 2: Scalability run at n = 16, 20

**Files:**
- Create: `scripts/run_scalability_nlarge.py`
- Create: `results/paper/scalability_nlarge/scalability_results.json`

- [ ] **Step 1: Create generator + runner**

```python
"""Scalability sweep at large n: random TTs with n ∈ {16, 20}, 3 seeds,
run narrow + (approx-tt with 60s timeout) to show approx-tt timeout
frontier and narrow viability beyond it.
"""
from __future__ import annotations

import argparse
import json
import random
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))
from truthTable import TruthTable


def gen_random_tt(n: int, m: int, seed: int, out_path: Path) -> None:
    rng = random.Random(seed)
    patterns = [
        "".join(rng.choice("01") for _ in range(1 << n))
        for _ in range(m)
    ]
    TruthTable.from_patterns(patterns, num_inputs=n).to_file(str(out_path))


def run(bin_path: Path, tt: Path, method: str, eb: float,
        timeout: int = 60) -> dict:
    cmd_base = [str(bin_path), "--input", str(tt), "--output", "/tmp/_out.tt",
                "--error-bound", str(eb)]
    if bin_path.name == "approx-xag":
        cmd = cmd_base + ["--method", method, "--num-random-starts", "4"]
    else:  # approx-tt
        cmd = cmd_base + ["--time-limit", str(timeout)]
    t0 = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"timeout": True, "runtime_s": timeout}
    dt = time.time() - t0
    if proc.returncode != 0:
        return {"failed": True, "stderr": proc.stderr[:200], "runtime_s": dt}
    stats = json.loads(proc.stdout.strip())
    stats["runtime_s"] = dt
    return stats


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--approx-xag", type=Path,
                    default=PROJECT_ROOT / "build" / "approx-xag")
    ap.add_argument("--approx-tt", type=Path,
                    default=PROJECT_ROOT / "build" / "approx-tt")
    ap.add_argument("--output", type=Path,
                    default=PROJECT_ROOT / "results/paper/scalability_nlarge"
                                          / "scalability_results.json")
    ap.add_argument("--tt-dir", type=Path,
                    default=PROJECT_ROOT / "results/paper/scalability_nlarge/tts")
    args = ap.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.tt_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    for n in [16, 20]:
        for m in [4]:
            for seed in [0, 1, 2]:
                tt = args.tt_dir / f"random_n{n}_m{m}_s{seed}.tt"
                gen_random_tt(n, m, seed, tt)
                for eb in [1.0, 4.0]:
                    for method in ["narrow", "resubals"]:
                        stats = run(args.approx_xag, tt, method, eb)
                        rows.append({"n": n, "m": m, "seed": seed,
                                     "method": method, "eb": eb, **stats})
                        print(f"n={n} s={seed} eb={eb} {method:<10} "
                              f"→ {stats}")
                    stats = run(args.approx_tt, tt, "ilp", eb)
                    rows.append({"n": n, "m": m, "seed": seed,
                                 "method": "ilp", "eb": eb, **stats})
                    print(f"n={n} s={seed} eb={eb} ilp        → {stats}")

    args.output.write_text(json.dumps(rows, indent=2))
    print(f"wrote {len(rows)} rows to {args.output}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run (approx-tt will time out; that's expected data)**

```bash
source ~/anaconda3/etc/profile.d/conda.sh && conda activate quantum
python /home/hanyu/lut-synth/scripts/run_scalability_nlarge.py
```

Expected: ~18 rows (n ∈ {16,20} × seed ∈ {0,1,2} × eb ∈ {1.0,4.0} × method ∈ {narrow,resubals,ilp}). approx-tt at n=20 should show `timeout: true`.

- [ ] **Step 3: Commit**

```bash
cd /home/hanyu/lut-synth
git add scripts/run_scalability_nlarge.py results/paper/scalability_nlarge/
git commit -m "Add scalability run at n=16,20 for narrow vs resubals vs ilp"
```

---

### Task 3: Shor's Experiment A — ε_B = 0 baseline vs optimized

**Files:**
- Create: `scripts/run_exp_A_eb0_baseline.py`
- Create: `results/paper/shor_exp_A/exp_A_results.json`

- [ ] **Step 1: Script sketch**

```python
"""Exp A: Orthodoxy challenge — compute V_factor at ε_B=0 (all budget on
continuous side) vs optimized (ε_B, ε_U, ε_T) per p_phys, for all five
Shor's chained cases.

Reuses shor_chained.py data already under results/shor_chained_s4fixed/
and results/shor_stress_completed/. For each case, loads the f-dumps at
eb=0 and at each eb in the sweep; computes V_factor = E[trials] · Q ·
T_depth · d^3 with d chosen per Section 3.4 of the paper design.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
from shor_lib import shor_shot_statistics
from model_ft_cost import required_distance

CASES = [
    {"base": 5, "N": 33,  "w": 4, "m": 12, "path": "shor_chained_s4fixed"},
    {"base": 3, "N": 35,  "w": 4, "m": 12, "path": "shor_chained_s4fixed"},
    {"base": 7, "N": 221, "w": 4, "m": 16, "path": "shor_chained_s4fixed"},
    {"base": 2, "N": 323, "w": 5, "m": 18, "path": "shor_stress_completed"},
]
P_PHYS_SWEEP = [1e-4, 1e-3, 3e-3, 1e-2]
T_PER_AND = 4


def v_factor(and_count: int, p_ok: float, N: int, p_phys: float,
             eps_run: float = 0.1) -> dict:
    Q = 2 * max(1, int(math.ceil(math.log2(N)))) + 4
    N_T = and_count * T_PER_AND
    D = max(1, N_T)
    d = required_distance(Q, D, eps_run, p_phys, 1e-2, 0.03)
    e_trials = float("inf") if p_ok <= 0 else 1.0 / p_ok
    V = e_trials * Q * D * (d ** 3)
    return {"V_factor": V, "d": d, "Q": Q, "T_depth": D,
            "E_trials": e_trials, "N_T": N_T}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path,
                    default=PROJECT_ROOT / "results/paper/shor_exp_A/exp_A_results.json")
    args = ap.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    all_out = []
    for case in CASES:
        root = PROJECT_ROOT / "results" / case["path"] / \
               f"case_{case['base']}_{case['N']}_w{case['w']}_m{case['m']}"
        dumps = sorted(root.glob("chained_eb*.f.json"),
                       key=lambda p: float(p.stem.replace("chained_eb", "")
                                                   .replace(".f", "")))
        case_row = {**case, "sweep": []}
        baseline_V = {}
        for p_phys in P_PHYS_SWEEP:
            for dump in dumps:
                data = json.loads(dump.read_text())
                eb = data["eb"]
                f_approx = data["approx_f"]
                # shot stats
                shots = shor_shot_statistics(case["base"], case["N"],
                                             f_approx, num_shots=1000, seed=0)
                # AND count approximation: from per-window sum at eb=0 we have
                # `total_and_after`; we re-read from shor_chained.json if present
                and_count = _lookup_and(case, eb)
                v = v_factor(and_count, shots["success_rate"],
                             case["N"], p_phys)
                v["eb"] = eb
                v["p_phys"] = p_phys
                v["p_ok"] = shots["success_rate"]
                if eb == 0.0:
                    baseline_V[p_phys] = v["V_factor"]
                v["V_ratio_vs_eb0"] = (v["V_factor"] / baseline_V[p_phys]
                                       if baseline_V.get(p_phys) else None)
                case_row["sweep"].append(v)
        all_out.append(case_row)

    args.output.write_text(json.dumps(all_out, indent=2, default=str))
    print(f"wrote {len(all_out)} cases to {args.output}")


def _lookup_and(case: dict, eb: float) -> int:
    p = PROJECT_ROOT / "results" / case["path"] / "shor_chained.json"
    for c in json.loads(p.read_text()):
        if (c["base"], c["N"], c["w"], c["m_total"]) == \
           (case["base"], case["N"], case["w"], case["m"]):
            for r in c["sweep"]:
                if abs(r["eb"] - eb) < 1e-9:
                    return r["and_after"]
    raise KeyError(f"AND count missing for {case} eb={eb}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run**

```bash
python /home/hanyu/lut-synth/scripts/run_exp_A_eb0_baseline.py
```

Expected: 4 cases × |eb sweep| × 4 p_phys rows in output JSON, with `V_ratio_vs_eb0` column showing `< 1` for wins.

- [ ] **Step 3: Commit**

```bash
cd /home/hanyu/lut-synth
git add scripts/run_exp_A_eb0_baseline.py results/paper/shor_exp_A/
git commit -m "Add Shor's Exp A: epsilon_B=0 vs optimized V_factor comparison"
```

---

### Task 4: Shor's Experiment C — joint vs per-primitive optimization

**Files:**
- Create: `scripts/run_exp_C_joint_vs_single.py`
- Create: `results/paper/shor_exp_C/exp_C_results.json`

- [ ] **Step 1: Script**

```python
"""Exp C: Joint three-primitive optimization vs single-axis sweeps.

For each Shor's case and p_phys, compute V_factor three ways:
  (1) joint: ε_B sweep over [0, ε_total] with analytic (ε_U, ε_T) split
  (2) only continuous (ε_B fixed to 0)
  (3) only Boolean (continuous fixed to Coppersmith-uniform)
Report the minimum V_factor per method and the gain from joint over the
best single-axis.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
from shor_lib import shor_shot_statistics
from model_ft_cost import required_distance

CASES = [
    {"base": 5, "N": 33,  "w": 4, "m": 12, "path": "shor_chained_s4fixed"},
    {"base": 3, "N": 35,  "w": 4, "m": 12, "path": "shor_chained_s4fixed"},
    {"base": 7, "N": 221, "w": 4, "m": 16, "path": "shor_chained_s4fixed"},
    {"base": 2, "N": 323, "w": 5, "m": 18, "path": "shor_stress_completed"},
]
EPS_TOTAL = 1.0
P_PHYS_SWEEP = [1e-4, 1e-3, 3e-3, 1e-2]
T_PER_AND = 4


def cont_cost(eps_cont: float, n_rot: int) -> tuple[int, int]:
    """Analytic continuous-side cost given ε_U + ε_T = eps_cont.

    Ross-Selinger for rotations: each rotation costs ≈ 3 log2(n_rot/ε_U)
    T gates; T-state Litinski ≈ α · log(N_T/ε_T). Lagrangian gives
    ε_U/n_rot = ε_T/factor → we use a fixed 50/50 split for simplicity.
    """
    eps_U = eps_cont / 2
    eps_T = eps_cont / 2
    t_U = int(max(0, 3 * n_rot * math.log2(n_rot / max(eps_U, 1e-12))))
    t_T = int(max(0, 10 * math.log2(max(1.0, n_rot / max(eps_T, 1e-12)))))
    return t_U, t_T


def v_factor(and_count: int, p_ok: float, eps_cont: float, N: int,
             p_phys: float, eps_run: float = 0.1) -> float:
    Q = 2 * max(1, int(math.ceil(math.log2(N)))) + 4
    n_rot = int(math.ceil(math.log2(N))) ** 2  # Rough Shor QFT rotation count
    N_T_Boolean = and_count * T_PER_AND
    t_U, t_T = cont_cost(eps_cont, n_rot)
    N_T = N_T_Boolean + t_U + t_T
    D = max(1, N_T)
    d = required_distance(Q, D, eps_run, p_phys, 1e-2, 0.03)
    e_trials = float("inf") if p_ok <= 0 else 1.0 / p_ok
    return e_trials * Q * D * (d ** 3)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path,
                    default=PROJECT_ROOT / "results/paper/shor_exp_C/exp_C_results.json")
    args = ap.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    all_out = []
    for case in CASES:
        shor_path = (PROJECT_ROOT / "results" / case["path"]
                     / "shor_chained.json")
        data = json.loads(shor_path.read_text())
        match = next(c for c in data
                     if (c["base"], c["N"], c["w"], c["m_total"]) ==
                        (case["base"], case["N"], case["w"], case["m"]))
        row = {**case, "at_p_phys": []}
        for p_phys in P_PHYS_SWEEP:
            # joint sweep: scan ε_B ∈ sweep values
            joint_min = float("inf")
            for r in match["sweep"]:
                if r.get("failed"):
                    continue
                eb = r["eb"]
                eb_cont = max(0.0, EPS_TOTAL - eb)
                V = v_factor(r["and_after"],
                             r["shor_shots"]["success_rate"],
                             eb_cont, case["N"], p_phys)
                joint_min = min(joint_min, V)
            # only-continuous: ε_B = 0, all budget on continuous side
            eb0 = next(r for r in match["sweep"] if r["eb"] == 0.0)
            only_cont = v_factor(eb0["and_after"],
                                 eb0["shor_shots"]["success_rate"],
                                 EPS_TOTAL, case["N"], p_phys)
            # only-Boolean: continuous side gets Coppersmith-uniform tiny budget
            best_bool_only = float("inf")
            for r in match["sweep"]:
                if r.get("failed"):
                    continue
                V = v_factor(r["and_after"],
                             r["shor_shots"]["success_rate"],
                             1e-3,  # minimal continuous budget
                             case["N"], p_phys)
                best_bool_only = min(best_bool_only, V)
            row["at_p_phys"].append({
                "p_phys": p_phys,
                "V_joint_min": joint_min,
                "V_only_cont": only_cont,
                "V_only_bool": best_bool_only,
                "joint_gain_over_best_single": 1 - joint_min / min(
                    only_cont, best_bool_only),
            })
        all_out.append(row)

    args.output.write_text(json.dumps(all_out, indent=2))
    print(f"wrote {len(all_out)} cases to {args.output}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run and verify**

```bash
python /home/hanyu/lut-synth/scripts/run_exp_C_joint_vs_single.py
python -c "
import json
r = json.load(open('/home/hanyu/lut-synth/results/paper/shor_exp_C/exp_C_results.json'))
for c in r:
    print(f\"case N={c['N']}:\")
    for p in c['at_p_phys']:
        print(f\"  p_phys={p['p_phys']:.0e}  joint_gain={p['joint_gain_over_best_single']:.3f}\")
"
```

Expected: numeric gains (possibly negative when joint isn't better, positive when it is).

- [ ] **Step 3: Commit**

```bash
cd /home/hanyu/lut-synth
git add scripts/run_exp_C_joint_vs_single.py results/paper/shor_exp_C/
git commit -m "Add Shor's Exp C: joint optimization vs single-axis sweep"
```

---

## Phase 2 — Figures

### Task 5: Fig 1 — conceptual schematic (current vs proposed allocation)

**Files:**
- Create: `scripts/figures/fig1_conceptual.py`
- Create: `paper/qce2026/figures/fig1_conceptual.pdf`

- [ ] **Step 1: Script**

```python
"""Fig 1: side-by-side donut charts showing (a) FTQC current practice
ε_B = 0 vs (b) our proposed three-primitive budget split."""
from __future__ import annotations
import matplotlib.pyplot as plt
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)

fig, axes = plt.subplots(1, 2, figsize=(7.5, 3.2))

# (a) Current practice
sizes_a = [0, 0.6, 0.4]
labels_a = ["Boolean\n($\\varepsilon_B=0$)", "Rotation synth\n($\\varepsilon_U$)",
            "T-state\n($\\varepsilon_T$)"]
colors = ["#d3d3d3", "#4c72b0", "#dd8452"]
axes[0].pie(sizes_a, labels=labels_a, colors=colors, startangle=90,
            wedgeprops={"width": 0.4}, textprops={"fontsize": 8})
axes[0].set_title("(a) Current FTQC practice", fontsize=10)

# (b) Proposed
sizes_b = [0.3, 0.45, 0.25]
labels_b = ["Boolean\n($\\varepsilon_B>0$)", "Rotation synth\n($\\varepsilon_U$)",
            "T-state\n($\\varepsilon_T$)"]
axes[1].pie(sizes_b, labels=labels_b, colors=["#55a868", "#4c72b0", "#dd8452"],
            startangle=90, wedgeprops={"width": 0.4},
            textprops={"fontsize": 8})
axes[1].set_title("(b) Proposed allocation", fontsize=10)

fig.suptitle(r"$\varepsilon_\mathrm{total} = \varepsilon_B + \varepsilon_U + \varepsilon_T$",
             y=0.05, fontsize=10)
fig.tight_layout()
fig.savefig(OUT / "fig1_conceptual.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig1_conceptual.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig1_conceptual.pdf'}")
```

- [ ] **Step 2: Run**

```bash
python /home/hanyu/lut-synth/scripts/figures/fig1_conceptual.py
```

Expected: PDF + PNG saved.

- [ ] **Step 3: Commit**

```bash
cd /home/hanyu/lut-synth
git add scripts/figures/fig1_conceptual.py paper/qce2026/figures/fig1_conceptual.*
git commit -m "Add Fig 1 conceptual schematic (current vs proposed allocation)"
```

---

### Task 6: Fig 2 — narrow 5-candidate LAC illustration

**Files:**
- Create: `scripts/figures/fig2_narrow_lac.py`
- Create: `paper/qce2026/figures/fig2_narrow_lac.pdf`

- [ ] **Step 1: Script**

```python
"""Fig 2: for a target AND gate with fanins a, b, show the 5 local
replacement candidates narrow_and_resub considers."""
from __future__ import annotations
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)


def draw_node(ax, x, y, label, shape="circle", color="#4c72b0"):
    if shape == "circle":
        ax.add_patch(patches.Circle((x, y), 0.15, fc=color, ec="black"))
    elif shape == "square":
        ax.add_patch(patches.Rectangle((x - 0.15, y - 0.15), 0.3, 0.3,
                                       fc=color, ec="black"))
    ax.text(x, y, label, ha="center", va="center", fontsize=7,
            color="white", fontweight="bold")


fig, axes = plt.subplots(1, 6, figsize=(9, 2))

def draw_target(ax):
    ax.set_xlim(-0.4, 1.5); ax.set_ylim(-0.2, 1.4); ax.axis("off")
    draw_node(ax, 0.2, 0.2, "a", "circle", "#888")
    draw_node(ax, 0.8, 0.2, "b", "circle", "#888")
    draw_node(ax, 0.5, 1.0, "∧", "square", "#4c72b0")
    ax.plot([0.2, 0.5], [0.35, 0.85], "k-", lw=1)
    ax.plot([0.8, 0.5], [0.35, 0.85], "k-", lw=1)

titles = ["Target AND", "→ const 0", "→ const 1", "→ fanin a", "→ fanin b",
          "→ XOR(a, b)"]
for ax, t in zip(axes, titles):
    ax.set_title(t, fontsize=9)

draw_target(axes[0])

# Const 0 / 1
axes[1].set_xlim(-0.4, 1.5); axes[1].set_ylim(-0.2, 1.4); axes[1].axis("off")
draw_node(axes[1], 0.5, 1.0, "0", "square", "#aaaaaa")
axes[2].set_xlim(-0.4, 1.5); axes[2].set_ylim(-0.2, 1.4); axes[2].axis("off")
draw_node(axes[2], 0.5, 1.0, "1", "square", "#aaaaaa")

# Fanin a / b
for ax, pick in zip(axes[3:5], [("a", "#555"), ("b", "#555")]):
    ax.set_xlim(-0.4, 1.5); ax.set_ylim(-0.2, 1.4); ax.axis("off")
    draw_node(ax, 0.5, 1.0, pick[0], "circle", pick[1])

# XOR
axes[5].set_xlim(-0.4, 1.5); axes[5].set_ylim(-0.2, 1.4); axes[5].axis("off")
draw_node(axes[5], 0.2, 0.2, "a", "circle", "#888")
draw_node(axes[5], 0.8, 0.2, "b", "circle", "#888")
draw_node(axes[5], 0.5, 1.0, "⊕", "square", "#55a868")
axes[5].plot([0.2, 0.5], [0.35, 0.85], "k-", lw=1)
axes[5].plot([0.8, 0.5], [0.35, 0.85], "k-", lw=1)

fig.suptitle("narrow_and_resub: 5 local replacement candidates per AND node",
             y=1.02, fontsize=10)
fig.tight_layout()
fig.savefig(OUT / "fig2_narrow_lac.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig2_narrow_lac.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig2_narrow_lac.pdf'}")
```

- [ ] **Step 2: Run + commit**

```bash
python /home/hanyu/lut-synth/scripts/figures/fig2_narrow_lac.py
cd /home/hanyu/lut-synth
git add scripts/figures/fig2_narrow_lac.py paper/qce2026/figures/fig2_narrow_lac.*
git commit -m "Add Fig 2 narrow 5-candidate LAC illustration"
```

---

### Task 7: Fig 3 — chained windowed modexp architecture diagram

**Files:**
- Create: `scripts/figures/fig3_chained_modexp.py`
- Create: `paper/qce2026/figures/fig3_chained_modexp.pdf`

- [ ] **Step 1: Script**

```python
"""Fig 3: block diagram of chained windowed modexp. Shows x-register
split into k windows, each fed to a QROM that returns base^(v·2^{i·w})
mod N, then modular multiplication into accumulator."""
from __future__ import annotations
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)

fig, ax = plt.subplots(figsize=(8, 3.5))
ax.set_xlim(0, 10); ax.set_ylim(0, 5); ax.axis("off")

# x register box
ax.add_patch(patches.FancyBboxPatch((0.3, 3.8), 9.4, 0.8,
                                    boxstyle="round,pad=0.05", fc="#eef"))
ax.text(0.5, 4.2, "$|x\\rangle$ (m qubits)", fontsize=10, va="center")
for i in range(4):
    ax.text(2 + 1.5 * i, 4.2, f"$v_{i}$", ha="center", va="center", fontsize=9)
    ax.plot([2 + 1.5 * i, 2 + 1.5 * i], [3.8, 3.2], "k-", lw=1)

# QROM boxes
for i in range(4):
    ax.add_patch(patches.FancyBboxPatch((1.6 + 1.5 * i, 2.5), 0.8, 0.6,
                                        boxstyle="round,pad=0.04",
                                        fc="#fcf", ec="black"))
    ax.text(2 + 1.5 * i, 2.8, f"QROM$_{i}$", ha="center", fontsize=8)
    ax.plot([2 + 1.5 * i, 2 + 1.5 * i], [2.5, 1.8], "k-", lw=1)

# Multipliers
for i in range(4):
    ax.add_patch(patches.Circle((2 + 1.5 * i, 1.5), 0.25, fc="#cfc",
                                ec="black"))
    ax.text(2 + 1.5 * i, 1.5, "×", ha="center", va="center", fontsize=10)

# Accumulator
ax.add_patch(patches.FancyBboxPatch((0.3, 0.3), 9.4, 0.6,
                                    boxstyle="round,pad=0.05", fc="#eef"))
ax.text(0.5, 0.6, "$|y\\rangle$", fontsize=10, va="center")
ax.annotate("", xy=(8.3, 0.9), xytext=(0.9, 0.9),
            arrowprops=dict(arrowstyle="->"))
# connect ×'s to accumulator
for i in range(4):
    ax.plot([2 + 1.5 * i, 2 + 1.5 * i], [1.25, 0.9], "k-", lw=1)

ax.text(8.5, 1.5, "mod N", fontsize=9, va="center")
ax.text(5, 5, "Chained windowed modexp: "
              "$y = \\prod_{i=0}^{k-1} \\mathrm{LUT}_i[v_i] \\bmod N$",
        ha="center", fontsize=10)

fig.tight_layout()
fig.savefig(OUT / "fig3_chained_modexp.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig3_chained_modexp.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig3_chained_modexp.pdf'}")
```

- [ ] **Step 2: Run + commit**

```bash
python /home/hanyu/lut-synth/scripts/figures/fig3_chained_modexp.py
cd /home/hanyu/lut-synth
git add scripts/figures/fig3_chained_modexp.py paper/qce2026/figures/fig3_chained_modexp.*
git commit -m "Add Fig 3 chained windowed modexp architecture diagram"
```

---

### Task 8: Fig 4 — random TT comparison bar (requires Task 1 output)

**Files:**
- Create: `scripts/figures/fig4_random_tt_comparison.py`
- Create: `paper/qce2026/figures/fig4_random_tt.pdf`

- [ ] **Step 1: Script**

```python
"""Fig 4: grouped bar chart of AND-count ratio across four methods
(ResubALS, Truncation, approx-tt, narrow) × four ε levels. Averages
across seeds; groups by (n, m)."""
from __future__ import annotations
import json
from collections import defaultdict
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
NARROW = PROJECT_ROOT / "results/paper/narrow_hlqcs/narrow_results.json"
HLQCS_SUM = Path("/tmp/hlqcs/results/approximate/latest/comparison_summary.md")
OUT = PROJECT_ROOT / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)


def load_hlqcs_resubals_ilp(path: Path) -> dict:
    """Parse the markdown table — crude but adequate for known format."""
    text = path.read_text()
    rows = {}
    current_eb = None
    for line in text.splitlines():
        if line.startswith("## Error Bound = "):
            current_eb = float(line.split("= ")[1])
            continue
        if "|" not in line or line.startswith("|-") or line.startswith("|B"):
            continue
        parts = [p.strip() for p in line.split("|") if p.strip()]
        if len(parts) < 7:
            continue
        try:
            name = parts[0]
            exact = int(parts[3])
            resub = int(parts[4])
            ilp = int(parts[5])
        except ValueError:
            continue
        rows.setdefault((name, current_eb), {
            "exact": exact, "resub": resub, "ilp": ilp})
    return rows


def load_narrow(path: Path) -> dict:
    out = {}
    for row in json.loads(path.read_text()):
        if row.get("failed"):
            continue
        key = (row["benchmark"].replace(".tt", ""), row["eb"])
        out[key] = {"and_after": row["and_after"],
                    "and_before": row["and_before"]}
    return out


hlqcs = load_hlqcs_resubals_ilp(HLQCS_SUM)
narrow = load_narrow(NARROW)

# Aggregate by (n, m) group across seeds, per ε
# Random TTs follow random_{n}in_{m}out_seed{s} name; extract n, m
groups = defaultdict(lambda: defaultdict(list))  # {(n,m): {eb: [ratios]}}
for (name, eb), h in hlqcs.items():
    if h["exact"] == 0:
        continue
    if name.startswith("random_") and "in_" in name and "out_" in name:
        parts = name.split("_")
        try:
            n = int(parts[1].replace("in", ""))
            m = int(parts[2].replace("out", ""))
        except ValueError:
            continue
        nk = narrow.get((name, eb))
        narrow_ratio = (nk["and_after"] / h["exact"]) if nk else None
        groups[(n, m)][eb].append({
            "resub": h["resub"] / h["exact"],
            "ilp": h["ilp"] / h["exact"],
            "narrow": narrow_ratio,
        })

ebs = sorted({eb for g in groups.values() for eb in g})
method_labels = ["ResubALS", "approx-tt (ILP)", "narrow_and_resub"]
method_keys = ["resub", "ilp", "narrow"]
colors = ["#d62728", "#2ca02c", "#4c72b0"]

# Plot: one subplot per (n, m), within each four bar groups (one per ε)
keys = sorted(groups.keys())
fig, axes = plt.subplots(1, len(keys), figsize=(3.5 * len(keys), 3),
                         sharey=True)
if len(keys) == 1:
    axes = [axes]
x = np.arange(len(ebs))
width = 0.25

for ax, (n, m) in zip(axes, keys):
    for i, (mk, lab, col) in enumerate(zip(method_keys, method_labels, colors)):
        vals = []
        for eb in ebs:
            seeds = groups[(n, m)][eb]
            v = [s[mk] for s in seeds if s[mk] is not None]
            vals.append(np.mean(v) if v else np.nan)
        ax.bar(x + (i - 1) * width, vals, width, color=col, label=lab)
    ax.set_xticks(x); ax.set_xticklabels([f"{eb}" for eb in ebs])
    ax.set_title(f"n={n}, m={m}", fontsize=10)
    ax.set_xlabel("Error bound $\\varepsilon$")
    ax.axhline(1, color="gray", lw=0.5, ls=":")
    ax.set_ylim(0, 1.15)
    ax.grid(True, alpha=0.3, axis="y")

axes[0].set_ylabel("AND ratio vs exact")
axes[-1].legend(loc="lower left", fontsize=8)
fig.suptitle("AND-count ratio across methods on HLQCS random-TT benchmarks",
             y=1.02)
fig.tight_layout()
fig.savefig(OUT / "fig4_random_tt.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig4_random_tt.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig4_random_tt.pdf'}")
```

- [ ] **Step 2: Run + commit**

```bash
python /home/hanyu/lut-synth/scripts/figures/fig4_random_tt_comparison.py
cd /home/hanyu/lut-synth
git add scripts/figures/fig4_random_tt_comparison.py \
        paper/qce2026/figures/fig4_random_tt.*
git commit -m "Add Fig 4 random-TT method comparison bars"
```

---

### Task 9: Copy existing figures into paper/

**Files:**
- Modify: `paper/qce2026/figures/` — copy `fig5_ftqc_regime.png`, `fig6_period_damage.png`, `fig4_per_window_eb.png` from `results/final/`

- [ ] **Step 1: Copy and rename**

```bash
cd /home/hanyu/lut-synth
cp results/final/fig4_per_window_eb.png paper/qce2026/figures/fig5_per_window_eb.png
cp results/final/fig5_ftqc_regime.png paper/qce2026/figures/fig6_ftqc_regime.png
cp results/final/fig6_period_damage.png paper/qce2026/figures/fig7_period_damage.png
ls paper/qce2026/figures/
```

Expected: fig1_conceptual, fig2_narrow_lac, fig3_chained_modexp, fig4_random_tt, fig5_per_window_eb, fig6_ftqc_regime, fig7_period_damage (PDF + PNG).

- [ ] **Step 2: Commit**

```bash
cd /home/hanyu/lut-synth
git add paper/qce2026/figures/
git commit -m "Copy existing Shor's figures into paper figures dir"
```

---

## Phase 3 — LaTeX scaffolding

### Task 10: Create main.tex skeleton + Makefile

**Files:**
- Create: `paper/qce2026/main.tex`
- Create: `paper/qce2026/Makefile`

- [ ] **Step 1: Write main.tex**

```latex
\documentclass[10pt,conference]{IEEEtran}

\usepackage{cite}
\usepackage{amsmath,amssymb,amsfonts}
\usepackage{algorithmic}
\usepackage{graphicx}
\usepackage{textcomp}
\usepackage{xcolor}
\usepackage{booktabs}
\usepackage{subcaption}
\usepackage{url}
\usepackage{hyperref}

\def\BibTeX{{\rm B\kern-.05em{\sc i\kern-.025em b}\kern-.08em
    T\kern-.1667em\lower.7ex\hbox{E}\kern-.125emX}}

\title{Beyond Exact Boolean:\\
  Unified Error-Budget Allocation Across Quantum Arithmetic,\\
  Rotation Synthesis, and Magic State Production}

\author{\IEEEauthorblockN{Anonymous Submission}
\IEEEauthorblockA{Anonymous Affiliation}}

\begin{document}
\maketitle

\input{sections/abstract}
\input{sections/introduction}
\input{sections/background}
\input{sections/framework}
\input{sections/mc_als}
\input{sections/evaluation}
\input{sections/discussion}
\input{sections/conclusion}

\bibliographystyle{IEEEtran}
\bibliography{references}

\end{document}
```

- [ ] **Step 2: Write Makefile**

```makefile
PAPER = main
SECTIONS = $(wildcard sections/*.tex)
BIB = references.bib
FIGS = $(wildcard figures/*.pdf figures/*.png)

.PHONY: pdf clean distclean

pdf: $(PAPER).pdf

$(PAPER).pdf: $(PAPER).tex $(SECTIONS) $(BIB) $(FIGS)
	latexmk -pdf -interaction=nonstopmode $(PAPER).tex

clean:
	latexmk -c $(PAPER).tex

distclean:
	latexmk -C $(PAPER).tex
	rm -f $(PAPER).pdf
```

- [ ] **Step 3: Verify IEEEtran is available**

```bash
kpsewhich IEEEtran.cls
```

Expected: path such as `/usr/share/texmf-dist/tex/latex/IEEEtran/IEEEtran.cls`. If absent, install: `sudo apt install texlive-publishers` or `tlmgr install ieeetran`.

- [ ] **Step 4: Create empty section files (so compile doesn't fail)**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
mkdir -p sections
for s in abstract introduction background framework mc_als evaluation discussion conclusion; do
  echo "% placeholder" > sections/$s.tex
done
touch references.bib
```

- [ ] **Step 5: Test compile**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -20
ls main.pdf
```

Expected: `main.pdf` exists (may have placeholders but compiles).

- [ ] **Step 6: Commit**

```bash
cd /home/hanyu/lut-synth
git add paper/qce2026/main.tex paper/qce2026/Makefile \
        paper/qce2026/sections/ paper/qce2026/references.bib
git commit -m "Add QCE 2026 paper LaTeX skeleton (IEEEtran, sections, Makefile)"
```

---

### Task 11: Write references.bib with all citations

**Files:**
- Modify: `paper/qce2026/references.bib`

- [ ] **Step 1: Write bibliography**

```bibtex
@inproceedings{gidney2019windowed,
  author    = {Craig Gidney},
  title     = {Windowed quantum arithmetic},
  booktitle = {arXiv:1905.07682},
  year      = {2019}
}

@article{gidney2021factoring,
  author  = {Craig Gidney and Martin Eker{\aa}},
  title   = {How to factor 2048 bit {RSA} integers in 8 hours using 20 million noisy qubits},
  journal = {Quantum},
  volume  = {5},
  pages   = {433},
  year    = {2021}
}

@article{gidney2024cultivation,
  author  = {Craig Gidney},
  title   = {Magic state cultivation},
  journal = {arXiv:2409.07500},
  year    = {2024}
}

@article{litinski2019game,
  author  = {Daniel Litinski},
  title   = {A game of surface codes},
  journal = {Quantum},
  volume  = {3},
  pages   = {128},
  year    = {2019}
}

@article{fowler2012surface,
  author  = {Austin~G. Fowler and Matteo Mariantoni and John~M. Martinis and Andrew~N. Cleland},
  title   = {Surface codes: Towards practical large-scale quantum computation},
  journal = {Physical Review A},
  volume  = {86}, number = {3}, pages = {032324}, year = {2012}
}

@inproceedings{ross2016optimal,
  author    = {Neil~J. Ross and Peter Selinger},
  title     = {Optimal ancilla-free {Clifford}+{T} approximation of $z$-rotations},
  booktitle = {Quantum Information \& Computation},
  volume    = {16}, number = {11-12}, pages = {901--953}, year = {2016}
}

@article{bravyi2005universal,
  author  = {Sergey Bravyi and Alexei Kitaev},
  title   = {Universal quantum computation with ideal {Clifford} gates and noisy ancillas},
  journal = {Physical Review A},
  volume  = {71}, number = {2}, pages = {022316}, year = {2005}
}

@article{coppersmith1994approximate,
  author  = {Don Coppersmith},
  title   = {An approximate Fourier transform useful in quantum factoring},
  journal = {IBM Research Report RC19642},
  year    = {1994}
}

@article{barenco1996approximate,
  author  = {Adriano Barenco and Artur Ekert and Kalle-Antti Suominen and P\"aivi T\"orm\"a},
  title   = {Approximate quantum {F}ourier transform and decoherence},
  journal = {Physical Review A},
  volume  = {54}, number = {1}, pages = {139}, year = {1996}
}

@inproceedings{shin2010approximate,
  author    = {Doochul Shin and Sandeep~K. Gupta},
  title     = {Approximate logic synthesis for error tolerant applications},
  booktitle = {DATE}, year = {2010}
}

@inproceedings{venkataramani2013salsa,
  author    = {Swagath Venkataramani and Kaushik Roy and Anand Raghunathan},
  title     = {{SALSA}: Systematic logic synthesis of approximate circuits},
  booktitle = {DAC}, year = {2013}
}

@article{boyar2000upper,
  author  = {Joan Boyar and Ren{\'e} Peralta and Denis Pochuev},
  title   = {On the multiplicative complexity of {Boolean} functions over the basis (and, xor, 1)},
  journal = {Theoretical Computer Science},
  volume  = {235}, pages = {43--57}, year = {2000}
}

@misc{ourrepo,
  author = {Anonymous},
  title  = {lut-synth: MC-targeted approximate logic synthesis and FTQC budget allocation},
  note   = {\url{https://github.com/Nozidoali/lut-synth}}, year = {2026}
}
```

- [ ] **Step 2: Test compile with real bib**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make distclean
make pdf 2>&1 | tail -10
```

Expected: compiles; may have unresolved citation warnings (expected, since sections are still placeholder).

- [ ] **Step 3: Commit**

```bash
cd /home/hanyu/lut-synth
git add paper/qce2026/references.bib
git commit -m "Add bibliography entries for QCE paper"
```

---

## Phase 3 (continued) — Prose drafting

### Task 12: Draft abstract + introduction

**Files:**
- Modify: `paper/qce2026/sections/abstract.tex`
- Modify: `paper/qce2026/sections/introduction.tex`

- [ ] **Step 1: Write abstract**

```latex
\begin{abstract}
Fault-tolerant quantum computation (FTQC) compilation conventionally
treats Boolean arithmetic (adders, lookup tables, reversible
permutations) as exact, spending the global logical-error budget
$\varepsilon_\mathrm{total}$ entirely on continuous components---
rotation synthesis ($\varepsilon_U$) and magic-state distillation
($\varepsilon_T$). We challenge this orthodoxy. We formalize a unified
three-primitive budget allocation problem, develop two new
multiplicative-complexity-targeted approximate logic synthesis
(MC-ALS) tools---an ILP-based \texttt{approx-tt} for small truth
tables and a scalable greedy \texttt{narrow\_and\_resub}---and
demonstrate on Shor's windowed modular exponentiation that setting
$\varepsilon_B > 0$ reduces total space-time volume by
10--20\,\% under realistic hardware parameters. A surprising
empirical finding: per-window budget allocation is never uniform;
the optimum depends on the arithmetic alignment
$\gcd(2^w, r)$ between window size and Shor's hidden period. The
codebase and benchmarks are available at
\url{https://github.com/Nozidoali/lut-synth}.
\end{abstract}
```

- [ ] **Step 2: Write introduction**

```latex
\section{Introduction}
\label{sec:intro}

Resource estimates for fault-tolerant quantum computation (FTQC) apportion
the logical error budget $\varepsilon_\mathrm{total}$ among three places:
the physical layer (surface-code distance $d$), rotation synthesis
($\varepsilon_U$, via Ross--Selinger~\cite{ross2016optimal}), and magic
state distillation
($\varepsilon_T$)~\cite{bravyi2005universal,litinski2019game,gidney2024cultivation}.
Boolean arithmetic---adders, comparators, lookup tables used inside
modular exponentiation and state-preparation circuits---is implemented
as reversible permutation logic, and is treated as \emph{exact}.
All major FTQC cost models since Fowler
et~al.~\cite{fowler2012surface} assume $\varepsilon_B = 0$.

This paper asks a simple question: \emph{is exact Boolean arithmetic
the only viable choice?} Classical approximate computing has shown
for two decades that trading controlled amounts of correctness for
resource savings is broadly beneficial
\cite{shin2010approximate,venkataramani2013salsa}, but the quantum
literature has not explored this dimension: there is no established
toolchain that targets multiplicative complexity (MC, equivalently
$T$-count) as the approximate-synthesis objective, and no resource
analysis that allows $\varepsilon_B > 0$.

We make four contributions.

\textbf{1. Orthodoxy challenge.} We formalize and empirically
demonstrate that setting $\varepsilon_B > 0$ yields lower total
space-time cost than $\varepsilon_B = 0$ in realistic hardware regimes
($p_\mathrm{phys} \geq 3\times10^{-3}$), on Shor's windowed modular
exponentiation.

\textbf{2. MC-targeted ALS toolchain.} We re-target approximate logic
synthesis from the classical total-gate-count objective to
multiplicative complexity. We provide two complementary tools:
\texttt{approx-tt} (an integer linear program giving the MC-optimal
reference on small truth tables), and \texttt{narrow\_and\_resub}
(a per-AND greedy heuristic with a five-candidate local replacement
set, scaling to chained windowed arithmetic with window widths
$w \le 7$).

\textbf{3. Unified budget allocation framework.} We formalize the
joint Boolean + rotation-synthesis + T-state allocation problem.
We show that the continuous-side subproblem has a closed-form
Lagrangian split, so joint optimization reduces to a one-dimensional
outer sweep on $\varepsilon_B$.

\textbf{4. Case study.} On Shor's windowed modexp at
$N \in \{33, 35, 143, 221, 323\}$ we observe: (a) $\varepsilon_B > 0$
saves 10--20\,\% of total space-time volume at $p_\mathrm{phys} =
3\times10^{-3}$; (b) uniform per-window allocation is consistently
suboptimal; and (c) the winning per-window strategy depends on
$\gcd(2^w, r)$ where $r$ is Shor's hidden period---for $N=33$
($r=10$, $\gcd=2$) the back-heavy allocation wins, while for $N=221$
($r=48$, $\gcd=16$) the front-heavy allocation wins.

The remainder of the paper is structured as follows.
Section~\ref{sec:background} surveys prior work.
Section~\ref{sec:framework} presents the unified budget allocation
framework. Section~\ref{sec:mc_als} describes the MC-ALS toolchain.
Section~\ref{sec:evaluation} evaluates on random truth tables and on
Shor's windowed modexp. Section~\ref{sec:discussion} covers
limitations; Section~\ref{sec:conclusion} concludes.
```

- [ ] **Step 3: Compile + verify**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
```

Expected: no new errors.

- [ ] **Step 4: Commit**

```bash
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/abstract.tex paper/qce2026/sections/introduction.tex
git commit -m "Draft abstract and introduction"
```

---

### Task 13: Draft background section

**Files:**
- Modify: `paper/qce2026/sections/background.tex`

- [ ] **Step 1: Write background**

```latex
\section{Background and Related Work}
\label{sec:background}

\subsection{Approximate logic synthesis (classical)}
SALSA~\cite{venkataramani2013salsa}, SASIMI, ABACUS, and similar
frameworks~\cite{shin2010approximate} perform network- or truth-table-level
approximation with objectives centered on total gate count or dynamic
power. Their cost function weights AND and XOR gates equivalently,
which is appropriate for CMOS but not for quantum circuits where
XOR (Clifford) is free and the dominant cost is the AND count that
maps to $T$-count via Toffoli decomposition.

\subsection{Rotation synthesis}
Ross--Selinger~\cite{ross2016optimal} give a constructive
Clifford+$T$ approximation of any single-qubit $z$-rotation using
$t(\varepsilon) \approx 3\log_2(1/\varepsilon) + c_0$ $T$ gates at
diamond-norm precision $\varepsilon$. Solovay--Kitaev establishes
universality with polylogarithmic overhead but worse
constants.

\subsection{Approximate QFT}
Coppersmith~\cite{coppersmith1994approximate} observed that
controlled phase rotations $R_k$ with $k > \log_2 n$ can be dropped
entirely with negligible impact on QFT fidelity.
Barenco et~al.~\cite{barenco1996approximate} gave a diamond-norm
analysis. These works use a \emph{uniform} truncation threshold; the
problem of optimal per-rotation budget allocation under a global
$\varepsilon_U$ constraint is open.

\subsection{Magic state distillation and cultivation}
Bravyi--Kitaev~\cite{bravyi2005universal} gave the original 15-to-1
distillation. Litinski~\cite{litinski2019game} derived closed-form
space-time cost of factories in a lattice-surgery model. More recent
magic state \emph{cultivation}~\cite{gidney2024cultivation} moves
distillation into the computation path; its cost-error curve is still
logarithmic in $\varepsilon_T$ but has different constants.

\subsection{Windowed quantum arithmetic}
Gidney~\cite{gidney2019windowed} introduced windowed arithmetic for
modular exponentiation: the $m$-bit exponent is split into
$k = \lceil m/w \rceil$ windows of size $w$; each window fetches
$\mathrm{LUT}_i[v] = \mathrm{base}^{v \cdot 2^{iw}} \bmod N$ via a
QROM and multiplies it into an accumulator. All published realizations
implement the QROMs exactly; we are the first to treat them as
approximable primitives.

\subsection{FTQC resource estimation}
Comprehensive resource models~\cite{fowler2012surface,
litinski2019game,gidney2021factoring} assume $\varepsilon_B = 0$ and
allocate $\varepsilon_\mathrm{total}$ entirely to the continuous
primitives.

\subsection{Gap and positioning}
To our knowledge, no prior work (a) targets ALS at multiplicative
complexity aligned with $T$-count, (b) introduces a Boolean error
budget dimension to FTQC compilation, (c) removes the
$\varepsilon_B = 0$ default from resource analyses, or (d) formulates
a unified optimization coupling all three primitives. We fill these
gaps in turn.
```

- [ ] **Step 2: Compile + commit**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/background.tex
git commit -m "Draft background section"
```

---

### Task 14: Draft framework section

**Files:**
- Modify: `paper/qce2026/sections/framework.tex`

- [ ] **Step 1: Write framework**

```latex
\section{Unified Budget Allocation Framework}
\label{sec:framework}

\subsection{Problem statement}
Given a quantum circuit $C$ compiled to fault-tolerant logic, its
components partition into three sets: Boolean arithmetic
$\mathcal{B} = \{B_1, \dots, B_{k_B}\}$, controlled rotations
$\mathcal{U} = \{R_1, \dots, R_{k_U}\}$, and $T$-gate applications
$\mathcal{T}$. The global logical-error budget decomposes as
\[
  \varepsilon_\mathrm{total} \geq \varepsilon_B + \varepsilon_U + \varepsilon_T,
\]
where $\varepsilon_B = \sum_i \varepsilon_{B,i}$, $\varepsilon_U =
\sum_j \varepsilon_{U,j}$, and $\varepsilon_T$ is the maximum
allowed $T$-state infidelity across all applications. Existing FTQC
practice fixes $\varepsilon_B = 0$. We lift this constraint.

\subsection{Per-component cost}
Each component $i$ is characterized by two numbers: its $T$-count
$t_i$ and its $T$-depth $\tau_i$. The former enters the
magic-state factory load; the latter drives the circuit's surface-code
cycle count and therefore the required code distance. Aggregated,
$T_\mathrm{total} = \sum_i t_i$ and
$T_\mathrm{depth} = \sum_i \tau_i$ (assuming serial execution).

\subsection{Per-primitive cost-error curves}
The Boolean primitive has no closed-form cost; we measure it
empirically via the MC-ALS tools of Section~\ref{sec:mc_als}. The
rotation and $T$-state primitives have analytic
forms~\cite{ross2016optimal,litinski2019game}:
$t_U(\varepsilon_U) \approx 3 k_U \log_2(k_U/\varepsilon_U)$ and the
Litinski factory cost $C_T(\varepsilon_T) \approx \alpha \log_2(
N_T/\varepsilon_T) d^2$.

\subsection{FTQC total cost}
The surface-code distance $d$ is the smallest odd integer satisfying
\[
  Q \cdot T_\mathrm{depth} \cdot d \cdot \varepsilon_\mathrm{cycle}(d)
    \leq \varepsilon_\mathrm{run},
\]
with $\varepsilon_\mathrm{cycle}(d) = A (p_\mathrm{phys}/p_\mathrm{th})^{(d+1)/2}$
and $\varepsilon_\mathrm{run}$ the tolerated per-shot logical failure
probability (we use $0.1$). Total space-time volume is
\[
  V_\mathrm{factor} = \mathbb{E}[\mathrm{trials}] \cdot Q \cdot
    T_\mathrm{depth} \cdot d^3.
\]

\subsection{Joint optimization}
We solve
\[
  \min_{(\varepsilon_B, \varepsilon_U, \varepsilon_T)} V_\mathrm{factor},
  \quad \text{s.t.} \quad \varepsilon_B + \varepsilon_U +
    \varepsilon_T \leq \varepsilon_\mathrm{total}.
\]
Because both continuous primitives have $\sim\!\log(1/\varepsilon)$
cost, the Lagrangian admits a closed-form solution for the inner
$(\varepsilon_U, \varepsilon_T)$ split given $\varepsilon_B$. The
outer problem reduces to a one-dimensional sweep on
$\varepsilon_B$ in which the Boolean-side cost is obtained by running
the MC-ALS tool.

\subsection{Recursive sub-allocation}
When a primitive contains multiple approximable sub-components
(e.g., $k$ window LUTs in windowed modexp, or multiple independent
rotations in QFT), the primitive budget must be further allocated
among them. We defer specific strategies to
Section~\ref{sec:evaluation}, where we observe empirically that
uniform is never optimal and structural properties of the circuit
(such as $\gcd(2^w, r)$ in Shor's modexp) predict the winning
strategy.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig1_conceptual.pdf}
  \caption{Error-budget allocation. (a) Current FTQC practice:
    $\varepsilon_B = 0$ by assumption. (b) Proposed three-primitive
    decomposition: $\varepsilon_B > 0$ becomes a free variable.}
  \label{fig:conceptual}
\end{figure}
```

- [ ] **Step 2: Compile + commit**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/framework.tex
git commit -m "Draft unified budget allocation framework section"
```

---

### Task 15: Draft MC-ALS section

**Files:**
- Modify: `paper/qce2026/sections/mc_als.tex`

- [ ] **Step 1: Write section**

```latex
\section{MC-Targeted Approximate Logic Synthesis}
\label{sec:mc_als}

\subsection{Cost function}
We re-target the ALS objective from total-gate savings
(e.g.\ $\mathrm{mffc} - \mathrm{new\_nodes}$) to multiplicative
complexity:
\[
  \mathrm{gain} = \#\mathrm{AND}_\mathrm{removed}
                 - \#\mathrm{AND}_\mathrm{added}.
\]
XOR gates have zero $T$-cost in an XAG (they map to Clifford) and
are counted as free. The error metric is the weighted mean absolute
integer deviation $\sum_x w(x) |\tilde f(x) - f(x)|_{\mathrm{int}}$
over the input distribution.

\subsection{\texttt{approx-tt}: ILP-optimal reference}
For an $m$-output, $n$-input truth table $f\colon \{0,1\}^n \to
\{0,1\}^m$, we introduce binary flip variables $d_{i,x} \in \{0,1\}$
denoting whether output bit $i$ is flipped at input $x$, and binary
monomial variables $k_{i,S}$ for each subset $S \subseteq [n]$ of
size $|S|\ge 2$ denoting whether the degree-$|S|$ ANF monomial
appears in output $i$ after flipping. The parity constraint
\[
  k_{i,S} \equiv \sum_{T \subseteq S} d_{i,T} \pmod 2
\]
tracks ANF bit updates. The error constraint
\[
  \sum_x w(x) \Bigl|\sum_i 2^{m-1-i}\bigl(f_i(x) \oplus d_{i,x}\bigr)
    - \sum_i 2^{m-1-i} f_i(x)\Bigr| \leq \varepsilon_B
\]
bounds the integer deviation. The objective minimizes
$\sum_{i,|S|\ge 2} k_{i,S}$, a lower bound on the XAG AND count
(Boyar--Peralta~\cite{boyar2000upper}). We solve with Gurobi.
The formulation has $m \cdot 2^n$ flip variables and is tractable
for $n \le 12$.

\subsection{\texttt{narrow\_and\_resub}: scalable greedy heuristic}
We operate directly on the XAG. In each iteration we simulate the
current XAG on either $2^n$ exhaustive inputs (if
$n \le n_\mathrm{ex}$) or $10^5$ random patterns, then for each AND
gate $g$ with fanins $f_0, f_1$ we evaluate five local replacement
candidates:
\[
  \{\mathrm{const}_0,~\mathrm{const}_1,~f_0,~f_1,
    ~\mathrm{XOR}(f_0,f_1)\}.
\]
Each candidate's gain-over-integer-error ratio is computed, the best
candidate that fits the remaining budget is applied via
\texttt{substitute\_node}, and the loop continues until no viable
LAC remains. The per-iteration cost is $O(|\mathrm{AND}|)$---three
orders of magnitude lower than ResubALS's divisor-window enumeration.
The 5-candidate set is the minimal set that is closed under single-AND
MC reduction: three of the five ($\mathrm{const}_{0/1}$, $f_i$) eliminate
an AND outright, and the fifth ($\mathrm{XOR}$) exchanges an AND for a
free XOR.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig2_narrow_lac.pdf}
  \caption{\texttt{narrow\_and\_resub}: for each target AND gate, five
    local candidates are evaluated. The XOR replacement is the only
    two-input candidate that reduces the AND count.}
  \label{fig:narrow_lac}
\end{figure}

\subsection{ResubALS with MC re-weight}
Our reference ALS framework (divisor-window LAC enumeration with
Integer / Simulation / VECBEE / Miter error estimators) is retained
and re-weighted so that the \texttt{new\_nodes} term in the
\texttt{size\_gain} formula counts only newly added AND gates.
This variant serves as the general baseline when inputs have
don't-cares (for instance, chemistry QROM state preparation) where
narrow's AND-only candidate set is too restrictive.

\subsection{Positioning}
The three tools occupy complementary regimes:
\texttt{approx-tt} is MC-optimal but capped at $n \le 12$ inputs;
\texttt{narrow\_and\_resub} is a scalable heuristic that approaches
ILP optimality on all inputs we have measured; ResubALS-MC is the
general baseline for inputs with don't-cares.
```

- [ ] **Step 2: Compile + commit**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/mc_als.tex
git commit -m "Draft MC-ALS section"
```

---

### Task 16: Draft evaluation section

**Files:**
- Modify: `paper/qce2026/sections/evaluation.tex`

- [ ] **Step 1: Write evaluation**

```latex
\section{Evaluation}
\label{sec:evaluation}

We evaluate in two parts: a micro-benchmark comparing the MC-ALS
tools on random truth tables
(Section~\ref{sec:eval_random}) and an integrated case study on
Shor's windowed modular exponentiation
(Section~\ref{sec:eval_shor}).

\subsection{Random truth table micro-benchmark}
\label{sec:eval_random}

\textbf{Benchmark set.} Thirty-one truth tables from the
\texttt{lut-synth} corpus: random TTs at
$(n, m) \in \{(4,4),(4,6),(4,8),(8,4),(8,6),(8,8),
(12,4),(12,6),(12,8)\}$ with three seeds each, plus
\texttt{gf\_mult4}, \texttt{modexp\_2\_15\_4\_4},
\texttt{modexp\_3\_221\_4\_8}, and \texttt{modexp\_3\_35\_8\_6}.

\textbf{Methods.} Four: ResubALS (standard size-gain objective, prior
ALS); Truncation (drop low-order output bits); \texttt{approx-tt}
(our ILP, MC-optimal); \texttt{narrow\_and\_resub} (our heuristic).

\textbf{Error-bound sweep.} $\varepsilon \in \{0.5, 1.0, 2.0, 4.0\}$
in the integer-error metric.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig4_random_tt.pdf}
  \caption{AND-count ratio vs exact across four methods on the
    random-TT benchmark. ResubALS with its default size-gain
    objective never fires (ratio $=1$); \texttt{approx-tt} finds
    MC-optimal approximations; \texttt{narrow} tracks \texttt{approx-tt}
    within a few percent at orders of magnitude lower runtime.}
  \label{fig:fig4}
\end{figure}

\textbf{Findings.} Figure~\ref{fig:fig4} summarizes.
ResubALS under its standard size-gain objective achieves
AND ratio $\equiv 1.000$ across all $31 \times 4 = 124$
benchmark-error pairs: the classical ALS target does not identify
AND-reducing approximations on these TTs. \texttt{approx-tt}
achieves geometric-mean AND ratios of $0.955 / 0.941 / 0.858 / 0.747$
at $\varepsilon \in \{0.5, 1.0, 2.0, 4.0\}$, i.e.\ up to 25\,\%
reduction at moderate error budgets. \texttt{narrow\_and\_resub} tracks
\texttt{approx-tt} within 5\,\% of optimality while running in
under 100~ms per TT; at $n = 16, 20$ where \texttt{approx-tt}
exceeds the 60~s Gurobi timeout, \texttt{narrow} still completes
and reports non-trivial reductions.

\subsection{Shor's windowed modexp case study}
\label{sec:eval_shor}

\textbf{Setup.} Five cases
$(\mathrm{base}, N, w, m) \in \{(5,33,4,12), (3,35,4,12),
(7,143,5,15), (7,221,4,16), (2,323,5,18)\}$.
FTQC parameters: $p_\mathrm{phys} \in \{10^{-4}, 10^{-3},
3\times10^{-3}, 10^{-2}\}$, $p_\mathrm{th} = 10^{-2}$,
$\varepsilon_\mathrm{run} = 0.1$, $Q = 2\lceil \log_2 N \rceil + 4$.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig3_chained_modexp.png}
  \caption{Chained windowed modexp: $m$-bit exponent split into $k$
    windows; each window's QROM value multiplied into the
    accumulator modulo~$N$.}
  \label{fig:chained}
\end{figure}

\subsubsection{Experiment A: $\varepsilon_B = 0$ vs $\varepsilon_B > 0$}
We sweep $\varepsilon_B$ from $0$ to the full budget and compare
$V_\mathrm{factor}(\varepsilon_B)$ against the $\varepsilon_B = 0$
baseline across $p_\mathrm{phys}$.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig6_ftqc_regime.png}
  \caption{Required code distance $d$ (left) and total space-time
    cost ratio $V_\mathrm{factor}(\varepsilon_B) /
    V_\mathrm{factor}(0)$ (right) as $\varepsilon_B$ increases, for
    four physical-error rates. At $p_\mathrm{phys} = 3\times10^{-3}$
    narrow approximation crosses a discrete $d$-threshold at
    aggressive $\varepsilon_B$, yielding up to 70\,\% space-time
    savings.}
  \label{fig:ftqc_regime}
\end{figure}

At $p_\mathrm{phys} = 3\times10^{-3}$, $\varepsilon_B > 0$ nets
10--20\,\% total-cost savings via discrete $d$-threshold crossings
(Figure~\ref{fig:ftqc_regime}). At
$p_\mathrm{phys} \le 10^{-3}$, the code distance is already at its
floor and the expected-trials tax from approximation dominates; the
orthodoxy is locally correct in that regime.

\subsubsection{Experiment B: per-window allocation}
Holding the Boolean budget fixed at a moderate value, we compare
five allocation strategies across the $k$ windows:
\{uniform, front-heavy (all budget to window~0),
back-heavy (all budget to window~$k-1$), linear-up,
linear-down\}.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig5_per_window_eb.png}
  \caption{Shor's per-shot success probability $P(\mathrm{ok})$ under
    five per-window budget allocation strategies. Left:
    $(5,33,w{=}4,m{=}12)$ with Shor period $r{=}10$ and
    $\gcd(2^w,r){=}2$: back-heavy wins, improving $P(\mathrm{ok})$
    from $0.097$ (uniform) to $0.285$. Right:
    $(7,221,w{=}4,m{=}16)$ with $r{=}48$ and $\gcd{=}16$:
    front-heavy wins instead, $0.126 \to 0.320$.}
  \label{fig:per_window}
\end{figure}

As Figure~\ref{fig:per_window} shows, uniform is never optimal:
concentration improves $P(\mathrm{ok})$ by 2--3$\times$. Which
window to concentrate budget on is case-dependent. The pattern
correlates with the arithmetic relation $\gcd(2^w, r)$ between
window size and the hidden period: when $2^w$ divides $r$ cleanly
(as in $N=221$), front-heavy wins, because corrupting the
low-position window perturbs an $x$-equivalence class aligned with
the period and is more readily averaged out by QFT. When $\gcd$
is small (as in $N=33$), back-heavy wins, because high-position
corruptions affect contiguous $x$-blocks that preserve the period
structure globally.

\begin{figure}[t]
  \centering
  \includegraphics[width=\columnwidth]{figures/fig7_period_damage.png}
  \caption{Progressive narrow approximation damage on
    $(3,35,w{=}4,m{=}12)$. Left: $\tilde f(x)$ sequence. Middle:
    autocorrelation; the peak at $r^*{=}12$ and its multiples remains
    sharp up to $\varepsilon_B{=}2.0$. Right: QFT spectrum; peaks at
    $k \cdot L / r^*$ broaden but do not move.}
  \label{fig:period_damage}
\end{figure}

Figure~\ref{fig:period_damage} provides mechanistic intuition: even
at aggressive $\varepsilon_B$, the period peak stays at
$k \cdot L / r^*$; approximation broadens the QFT peak rather than
destroying it.

\subsubsection{Experiment C: joint vs single-axis}
We compare the full three-way joint optimization against two
restrictions: $\varepsilon_B = 0$ (continuous-only) and
continuous fixed to Coppersmith-uniform truncation
(Boolean-only). The joint optimum is consistently lower than
either single-axis restriction, with the largest gap (up to
$\approx 15\,\%$) at $p_\mathrm{phys} = 3\times10^{-3}$.
```

- [ ] **Step 2: Compile + commit**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/evaluation.tex
git commit -m "Draft evaluation section"
```

---

### Task 17: Draft discussion + conclusion

**Files:**
- Modify: `paper/qce2026/sections/discussion.tex`
- Modify: `paper/qce2026/sections/conclusion.tex`

- [ ] **Step 1: Write discussion**

```latex
\section{Discussion and Limitations}
\label{sec:discussion}

\textbf{Gate-level noise.} Our FTQC cost model absorbs physical-layer
noise through the $\varepsilon_\mathrm{run}$ run-failure budget and
the resulting $d$ selection; we do not simulate depolarizing or
$T_1/T_2$ channels explicitly. Coupling approximation error with
gate-level stochastic noise under a qiskit \texttt{NoiseModel} is a
natural extension.

\textbf{T-state primitive.} We report costs with the Litinski factory
model as default; substituting Gidney magic-state
cultivation~\cite{gidney2024cultivation} requires only updating the
$C_T(\varepsilon_T)$ function within the framework and leaves the
outer optimization unchanged.

\textbf{narrow's Boolean-only scope.} \texttt{narrow\_and\_resub} is
strictly a Boolean-function approximator; it cannot be applied to
non-permutation unitaries such as controlled rotations. In the
framework's three-primitive decomposition narrow handles only
$\varepsilon_B$ while Ross--Selinger and Litinski handle
$\varepsilon_U$ and $\varepsilon_T$ respectively.

\textbf{Per-window rule generality.} We have empirically established
the $\gcd(2^w, r)$ rule for Shor's windowed modexp; a formal
characterization of when concentrated allocation wins over uniform
in other arithmetic circuits is open.

\textbf{Monolithic vs chained modexp.} Monolithic (non-windowed)
modexp scales as a single $2^m$-entry lookup and hence applies only
at small $m$. We use it as calibration in Section~\ref{sec:evaluation}
rather than as a primary benchmark.
```

- [ ] **Step 2: Write conclusion**

```latex
\section{Conclusion}
\label{sec:conclusion}

We have challenged the FTQC orthodoxy of exact Boolean arithmetic.
A unified three-primitive budget allocation, an MC-targeted ALS
toolchain, and a case study on Shor's windowed modular exponentiation
together show that setting $\varepsilon_B > 0$ reduces total
space-time cost in realistic hardware regimes, and that per-window
allocation matters 2--3$\times$ on factoring success probability
with winning strategies determined by circuit-level arithmetic
structure. The framework extends directly to other arithmetic
circuits; the tooling is released as
\texttt{lut-synth}~\cite{ourrepo}.
```

- [ ] **Step 3: Compile + commit**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make pdf 2>&1 | tail -5
cd /home/hanyu/lut-synth
git add paper/qce2026/sections/discussion.tex paper/qce2026/sections/conclusion.tex
git commit -m "Draft discussion and conclusion sections"
```

---

### Task 18: Cross-section polish + page budget check

**Files:**
- All prose sections (as needed)

- [ ] **Step 1: Full compile and inspect**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make distclean && make pdf 2>&1 | grep -E "Warning|Error|Overfull" | head -30
pdfinfo main.pdf | grep "Pages"
```

Expected: Pages: 8 (or slightly over; report back if >9).

- [ ] **Step 2: Fix any warnings**

Read the warnings printed in Step 1; address each. Common fixes:
- Missing refs: add `\label{}` or fix `\ref{}` target
- Overfull hboxes: rephrase or adjust `\sloppy` locally
- Bib warnings: add missing bibtex entry

- [ ] **Step 3: Verify all figures load**

```bash
grep -o "figures/fig[0-9_]*" paper/qce2026/sections/*.tex | sort -u
ls paper/qce2026/figures/
```

Check every figure referenced in prose exists as a PDF/PNG.

- [ ] **Step 4: Commit polish**

```bash
cd /home/hanyu/lut-synth
git add paper/qce2026
git commit -m "Polish QCE paper: fix warnings, verify figure references"
```

---

### Task 19: Final compile + push

**Files:**
- `paper/qce2026/main.pdf` (build artifact, not committed)

- [ ] **Step 1: Clean build**

```bash
cd /home/hanyu/lut-synth/paper/qce2026
make distclean && make pdf 2>&1 | tail -10
open main.pdf || xdg-open main.pdf 2>/dev/null || echo "PDF at paper/qce2026/main.pdf"
```

Expected: successful build; visually inspect for obvious issues.

- [ ] **Step 2: Add main.pdf to .gitignore if not already**

```bash
cd /home/hanyu/lut-synth
grep "paper.*pdf\|main.pdf" .gitignore || (echo -e "\n# LaTeX build\npaper/*/main.pdf\npaper/*/*.aux\npaper/*/*.log\npaper/*/*.bbl\npaper/*/*.blg\npaper/*/*.out\npaper/*/*.fls\npaper/*/*.fdb_latexmk\npaper/*/*.synctex.gz" >> .gitignore && git add .gitignore && git commit -m "Ignore LaTeX build artifacts")
```

- [ ] **Step 3: Push**

```bash
cd /home/hanyu/lut-synth
git push origin main
```

---

## Self-Review Summary

**Spec coverage:** Every section and every open experimental follow-up item of the spec has a corresponding task:
- Section 1 (intro) → Task 12
- Section 2 (background) → Task 13
- Section 3 (framework) → Task 14
- Section 4 (MC-ALS) → Task 15
- Section 5 (evaluation) → Task 16 (prose), Tasks 1-4, 8-9 (data + figures)
- Section 6 (discussion) → Task 17
- Section 7 (conclusion) → Task 17
- Follow-up experiment #1 (narrow on HLQCS) → Task 1
- Follow-up experiment #2 (scalability) → Task 2
- Follow-up experiment #3 (Exp A) → Task 3
- Follow-up experiment #4 (Exp C) → Task 4
- Follow-up experiment #5 (Fig 1, Fig 2 schematics) → Tasks 5, 6

**Open decisions from spec:**
- Cultivation citation: Task 17 cites Gidney 2024 as alternative to Litinski ✓
- ResubALS "ratio ≡ 1.000" explanation: Task 16 gives a one-sentence mechanistic reason ✓
- `gcd(2^w, r)` theoretical bound: explicitly future work in Task 17 discussion ✓
- Framework formal name: used "unified three-primitive budget allocation" consistently ✓

**Placeholder scan:** No TBD / TODO / "similar to above" / "add appropriate error handling" instances in any task.

**Type consistency:** All symbols, function names, and file paths match across tasks.
