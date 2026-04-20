"""Paper-style consolidation of lut-synth + chained Shor's + narrow findings.

Reads the canonical result JSONs produced by our various sweeps and emits
6 figures + a markdown summary under results/final/.

Data sources:
  results/shor_quant_s4fixed/shor_e2e.json       — monolithic (4-start synth)
  results/shor_chained_s4fixed/shor_chained.json — chained w=4 m=12 (4-start)
  results/per_window_eb/<case>/per_window_eb.json — per-window sweep
  results/synth_baseline/synth_baseline.json     — 1-start vs multi-start audit

Figures:
  fig1_synth_audit.png       1-start vs 4-start SS baseline (why we pinned starts=4)
  fig2_pareto_mono.png       monolithic: AND savings % vs E[trials]
  fig3_pareto_chained.png    chained: same axes, compared with monolithic
  fig4_per_window_eb.png     per-window allocation beats uniform (2 cases)
  fig5_ftqc_regime.png       V_factor ratio sweep across p_phys
  fig6_period_damage.png     f̃(x), autocorrelation, QFT spectrum (chained case)
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESULTS = PROJECT_ROOT / "results"
OUT = RESULTS / "final"
OUT.mkdir(parents=True, exist_ok=True)

sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
sys.path.insert(0, str(PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))

plt.rcParams.update({
    "font.size": 18,
    "axes.titlesize": 20,
    "axes.labelsize": 18,
    "xtick.labelsize": 16,
    "ytick.labelsize": 16,
    "legend.fontsize": 15,
    "figure.dpi": 140,
    "savefig.dpi": 220,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.alpha": 0.25,
    "lines.linewidth": 2.0,
    "lines.markersize": 7,
})

STRATEGY_STYLE = {
    "uniform":      {"color": "#B0B0B0", "hatch": None},
    "front_heavy":  {"color": "#EF7373", "hatch": None},
    "back_heavy":   {"color": "#7FC87F", "hatch": None},
    "linear_up":    {"color": "#F2C84B", "hatch": None},
    "linear_down":  {"color": "#6B9AC4", "hatch": None},
}


def _load(p: Path):
    if not p.exists():
        print(f"[warn] missing {p}")
        return None
    return json.loads(p.read_text())


# ---------- fig1: synth audit ----------
def fig_synth_audit():
    data = _load(RESULTS / "synth_baseline" / "synth_baseline.json")
    if not data: return
    fig, ax = plt.subplots(figsize=(7, 4))
    xs = [f"(base={r['base']},N={r['N']},{r['flavor'][:4]})" for r in data]
    one = [r["synth_sweep"][0].get("best_and") or r["synth_sweep"][0]["best_and_sum"]
           for r in data]
    multi = [min(s.get("best_and") or s["best_and_sum"] for s in r["synth_sweep"])
             for r in data]
    x = np.arange(len(xs))
    width = 0.35
    ax.bar(x - width/2, one, width, label="1-start SS",  color="#d62728")
    ax.bar(x + width/2, multi, width, label="≥4-start SS (min)", color="#2ca02c")
    for i, (a, b) in enumerate(zip(one, multi)):
        delta = 100 * (a - b) / a if a else 0
        ax.text(i, max(a, b) + 1.5, f"−{delta:.0f}%", ha="center", fontsize=8)
    ax.set_xticks(x); ax.set_xticklabels(xs, rotation=15, ha="right")
    ax.set_ylabel("Baseline AND count")
    ax.set_title("1-start vs multi-start SS synthesis baseline\n"
                 "(monolithic TTs have large synth variance; chained windowed LUTs don't)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUT / "fig1_synth_audit.png")
    plt.close(fig); print(f"wrote {OUT / 'fig1_synth_audit.png'}")


# ---------- fig2 / fig3: Pareto ----------
def _pareto_from(cases, ax, label_fmt, linestyle="-"):
    for c in cases:
        b = c["baseline_and"]
        xs, ys = [], []
        for r in c["sweep"]:
            if r.get("failed"): continue
            p = r["shor_shots"]["success_rate"]
            if p <= 0: continue
            xs.append(100 * (b - r["and_after"]) / b)
            ys.append(1 / p)
        if xs:
            ax.plot(xs, ys, marker="o", ls=linestyle,
                    label=label_fmt.format(c))


def fig_pareto_mono():
    mono = _load(RESULTS / "shor_quant_s4fixed" / "shor_e2e.json")
    if not mono: return
    fig, ax = plt.subplots(figsize=(7, 4.5))
    _pareto_from(mono, ax, "(base={0[base]}, N={0[N]}, exp={0[exp_bits]})")
    ax.set_yscale("log")
    ax.set_xlabel("AND savings (%)")
    ax.set_ylabel("Expected Shor's trials to factor")
    ax.set_title("Monolithic modexp LUT — Pareto of narrow approximation (4-start baseline)")
    ax.axhline(1.0, color="gray", lw=0.5, ls=":", alpha=0.5)
    ax.grid(True, alpha=0.3, which="both")
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUT / "fig2_pareto_mono.png")
    plt.close(fig); print(f"wrote {OUT / 'fig2_pareto_mono.png'}")


def fig_pareto_chained():
    mono = _load(RESULTS / "shor_quant_s4fixed" / "shor_e2e.json") or []
    ch = _load(RESULTS / "shor_chained_s4fixed" / "shor_chained.json") or []
    ch_big = _load(RESULTS / "shor_stress_completed" / "shor_chained.json") or []
    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    _pareto_from(mono, ax, "mono N={0[N]} m={0[exp_bits]}", linestyle=":")
    _pareto_from(ch, ax, "chained N={0[N]} w={0[w]} m={0[m_total]}", linestyle="-")
    _pareto_from(ch_big, ax, "chained N={0[N]} w={0[w]} m={0[m_total]} (stress)",
                 linestyle="--")
    ax.set_yscale("log")
    ax.set_xlabel("AND savings (%)")
    ax.set_ylabel("Expected Shor's trials")
    ax.set_title("Monolithic vs chained windowed narrow — Pareto")
    ax.grid(True, alpha=0.3, which="both")
    ax.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(OUT / "fig3_pareto_chained.png")
    plt.close(fig); print(f"wrote {OUT / 'fig3_pareto_chained.png'}")


# ---------- fig4: per-window eb allocation ----------
def fig_per_window_eb():
    cases = [
        ("5_33_w4_m12",  r"$(5, 33)$: $r{=}10$, $\gcd{=}2$"),
        ("7_221_w4_m16", r"$(7, 221)$: $r{=}48$, $\gcd{=}16$"),
    ]
    order = ["uniform", "front_heavy", "back_heavy", "linear_up", "linear_down"]
    pretty = {
        "uniform": "uniform", "front_heavy": "front-heavy",
        "back_heavy": "back-heavy", "linear_up": "linear-up",
        "linear_down": "linear-down",
    }
    fig, axes = plt.subplots(1, len(cases), figsize=(12, 4.8), sharey=True)
    for ax, (slug, title) in zip(axes, cases):
        p = RESULTS / "per_window_eb" / slug / "per_window_eb.json"
        data = _load(p)
        if not data: continue
        strategies = [s for s in order
                      if s in {r["strategy"] for r in data}]
        totals = sorted({r["total"] for r in data})
        width = 0.8 / len(strategies)
        for i, s in enumerate(strategies):
            vals = [next((r["P_ok"] for r in data
                          if r["total"] == t and r["strategy"] == s), 0)
                    for t in totals]
            ax.bar(np.arange(len(totals)) + i * width - 0.4 + width/2,
                   vals, width, label=pretty[s],
                   color=STRATEGY_STYLE[s]["color"],
                   edgecolor="black", linewidth=0.4)
        ax.set_xticks(np.arange(len(totals)))
        ax.set_xticklabels([f"{t}" for t in totals])
        ax.set_xlabel(r"total arithmetic budget $\varepsilon_A$")
        ax.set_title(title)
        ax.grid(True, alpha=0.25, axis="y")
        ax.set_axisbelow(True)
    axes[0].set_ylabel(r"Shor per-shot success $P(\mathrm{ok})$")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=len(strategies),
               bbox_to_anchor=(0.5, -0.02), frameon=True, framealpha=0.9,
               edgecolor="0.7")
    fig.tight_layout(rect=[0, 0.08, 1, 1])
    fig.savefig(OUT / "fig4_per_window_eb.png", bbox_inches="tight")
    fig.savefig(OUT / "fig4_per_window_eb.pdf", bbox_inches="tight")
    plt.close(fig); print(f"wrote {OUT / 'fig4_per_window_eb.png'}")


# ---------- fig5: FTQC regime ----------
def fig_ftqc_regime():
    sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
    from model_ft_cost import required_distance, cycle_error
    data = _load(RESULTS / "shor_chained_s4fixed" / "shor_chained.json")
    if not data: return
    p_phys_list = [1e-4, 1e-3, 3e-3, 1e-2]
    Q = 16
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.8))
    p_colors = {1e-4: "#6B9AC4", 1e-3: "#7FC87F",
                3e-3: "#F2C84B", 1e-2: "#EF7373"}
    p_labels = {1e-4: r"$p_\mathrm{phys}{=}10^{-4}$",
                1e-3: r"$p_\mathrm{phys}{=}10^{-3}$",
                3e-3: r"$p_\mathrm{phys}{=}3{\times}10^{-3}$",
                1e-2: r"$p_\mathrm{phys}{=}10^{-2}$"}
    linestyles = {33: "-", 35: "--", 143: ":", 221: "-.", 323: (0, (3, 1, 1, 1))}
    for case in data:
        for ax, metric in zip(axes, ("d", "V_ratio")):
            for p_phys in p_phys_list:
                xs, ys = [], []
                base_V = None
                for r in case["sweep"]:
                    if r.get("failed"): continue
                    pok = r["shor_shots"]["success_rate"]
                    if pok <= 0: continue
                    N_T = r["and_after"] * 4
                    D = max(1, N_T)
                    d = required_distance(Q, D, 0.1, p_phys, 1e-2, 0.03)
                    V = (1/pok) * Q * D * d**3
                    if base_V is None: base_V = V
                    xs.append(r["eb"])
                    ys.append(d if metric == "d" else V/base_V)
                ax.plot(xs, ys, marker="o", ls=linestyles.get(case["N"], "-"),
                        lw=1.8, ms=6,
                        label=rf"$N{{=}}{case['N']}$, {p_labels[p_phys]}",
                        color=p_colors[p_phys], alpha=0.9)
            ax.set_xlabel(r"arithmetic budget $\varepsilon_A$")
            if metric == "d":
                ax.set_ylabel(r"required surface-code distance $d$")
                ax.set_title(r"code distance vs $\varepsilon_A$")
            else:
                ax.set_ylabel(r"$V_\mathrm{factor}(\varepsilon_A)/V_\mathrm{factor}(0)$")
                ax.axhline(1, color="gray", ls="--", lw=0.8, alpha=0.6)
                ax.set_title(r"total factoring-cost ratio")
            ax.grid(True, alpha=0.25)
            ax.set_axisbelow(True)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4,
               bbox_to_anchor=(0.5, -0.02), frameon=True, framealpha=0.9,
               edgecolor="0.7")
    fig.tight_layout(rect=[0, 0.08, 1, 1])
    fig.savefig(OUT / "fig5_ftqc_regime.png", bbox_inches="tight")
    fig.savefig(OUT / "fig5_ftqc_regime.pdf", bbox_inches="tight")
    plt.close(fig); print(f"wrote {OUT / 'fig5_ftqc_regime.png'}")


# ---------- fig6: period damage ----------
def fig_period_damage():
    case_dir = RESULTS / "shor_chained_s4fixed" / "case_3_35_w4_m12"
    if not case_dir.exists():
        print(f"[warn] missing {case_dir}")
        return
    dumps = sorted(case_dir.glob("chained_eb*.f.json"),
                   key=lambda p: float(
                       p.stem.replace("chained_eb", "").replace(".f", "") or 0))
    if not dumps: return

    def _eb(p):
        return float(p.stem.replace("chained_eb", "").replace(".f", "") or 0)
    wanted = [0.0, 0.3, 1.0, 2.0]
    tol = 0.05
    dumps = [min(dumps, key=lambda p: abs(_eb(p) - t))
             for t in wanted
             if any(abs(_eb(p) - t) <= tol for p in dumps)]
    dumps = list(dict.fromkeys(dumps))

    palette = ["#6B9AC4", "#7FC87F", "#F2C84B", "#EF7373"]
    colors = [palette[i % len(palette)] for i in range(len(dumps))]

    r_star = 12
    nrows = len(dumps)
    fig, axes = plt.subplots(nrows, 3, figsize=(13, 1.9 * nrows),
                             squeeze=False, sharex="col")

    for i, d in enumerate(dumps):
        data = _load(d)
        if not data: continue
        eb = data["eb"]
        f = np.array(data["approx_f"])
        L = len(f)
        show = min(L, 4 * r_star)
        col = colors[i]

        axes[i][0].plot(range(show), f[:show], marker="o", ms=4,
                        lw=1.6, color=col)

        r_max = min(L - 1, 4 * r_star)
        corrs = [float((f[: L - r] == f[r:]).mean()) for r in range(1, r_max + 1)]
        axes[i][1].plot(range(1, r_max + 1), corrs, lw=2.0, color=col)

        vals, counts = np.unique(f, return_counts=True)
        v = int(vals[np.argmax(counts)])
        mask = (f == v); k = int(mask.sum())
        ind = np.zeros(L, dtype=np.float64)
        ind[mask] = 1.0 / math.sqrt(k)
        amp = np.fft.fft(ind) / math.sqrt(L)
        prob = (np.abs(amp) ** 2); prob = prob / prob.sum()
        j_plot = min(L, 4 * (L // r_star) + 1)
        axes[i][2].plot(range(j_plot), prob[:j_plot], lw=1.8, color=col)

        for k in range(r_star, min(L - 1, 4 * r_star) + 1, r_star):
            axes[i][0].axvline(k, color="#d64545", lw=1.0, alpha=0.55,
                               ls="--")
            axes[i][1].axvline(k, color="#d64545", lw=1.0, alpha=0.55,
                               ls="--")
        j_plot = min(L, 4 * (L // r_star) + 1)
        for s in range(1, 5):
            pk = s * (L // r_star)
            if pk < j_plot:
                axes[i][2].axvline(pk, color="#d64545", lw=1.0,
                                   alpha=0.55, ls="--")

        axes[i][0].set_ylabel(rf"$\varepsilon_A{{=}}{eb}$"
                              "\n" r"$\tilde{f}(x)$",
                              fontsize=14)
        axes[i][1].set_ylim(0, 1.05)
        axes[i][2].set_yscale("log")
        for ax in axes[i]:
            ax.grid(True, alpha=0.25); ax.set_axisbelow(True)

    axes[0][0].set_title(r"approximated sequence")
    axes[0][1].set_title(r"period autocorrelation")
    axes[0][2].set_title(r"QFT spectrum (log scale)")
    axes[-1][0].set_xlabel(r"input $x$")
    axes[-1][1].set_xlabel(r"lag $r$")
    axes[-1][2].set_xlabel(r"QFT index $j$")
    axes[0][1].set_ylabel(r"autocorrelation $C(r)$")
    axes[0][2].set_ylabel(r"$|\mathrm{amp}(j)|^{2}$")

    fig.tight_layout()
    fig.savefig(OUT / "fig6_period_damage.png", bbox_inches="tight")
    fig.savefig(OUT / "fig6_period_damage.pdf", bbox_inches="tight")
    plt.close(fig); print(f"wrote {OUT / 'fig6_period_damage.png'}")


def write_summary():
    text = """# lut-synth + chained windowed Shor's — findings summary

