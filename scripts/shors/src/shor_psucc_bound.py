"""Closed-form lower bound on Shor's success rate under approximation.

Bound: P_succ(eb) >= (1 - delta)^2 * P_succ(0)

where delta = patterns_changed / 2^m. Computes per-(case, eb) rows
and reports any violations of this lower bound.
"""
from __future__ import annotations

from typing import Any


def compute_bounds(cases: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """For each case in a chained-windowed sweep JSON, compute the bound row.

    Each input case is one entry in shor_chained.json. Returns flat rows
    suitable for serialization.
    """
    rows: list[dict[str, Any]] = []
    for case in cases:
        base = case["base"]
        N = case["N"]
        m = case["m_total"]
        L = 1 << m
        sweep = case["sweep"]
        baseline = next((s for s in sweep if s["eb"] == 0.0), sweep[0])
        p0 = baseline.get("shor_shots", {}).get("success_rate")
        if p0 is None:
            continue
        for s in sweep:
            pc = s.get("patterns_changed", 0)
            delta = pc / L
            p_meas = s.get("shor_shots", {}).get("success_rate")
            if p_meas is None:
                continue
            p_lb = (1.0 - delta) ** 2 * p0
            rows.append({
                "base": base, "N": N, "m": m, "eb": s["eb"],
                "delta": delta,
                "p_succ_baseline": p0,
                "p_succ_predicted_lb": p_lb,
                "p_succ_measured": p_meas,
                "and_after": s["and_after"],
                "and_before": s["and_before"],
            })
    return rows


def report_bound_violations(rows: list[dict[str, Any]],
                            tolerance: float = 5e-3) -> list[dict[str, Any]]:
    """Return rows where measured P_succ falls below the predicted bound."""
    return [r for r in rows
            if r["p_succ_measured"] + tolerance < r["p_succ_predicted_lb"]]
