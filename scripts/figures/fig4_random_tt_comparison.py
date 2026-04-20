"""Fig 4: random-TT AND-count ratio across methods.

Uses HLQCS-tabulated ResubALS vs ILP (approx-tt) data from
/tmp/hlqcs/results/approximate/latest/comparison_summary.md. If narrow-
on-HLQCS data is available at results/paper/narrow_hlqcs/narrow_results.json
it is included as a fourth bar; otherwise only three bars are drawn.
"""
from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
NARROW_JSON = PROJECT_ROOT / "results/paper/narrow_hlqcs/narrow_results.json"
HLQCS_SUM = Path("/tmp/hlqcs/results/approximate/latest/comparison_summary.md")
OUT = PROJECT_ROOT / "paper/qce2026/figures"
OUT.mkdir(parents=True, exist_ok=True)


def load_hlqcs(path: Path) -> dict[tuple[str, float], dict]:
    text = path.read_text()
    rows: dict[tuple[str, float], dict] = {}
    eb = None
    for line in text.splitlines():
        if line.startswith("## Error Bound = "):
            eb = float(line.split("= ")[1])
            continue
        if not line.startswith("| ") or line.startswith("|-"):
            continue
        parts = [p.strip() for p in line.strip("|").split("|")]
        if len(parts) < 7 or not parts[0] or parts[0] == "Benchmark":
            continue
        try:
            name = parts[0]
            exact = int(parts[3])
            resub = int(parts[4])
            ilp = int(parts[5])
        except ValueError:
            continue
        rows[(name, eb)] = {"exact": exact, "resub": resub, "ilp": ilp}
    return rows


def load_narrow(path: Path) -> dict[tuple[str, float], dict]:
    if not path.exists():
        return {}
    out: dict[tuple[str, float], dict] = {}
    for row in json.loads(path.read_text()):
        if row.get("failed"):
            continue
        name = row["benchmark"].replace(".tt", "")
        out[(name, row["eb"])] = {"and_before": row["and_before"],
                                  "and_after": row["and_after"]}
    return out


def parse_nm(name: str) -> tuple[int, int] | None:
    m = re.match(r"random_(\d+)in_(\d+)out_seed(\d+)", name)
    if m:
        return int(m.group(1)), int(m.group(2))
    return None


hlqcs = load_hlqcs(HLQCS_SUM)
narrow = load_narrow(NARROW_JSON)

# Aggregate per (n, m) group across seeds
groups: dict[tuple[int, int], dict[float, list[dict]]] = defaultdict(
    lambda: defaultdict(list))
for (name, eb), h in hlqcs.items():
    nm = parse_nm(name)
    if nm is None or h["exact"] == 0:
        continue
    nk = narrow.get((name, eb))
    narrow_ratio = nk["and_after"] / h["exact"] if nk else None
    groups[nm][eb].append({
        "resub": h["resub"] / h["exact"],
        "ilp": h["ilp"] / h["exact"],
        "narrow": narrow_ratio,
    })

if not groups:
    raise SystemExit("no HLQCS groups found — run HLQCS pipeline first")

ebs = sorted({eb for g in groups.values() for eb in g})
has_narrow = any(r["narrow"] is not None
                 for g in groups.values() for seeds in g.values()
                 for r in seeds)
method_labels = ["ResubALS (HLQCS)", "approx-tt (ILP)"] + \
                (["narrow_and_resub"] if has_narrow else [])
method_keys = ["resub", "ilp"] + (["narrow"] if has_narrow else [])
colors = ["#d62728", "#2ca02c"] + (["#4c72b0"] if has_narrow else [])

keys = sorted(groups.keys())
fig, axes = plt.subplots(1, len(keys), figsize=(3.2 * len(keys), 3),
                         sharey=True)
if len(keys) == 1:
    axes = [axes]
x = np.arange(len(ebs))
width = 0.8 / len(method_keys)

for ax, (n, m) in zip(axes, keys):
    for i, (mk, lab, col) in enumerate(zip(method_keys, method_labels, colors)):
        vals = []
        for eb in ebs:
            seeds = groups[(n, m)][eb]
            v = [s[mk] for s in seeds if s.get(mk) is not None]
            vals.append(np.mean(v) if v else np.nan)
        ax.bar(x + (i - 0.5 * (len(method_keys) - 1)) * width, vals, width,
               color=col, label=lab, edgecolor="black", linewidth=0.3)
    ax.set_xticks(x)
    ax.set_xticklabels([f"{eb}" for eb in ebs])
    ax.set_title(f"n={n}, m={m}", fontsize=10)
    ax.set_xlabel("Error bound ε")
    ax.axhline(1, color="gray", lw=0.5, ls=":", alpha=0.5)
    ax.set_ylim(0, 1.15)
    ax.grid(True, alpha=0.25, axis="y")

axes[0].set_ylabel("AND ratio vs exact")
axes[-1].legend(loc="lower left", fontsize=7)
fig.suptitle("AND-count ratio across methods on HLQCS random-TT benchmarks",
             y=1.00, fontsize=11)
fig.tight_layout()
fig.savefig(OUT / "fig4_random_tt.pdf", dpi=300, bbox_inches="tight")
fig.savefig(OUT / "fig4_random_tt.png", dpi=200, bbox_inches="tight")
print(f"wrote {OUT / 'fig4_random_tt.pdf'}  ({'with' if has_narrow else 'without'} narrow)")
