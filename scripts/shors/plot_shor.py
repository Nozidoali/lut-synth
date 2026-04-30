"""Plot Shor's e2e quantitative results.

Reads shor_e2e.json produced by scripts/shor_e2e.py and emits:

  1. pareto.png      — AND-savings % vs expected Shor's trials (per case)
  2. per_case.png    — one subplot per case, eb on x-axis, AND / P(ok) / E[trials]
  3. per_case_snr.png — period-autocorrelation SNR vs eb
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def load(path: Path) -> list[dict]:
    return json.loads(path.read_text())


def _case_label(case: dict) -> str:
    return f"({case['base']},{case['N']},{case['exp_bits']})"


def plot_pareto(cases: list[dict], out: Path) -> None:
    fig, ax = plt.subplots(figsize=(7, 5))
    for case in cases:
        baseline = case["baseline_and"]
        xs, ys = [], []
        for r in case["sweep"]:
            if r.get("failed"):
                continue
            p = r["shor_shots"]["success_rate"]
            if p <= 0:
                continue
            savings = 100.0 * (baseline - r["and_after"]) / baseline
            xs.append(savings)
            ys.append(1.0 / p)
        if not xs:
            continue
        ax.plot(xs, ys, "o-", label=_case_label(case))
    ax.set_xlabel("AND savings (%)")
    ax.set_ylabel("Expected Shor's trials to factor")
    ax.set_yscale("log")
    ax.grid(True, alpha=0.3)
    ax.legend(title="(base, N, exp_bits)", fontsize=8, loc="best")
    ax.set_title("Pareto: Narrow approximation vs Shor's trial cost")
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    print(f"wrote {out}")


def plot_per_case(cases: list[dict], out: Path) -> None:
    n = len(cases)
    cols = 3
    rows = (n + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(5 * cols, 3.2 * rows),
                             squeeze=False)
    for idx, case in enumerate(cases):
        ax = axes[idx // cols][idx % cols]
        ebs = [r["eb"] for r in case["sweep"] if not r.get("failed")]
        and_after = [r["and_after"] for r in case["sweep"] if not r.get("failed")]
        p_ok = [r["shor_shots"]["success_rate"]
                for r in case["sweep"] if not r.get("failed")]
        baseline = case["baseline_and"]

        ax.set_title(
            f"{_case_label(case)}  baseline={baseline} AND, r*={case['exact_period']}",
            fontsize=10)
        color_and = "#1f77b4"
        ax.plot(ebs, and_after, "o-", color=color_and, label="AND count")
        ax.set_xlabel("error bound (eb)")
        ax.set_ylabel("AND count", color=color_and)
        ax.tick_params(axis="y", labelcolor=color_and)
        ax.grid(True, alpha=0.3)

        ax2 = ax.twinx()
        color_p = "#d62728"
        ax2.plot(ebs, p_ok, "s--", color=color_p, label="P(shot → factor)")
        ax2.set_ylabel("P(ok) per shot", color=color_p)
        ax2.tick_params(axis="y", labelcolor=color_p)
        ax2.set_ylim(0, max(0.05, max(p_ok) * 1.15) if p_ok else 1)

    for k in range(n, rows * cols):
        axes[k // cols][k % cols].set_visible(False)
    fig.suptitle("Narrow approximation: AND count (left) vs P(ok) (right) per eb",
                 y=0.995)
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    print(f"wrote {out}")


def plot_per_case_snr(cases: list[dict], out: Path) -> None:
    n = len(cases)
    cols = 3
    rows = (n + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(5 * cols, 3.2 * rows),
                             squeeze=False)
    for idx, case in enumerate(cases):
        ax = axes[idx // cols][idx % cols]
        ebs = [r["eb"] for r in case["sweep"] if not r.get("failed")]
        snr_vals = []
        for r in case["sweep"]:
            if r.get("failed"):
                continue
            s = r["period_snr"]["snr"]
            snr_vals.append(s if not math.isinf(s) else None)
        # replace None/inf with plot marker above max
        finite = [s for s in snr_vals if s is not None]
        max_finite = max(finite) if finite else 1.0
        plot_vals = [s if s is not None else max_finite * 1.2
                     for s in snr_vals]

        ax.plot(ebs, plot_vals, "o-", color="#2ca02c")
        for i, s in enumerate(snr_vals):
            if s is None:
                ax.annotate("∞", (ebs[i], plot_vals[i]), fontsize=9,
                            ha="center", va="bottom")
        ax.axhline(1.0, color="gray", lw=0.7, ls="--", alpha=0.5)
        ax.set_yscale("log")
        ax.set_title(
            f"{_case_label(case)}  r*={case['exact_period']}",
            fontsize=10)
        ax.set_xlabel("error bound (eb)")
        ax.set_ylabel("autocorr SNR (signal/noise)")
        ax.grid(True, alpha=0.3)

    for k in range(n, rows * cols):
        axes[k // cols][k % cols].set_visible(False)
    fig.suptitle("Period autocorrelation SNR vs eb  (higher = true period still visible)",
                 y=0.995)
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    print(f"wrote {out}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "shor_e2e.json")
    ap.add_argument("--outdir", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "plots")
    args = ap.parse_args()

    cases = load(args.input)
    args.outdir.mkdir(parents=True, exist_ok=True)
    plot_pareto(cases, args.outdir / "pareto.png")
    plot_per_case(cases, args.outdir / "per_case.png")
    plot_per_case_snr(cases, args.outdir / "per_case_snr.png")


if __name__ == "__main__":
    main()
