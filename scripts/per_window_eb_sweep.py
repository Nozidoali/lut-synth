"""Per-window eb allocation study.

For a fixed TOTAL budget across all k windows, try multiple allocation
strategies (uniform, front-heavy, back-heavy, linear gradient, random)
and measure which gives better (AND savings, P(ok)) tradeoff.

Uniform is the baseline we've been using. This script asks: can we
gain by directing more error budget to the windows that corrupt f̃(x)
the least per saved AND?
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from shor_chained import run_case


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


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--case", type=str, default="5,33,4,12",
                    help="base,N,w,m_total")
    ap.add_argument("--totals", nargs="+", type=float,
                    default=[0.3, 0.5, 1.0],
                    help="total eb budget (sum across windows)")
    ap.add_argument("--strategies", nargs="+",
                    default=["uniform", "front_heavy", "back_heavy",
                             "linear_up", "linear_down"])
    ap.add_argument("--shots", type=int, default=1000)
    ap.add_argument("--num-random-starts", type=int, default=4)
    ap.add_argument("--workdir", type=Path,
                    default=PROJECT_ROOT / "results" / "per_window_eb")
    ap.add_argument("--synth", type=Path,
                    default=PROJECT_ROOT / "build" / "synth-tt")
    ap.add_argument("--approx", type=Path,
                    default=PROJECT_ROOT / "build" / "approx-xag")
    args = ap.parse_args()

    b, N, w, m = (int(x) for x in args.case.split(","))
    k = math.ceil(m / w)
    print(f"case=(base={b},N={N},w={w},m={m}) → k={k} windows")

    all_results = []
    for total in args.totals:
        for strategy in args.strategies:
            ebs = allocate(strategy, k, total)
            case_workdir = args.workdir / f"{b}_{N}_w{w}_m{m}" / \
                           f"total{total}_{strategy}"
            print(f"\n--- total={total} strategy={strategy} eb_i={ebs} ---")
            res = run_case(b, N, w, m, [ebs], case_workdir,
                           args.synth, args.approx,
                           num_shots=args.shots,
                           num_random_starts=args.num_random_starts)
            row = res["sweep"][0]
            all_results.append({
                "total": total, "strategy": strategy, "ebs": ebs,
                "and_before": row["and_before"],
                "and_after": row["and_after"],
                "and_saved": row["and_before"] - row["and_after"],
                "patterns_changed": row["patterns_changed"],
                "P_ok": row["shor_shots"]["success_rate"],
                "E_trials": row["shor_shots"]["expected_trials"],
                "det_ok": row["shor"]["success"],
            })

    out_path = args.workdir / f"{b}_{N}_w{w}_m{m}" / "per_window_eb.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(all_results, indent=2))

    print(f"\n{'total':>6} {'strategy':>14} {'AND':>10} {'saved':>6} "
          f"{'chg%':>6} {'P(ok)':>6} {'E[tr]':>8} {'det':>4}")
    print("-" * 78)
    for r in all_results:
        et = f"{r['E_trials']:.1f}" if math.isfinite(r['E_trials']) else "inf"
        print(f"{r['total']:>6.2f} {r['strategy']:>14} "
              f"{r['and_before']}→{r['and_after']:<4} {r['and_saved']:>6} "
              f"{100*r['patterns_changed']/(1<<m):>5.1f}% "
              f"{r['P_ok']:>6.3f} {et:>8} "
              f"{'✓' if r['det_ok'] else '✗':>4}")
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
