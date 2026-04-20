"""Fig 3: block diagram of chained windowed modexp."""
from __future__ import annotations

from pathlib import Path

import matplotlib.patches as patches
import matplotlib.pyplot as plt

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)

fig, ax = plt.subplots(figsize=(8.5, 4.0))
ax.set_xlim(0, 10.5)
ax.set_ylim(0, 5.2)
ax.axis("off")

# x register bar
ax.add_patch(patches.FancyBboxPatch((0.3, 4.0), 9.7, 0.7,
                                    boxstyle="round,pad=0.04",
                                    fc="#e8ecf4", ec="black", lw=0.8))
ax.text(0.55, 4.35, r"$|x\rangle$", fontsize=14, va="center")
ax.text(0.55, 3.78, r"($m$ qubits)", fontsize=10, va="center", color="gray")
for i in range(4):
    xc = 2.3 + 1.7 * i
    ax.add_patch(patches.Rectangle((xc - 0.28, 4.05), 0.56, 0.6,
                                   fc="#f4e8e8", ec="black", lw=0.5))
    ax.text(xc, 4.35, f"$v_{i}$", ha="center", va="center", fontsize=12)
    ax.annotate("", xy=(xc, 3.1), xytext=(xc, 4.0),
                arrowprops=dict(arrowstyle="->", lw=1.0))

# QROM boxes
for i in range(4):
    xc = 2.3 + 1.7 * i
    ax.add_patch(patches.FancyBboxPatch((xc - 0.55, 2.5), 1.1, 0.6,
                                        boxstyle="round,pad=0.03",
                                        fc="#fef3d8", ec="black", lw=0.9))
    ax.text(xc, 2.80, f"QROM$_{i}$", ha="center", va="center", fontsize=11)
    ax.annotate("", xy=(xc, 1.95), xytext=(xc, 2.5),
                arrowprops=dict(arrowstyle="->", lw=1.0))

# Multipliers
for i in range(4):
    xc = 2.3 + 1.7 * i
    ax.add_patch(patches.Circle((xc, 1.6), 0.32, fc="#d5eedb",
                                ec="black", lw=0.9))
    ax.text(xc, 1.6, r"$\times$", ha="center", va="center", fontsize=15)

# Accumulator bar
ax.add_patch(patches.FancyBboxPatch((0.3, 0.3), 9.7, 0.7,
                                    boxstyle="round,pad=0.04",
                                    fc="#e8ecf4", ec="black", lw=0.9))
ax.text(0.55, 0.65, r"$|y\rangle$", fontsize=14, va="center")
ax.text(0.55, 0.08, "accumulator", fontsize=10, va="center", color="gray")

# Arrows from multipliers into accumulator (serial chain visualization)
for i in range(4):
    xc = 2.3 + 1.7 * i
    ax.annotate("", xy=(xc, 1.0), xytext=(xc, 1.3),
                arrowprops=dict(arrowstyle="->", lw=1.0))

# mod N label
ax.text(10.15, 1.6, r"$\mathrm{mod}\ N$", fontsize=12, va="center", color="gray")

ax.text(5.25, 5.05,
        r"$y = \prod_{i=0}^{k-1} \mathrm{LUT}_i[v_i]\ \mathrm{mod}\ N$",
        ha="center", fontsize=13)
ax.text(5.25, 3.44,
        r"$\mathrm{LUT}_i[v] = \mathrm{base}^{v \cdot 2^{iw}}\ \mathrm{mod}\ N$",
        ha="center", fontsize=11, color="gray")

fig.tight_layout()
fig.savefig(OUT / "fig3_chained_modexp.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig3_chained_modexp.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig3_chained_modexp.pdf'}")
