"""Fig 2: narrow_and_resub's 5 local replacement candidates per AND gate."""
from __future__ import annotations

from pathlib import Path

import matplotlib.patches as patches
import matplotlib.pyplot as plt

OUT = Path(__file__).resolve().parent.parent.parent / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)


def _node(ax, x, y, label, shape="circle", color="#4c72b0"):
    if shape == "circle":
        ax.add_patch(patches.Circle((x, y), 0.17, fc=color, ec="black", lw=0.8))
    else:
        ax.add_patch(patches.Rectangle((x - 0.17, y - 0.17), 0.34, 0.34,
                                       fc=color, ec="black", lw=0.8))
    ax.text(x, y, label, ha="center", va="center", fontsize=8,
            color="white", fontweight="bold")


def _draw_target(ax):
    _node(ax, 0.25, 0.25, "a", color="#888")
    _node(ax, 0.75, 0.25, "b", color="#888")
    _node(ax, 0.50, 1.00, "∧", shape="square", color="#4c72b0")
    ax.plot([0.25, 0.50], [0.42, 0.83], "k-", lw=1)
    ax.plot([0.75, 0.50], [0.42, 0.83], "k-", lw=1)


def _draw_simple(ax, label, color):
    _node(ax, 0.50, 1.00, label, shape="square", color=color)


def _draw_fanin(ax, label):
    _node(ax, 0.50, 1.00, label, color="#555")


def _draw_xor(ax):
    _node(ax, 0.25, 0.25, "a", color="#888")
    _node(ax, 0.75, 0.25, "b", color="#888")
    _node(ax, 0.50, 1.00, "⊕", shape="square", color="#55a868")
    ax.plot([0.25, 0.50], [0.42, 0.83], "k-", lw=1)
    ax.plot([0.75, 0.50], [0.42, 0.83], "k-", lw=1)


fig, axes = plt.subplots(1, 6, figsize=(10, 1.8))
titles = ["Target AND", "→ const 0", "→ const 1",
          "→ fanin a", "→ fanin b", "→ XOR(a, b)"]
for ax, t in zip(axes, titles):
    ax.set_xlim(-0.25, 1.25)
    ax.set_ylim(-0.05, 1.35)
    ax.axis("off")
    ax.set_title(t, fontsize=9)

_draw_target(axes[0])
_draw_simple(axes[1], "0", "#aaaaaa")
_draw_simple(axes[2], "1", "#aaaaaa")
_node(axes[3], 0.50, 1.00, "a", color="#555")
_node(axes[4], 0.50, 1.00, "b", color="#555")
_draw_xor(axes[5])

fig.suptitle("narrow_and_resub: 5 local replacement candidates per AND gate",
             y=1.05, fontsize=10)
fig.tight_layout(rect=[0, 0, 1, 0.95])
fig.savefig(OUT / "fig2_narrow_lac.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig2_narrow_lac.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig2_narrow_lac.pdf'}")
