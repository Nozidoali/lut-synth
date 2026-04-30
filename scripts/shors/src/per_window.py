"""Per-window error-budget allocation strategies for chained Shor's.

Given a chained windowed case (k = ceil(m/w) windows) and a TOTAL ε_B budget,
compare allocation strategies (uniform, front-heavy, back-heavy, gradients)
by running shor_chained.run_case once per (total, strategy).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from shor_chained import run_case  # noqa: E402


def allocate(strategy: str, k: int, total: float) -> list[float]:
    if strategy == "uniform":
        return [total / k] * k
    if strategy == "front_heavy":
        ebs = [0.0] * k
        ebs[0] = total
        return ebs
    if strategy == "back_heavy":
        ebs = [0.0] * k
        ebs[-1] = total
        return ebs
    if strategy == "linear_up":
        weights = [i + 1 for i in range(k)]
        s = sum(weights)
        return [total * wi / s for wi in weights]
    if strategy == "linear_down":
        weights = [k - i for i in range(k)]
        s = sum(weights)
        return [total * wi / s for wi in weights]
    raise ValueError(f"unknown strategy {strategy}")


def run_alloc(base: int, N: int, w: int, m_total: int,
              totals: list[float], strategies: list[str],
              workdir: Path, synth_bin: Path, approx_bin: Path,
              num_shots: int, num_random_starts: int) -> list[dict[str, Any]]:
    k = math.ceil(m_total / w)
    print(f"case=(base={base},N={N},w={w},m={m_total}) → k={k} windows")

    rows: list[dict[str, Any]] = []
    for total in totals:
        for strategy in strategies:
            ebs = allocate(strategy, k, total)
            case_workdir = workdir / f"total{total}_{strategy}"
            print(f"\n--- total={total} strategy={strategy} eb_i={ebs} ---")
            res = run_case(base, N, w, m_total, [ebs], case_workdir,
                           synth_bin, approx_bin,
                           num_shots=num_shots,
                           num_random_starts=num_random_starts)
            row = res["sweep"][0]
            rows.append({
                "total": total, "strategy": strategy, "ebs": ebs,
                "and_before": row["and_before"],
                "and_after": row["and_after"],
                "and_saved": row["and_before"] - row["and_after"],
                "patterns_changed": row["patterns_changed"],
                "P_ok": row["shor_shots"]["success_rate"],
                "E_trials": row["shor_shots"]["expected_trials"],
                "det_ok": row["shor"]["success"],
            })
    return rows


def print_alloc_table(rows: list[dict[str, Any]], m_total: int) -> None:
    print(f"\n{'total':>6} {'strategy':>14} {'AND':>10} {'saved':>6} "
          f"{'chg%':>6} {'P(ok)':>6} {'E[tr]':>8} {'det':>4}")
    print("-" * 78)
    L = 1 << m_total
    for r in rows:
        et = f"{r['E_trials']:.1f}" if math.isfinite(r['E_trials']) else "inf"
        print(f"{r['total']:>6.2f} {r['strategy']:>14} "
              f"{r['and_before']}→{r['and_after']:<4} {r['and_saved']:>6} "
              f"{100*r['patterns_changed']/L:>5.1f}% "
              f"{r['P_ok']:>6.3f} {et:>8} "
              f"{'✓' if r['det_ok'] else '✗':>4}")
