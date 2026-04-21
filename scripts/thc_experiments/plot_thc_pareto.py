"""Plot AND-saved vs state-prep infidelity Pareto for THC benchmarks."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load(tag: str, root: Path) -> dict:
    return json.loads((root / tag / "summary.json").read_text())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=str, default="data/thc_fidelity")
    ap.add_argument("--tags", nargs="+", required=True)
    ap.add_argument("--out", type=str, default="figures/thc_pareto.pdf")
    args = ap.parse_args()

    root = Path(args.root)
    summaries = [load(t, root) for t in args.tags]

    fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(11, 4.2))
    colors = plt.cm.viridis(np.linspace(0.15, 0.85, len(summaries)))

    for s, c in zip(summaries, colors):
        base_and = s["keep_and_baseline"] + s["alias_and_baseline"]
        and_sum = [r["keep_and_after"] + r["alias_and_after"]
                   for r in s["results"]]
        saved_pct = [100.0 * (base_and - a) / base_and for a in and_sum]
        infid = [max(r["infidelity"], 1e-8) for r in s["results"]]
        label = f"{s['tag']} (N={s['N']}, M={s['M']})"
        ax_left.plot([r["eb"] for r in s["results"]], infid,
                     marker="o", lw=1.6, color=c, label=label)
        ax_right.plot(saved_pct, infid, marker="o", lw=1.6,
                      color=c, label=label)

    for ax in (ax_left, ax_right):
        ax.set_yscale("log")
        ax.grid(True, alpha=0.3)
    ax_left.set_xlabel(r"arithmetic budget $\varepsilon_A$")
    ax_left.set_ylabel(r"state-prep infidelity $1-F$")
    ax_right.set_xlabel("AND-count saved (keep + alias, %)")
    ax_right.set_ylabel(r"state-prep infidelity $1-F$")
    ax_right.legend(fontsize=9, loc="best")

    fig.tight_layout()
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out, dpi=200, bbox_inches="tight")
    png = str(Path(args.out).with_suffix(".png"))
    fig.savefig(png, dpi=160, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