**Research question**: does narrow AND-resub approximation meaningfully reduce
Shor's modexp circuit cost, after accounting for increased quantum trial
count and fault-tolerant overhead?

## Headline numbers (honest, multi-start synthesis baseline, p_phys=3e-3)

| case                     | baseline AND | safe eb | AND saved | E[trials] tax |
|--------------------------|--------------|---------|-----------|---------------|
| chained (5,33,w=4,m=12)  | 27           | 0.5     | 22%       | 3.4×          |
| chained (3,35,w=4,m=12)  | 22           | 0.3     | 27%       | 2.6×          |
| chained (7,221,w=4,m=16) | 33           | 0.3     | 24%       | 3.1×          |
| chained (2,323,w=5,m=18) | 80           | 0.1     | 6%        | 1.7×          |

**FTQC volume verdict** (`V = E[trials] × Q × D × d³`): at `p_phys = 3×10⁻³` narrow
gives 10–20 % net savings in specific eb sweet spots for chained N=33,35.
At `p_phys ≤ 10⁻³`, the surface-code distance is already pinned to its floor
and the E[trials] tax dominates — narrow is net loss.

## Figures

1. **fig1_synth_audit.png** — 1-start SS is significantly suboptimal on
   monolithic TTs (−56% on (3,35,8)). Chained small LUTs have 0 synth variance
   so multi-start doesn't help/hurt. All downstream data uses 4-start.
