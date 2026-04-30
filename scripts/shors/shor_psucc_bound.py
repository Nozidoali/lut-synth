"""Per-shot Shor success probability lower bound from narrow's
patterns_changed metric.

Lemma (chained windowed modexp under per-window arithmetic
approximation):
    P_succ(eps_A) >= (1 - delta(eps_A))^2 * P_succ(0),
where
    delta(eps_A) = (# inputs x in [0, 2^m) with tilde f(x) != f(x))
                   / 2^m
                 = patterns_changed / 2^m   (already reported by
                                              shor_chained.py).

Sketch: the per-shot QFT measurement distribution is the FFT of
the match autocorrelation m(tau) = Pr_x[f(x) == f(x+tau)]. The
peak at tau = r is reduced by exactly the probability that BOTH
f(x) and f(x+r) are computed correctly, i.e. by (1-delta)^2 if
errors at different x are treated as independent. The base
classical recovery rate (continued fractions + gcd) is unchanged
under approximation, so the per-shot success probability lower
bound is (1-delta)^2 * P_succ(eps_A=0).

This script joins the shor_chained.json sweep with the Monte-Carlo
P_succ measurements and reports both the predicted lower bound and
the measured rate, so we can check the bound is (a) valid and
(b) approximately tight on the chained windowed modexp.

Usage:
  python scripts/shor_psucc_bound.py \\
      --input results/shor_chained/shor_chained.json \\
      --output paper/qce2026/data/shor_chained/psucc_bound.json
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()

    cases = json.loads(args.input.read_text())
    rows = []
    for case in cases:
        base = case["base"]; N = case["N"]; m = case["m_total"]
        L = 1 << m
        sweep = case["sweep"]
        # baseline: eb=0 (or smallest eb with patterns_changed = 0)
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

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(rows, indent=2))
    print(f"wrote {len(rows)} rows to {args.output}")
    # Quick sanity check
    bad = [r for r in rows if r["p_succ_measured"] + 5e-3
           < r["p_succ_predicted_lb"]]
    print(f"  rows where measured < predicted lower bound: {len(bad)}/{len(rows)}")
    if bad:
        for r in bad[:5]:
            print(f"    ({r['base']},{r['N']}) m={r['m']} eb={r['eb']}: "
                  f"delta={r['delta']:.3f} "
                  f"lb={r['p_succ_predicted_lb']:.3f} "
                  f"meas={r['p_succ_measured']:.3f}")


if __name__ == "__main__":
    main()
