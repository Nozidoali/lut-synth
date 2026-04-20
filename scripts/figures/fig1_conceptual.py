"""Fig 1: side-by-side donut charts comparing FTQC current practice
(ε_B = 0) vs our proposed three-primitive budget split."""
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)

fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.2))

# (a) Current practice
sizes_a = [1e-6, 0.6, 0.4]  # tiny slice instead of zero for pie render
labels_a = ["Boolean\n(ε_B = 0)", "Rotation\nsynthesis (ε_U)",
            "T-state\n(ε_T)"]
colors_a = ["#d0d0d0", "#4c72b0", "#dd8452"]
axes[0].pie(sizes_a, labels=labels_a, colors=colors_a, startangle=90,
            wedgeprops={"width": 0.35, "edgecolor": "white"},
            textprops={"fontsize": 8})
axes[0].set_title("(a) Current FTQC practice", fontsize=10, pad=12)

# (b) Proposed
sizes_b = [0.30, 0.45, 0.25]
labels_b = ["Boolean\n(ε_B > 0)", "Rotation\nsynthesis (ε_U)",
            "T-state\n(ε_T)"]
colors_b = ["#55a868", "#4c72b0", "#dd8452"]
axes[1].pie(sizes_b, labels=labels_b, colors=colors_b, startangle=90,
            wedgeprops={"width": 0.35, "edgecolor": "white"},
            textprops={"fontsize": 8})
axes[1].set_title("(b) Proposed three-primitive allocation",
                  fontsize=10, pad=12)

fig.suptitle(r"$\varepsilon_{\mathrm{total}} \geq \varepsilon_B + "
             r"\varepsilon_U + \varepsilon_T$", y=0.02, fontsize=10)
fig.tight_layout(rect=[0, 0.04, 1, 1])
fig.savefig(OUT / "fig1_conceptual.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig1_conceptual.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig1_conceptual.pdf'}")