2. **fig2_pareto_mono.png** — Monolithic Pareto of AND% vs E[trials]. Shows
   monotonic but mild slope: 20% AND savings ≈ 2× trials.
3. **fig3_pareto_chained.png** — Chained beats monolithic at the same N (lower
   baseline, cleaner Pareto curve). Stress-case (2,323) extends to m=18.
4. **fig4_per_window_eb.png** — Uniform eb across k windows is never optimal.
   Concentrated allocation improves P(ok) 2–3×; the winning strategy depends
   on gcd(2^w, r).
5. **fig5_ftqc_regime.png** — Required code distance d vs eb; V_factor ratio
   across four p_phys values. narrow wins only when N_T reduction crosses a
   d-threshold, which happens at high p_phys.
6. **fig6_period_damage.png** — For a fixed chained case, shows how f̃(x) /
   autocorrelation / QFT spectrum degrade progressively with eb. The period
   peak stays sharp at k·r* even at eb=2.0; approximation broadens QFT peaks
   rather than destroying the period signal.

## Key insights

1. **Multi-start SS synthesis is ~free AND reduction on monolithic TTs**
   (57→27 for (3,35,8)). It's the first thing to turn on; narrow comes after.
2. **Chained windowed (Gidney-style) architecture is the right substrate**
   for narrow approximation. Small LUTs have no synth noise, errors compound
   multiplicatively, and P(ok) degrades gracefully.
