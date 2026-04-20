"""Fig 1: 1D stacked-bar breakdown of the FTQC error budget.

Top bar: conventional practice spends the full budget on rotation
synthesis and T-state distillation (arithmetic share is zero).
Bottom bar: our unified decomposition splits the same total across
three primitives. Opening an arithmetic share necessarily tightens
the two continuous shares, but the total cost (shown above each
bar) drops because the arithmetic cost-error curve is steep.
"""
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.patches as patches

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)

plt.rcParams.update({
    "font.size": 18,
    "axes.titlesize": 20,
    "axes.labelsize": 18,
    "xtick.labelsize": 16,
    "ytick.labelsize": 16,
    "legend.fontsize": 15,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "savefig.dpi": 220,
})

TOTAL = 1.0
COLOR_A = "#2a9d8f"
COLOR_U = "#1f6fb4"
COLOR_T = "#d64545"

scenarios = [
    {
        "label": r"Conventional ($\varepsilon_A = 0$)",
        "segments": [(0.0, r"$\varepsilon_A$"),
                     (0.50, r"$\varepsilon_U$"),
                     (0.50, r"$\varepsilon_T$")],
        "cost": r"relative cost: $1.00\times$",
    },
    {
        "label": r"Proposed ($\varepsilon_A > 0$, joint optimum)",
        "segments": [(0.30, r"$\varepsilon_A$"),
                     (0.40, r"$\varepsilon_U$"),
                     (0.30, r"$\varepsilon_T$")],
        "cost": r"relative cost: $0.85\times$",
    },
]

fig, ax = plt.subplots(figsize=(10, 3.8))

bar_h = 0.55
y_positions = [1.0, 0.0]
colors = [COLOR_A, COLOR_U, COLOR_T]

for y, sc in zip(y_positions, scenarios):
    left = 0.0
    for (w, _), col in zip(sc["segments"], colors):
        if w > 0:
            ax.add_patch(patches.Rectangle(
                (left, y), w, bar_h,
                facecolor=col, edgecolor="white", linewidth=2.0))
            ax.text(left + w / 2, y + bar_h / 2, sc["segments"][colors.index(col)][1],
                    ha="center", va="center", color="white",
                    fontsize=18, fontweight="bold")
        left += w
    ax.text(-0.015, y + bar_h / 2, sc["label"], ha="right", va="center",
            fontsize=16)
    ax.text(1.01, y + bar_h / 2, sc["cost"], ha="left", va="center",
            fontsize=15, color="0.25")

ax.set_xlim(-0.01, 1.42)
ax.set_ylim(-0.25, 1.85)
ax.set_xticks([0.0, 0.25, 0.5, 0.75, 1.0])
ax.set_xticklabels(["0", "", r"$\varepsilon_\mathrm{total}/2$", "",
                    r"$\varepsilon_\mathrm{total}$"])
ax.set_yticks([])
ax.set_xlabel(r"share of global logical-error budget $\varepsilon_\mathrm{total}$")
ax.axvline(1.0, color="0.4", linestyle=":", linewidth=1.0)

legend_handles = [
    patches.Patch(facecolor=COLOR_A, label=r"arithmetic ($\varepsilon_A$)"),
    patches.Patch(facecolor=COLOR_U, label=r"rotation synthesis ($\varepsilon_U$)"),
    patches.Patch(facecolor=COLOR_T, label=r"$T$-state distillation ($\varepsilon_T$)"),
]
ax.legend(handles=legend_handles, loc="upper center",
          bbox_to_anchor=(0.5, -0.22), ncol=3, frameon=True,
          framealpha=0.9, edgecolor="0.7")

fig.tight_layout()
fig.savefig(OUT / "fig1_conceptual.pdf", bbox_inches="tight")
fig.savefig(OUT / "fig1_conceptual.png", bbox_inches="tight")
print(f"wrote {OUT / 'fig1_conceptual.pdf'}")