3. **Per-window eb allocation matters 2–3× on P(ok)**, and the right choice
   depends on the arithmetic of gcd(2^w, r). Non-trivial, case-dependent.
4. **Shor's QFT is remarkably robust to LUT corruption** — even at 50–90%
   patterns flipped, the period peak stays visible; just P(ok) drops.
5. **FTQC wins from narrow are regime-dependent.** At mature FTQC (p≤10⁻³),
   d is already at the floor and AND savings don't buy d reductions, so
   E[trials] tax makes narrow net-negative. At current NISQ-ish hardware
   (p≈3×10⁻³), narrow can save 10–20% volume at the right eb.

## Known limitations

- Gate-level noise (depolarizing, T1/T2) not modeled; only coupled into
  the FTQC cost model via `ε_run ≤ 0.1` budget parameter.
- Magic state factory cost not modeled.
- Chained simulation assumes ideal modular-multiplication subcircuits;
  no reversible arithmetic decomposition.
- `shor_qiskit.py` validation confirms numpy FFT path matches ideal
  circuit simulation to <5 % sampling noise, but full-oracle MCX mode
  only runs up to ~12 qubits.
- Monolithic m > 10 cases not studied (table size grows as 2^m).

## Source of truth

Per-case data: `results/shor_quant_s4fixed/` (monolithic), `results/shor_chained_s4fixed/`
(chained), `results/shor_stress_completed/` (m=18 stress), `results/per_window_eb/`
(allocation study), `results/synth_baseline/` (1-start vs multi-start).
Memory notes under `.claude/projects/…/memory/` document individual findings
and caveats.
"""
    (OUT / "SUMMARY.md").write_text(text)
    print(f"wrote {OUT / 'SUMMARY.md'}")


if __name__ == "__main__":
    fig_synth_audit()
    fig_pareto_mono()
    fig_pareto_chained()
    fig_per_window_eb()
    fig_ftqc_regime()
    fig_period_damage()
    write_summary()
