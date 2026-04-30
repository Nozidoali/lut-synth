"""Unified analyzer for Shor's sweep JSON.

Reads a sweep JSON produced by run.py (or shor_chained.json / shor_e2e.json
from older runs) and applies opt-in analyses. Each --flag enriches the
output with derived columns or writes a side artifact (plot).

Usage:
    python analyze.py --input shor_chained.json --output enriched.json \\
        --bound \\
        --eh --workdir results/shor_chained --shots-per-batch 1 2 3 \\
        --ft-cost --p-phys 1e-3 --eps-budget 0.1

Each analysis is independent; pass any combination.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from shor_psucc_bound import compute_bounds, report_bound_violations  # noqa: E402
from shor_eh import run_eh_on_workdir  # noqa: E402
from model_ft_cost import (  # noqa: E402
    analyze_case as ft_analyze_case,
    print_report as ft_print_report,
    plot_savings as ft_plot_savings,
)


def _parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", type=Path, required=True,
                    help="sweep JSON (e.g. shor_chained.json)")
    ap.add_argument("--output", type=Path, default=None,
                    help="enriched JSON output (default: <input>.analyzed.json)")

    ap.add_argument("--bound", action="store_true",
                    help="add closed-form P_succ lower bound rows")

    ap.add_argument("--eh", action="store_true",
                    help="run Ekera-Hastad multi-shot analysis")
    ap.add_argument("--workdir", type=Path, default=None,
                    help="EH: directory with case_*/chained_eb*.f.json files")
    ap.add_argument("--shots-per-batch", nargs="+", type=int,
                    default=[1, 2, 3, 5])
    ap.add_argument("--num-batches", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)

    ap.add_argument("--ft-cost", action="store_true",
                    help="add FTQC space-time volume columns + plot")
    ap.add_argument("--p-phys", type=float, default=1e-3)
    ap.add_argument("--p-th", type=float, default=1e-2)
    ap.add_argument("--A", type=float, default=0.03)
    ap.add_argument("--eps-budget", type=float, default=0.1)
    ap.add_argument("--t-per-and", type=int, default=4)
    ap.add_argument("--parallelism", type=int, default=1)
    ap.add_argument("--ancilla", type=int, default=4)
    ap.add_argument("--ft-plot", type=Path, default=None,
                    help="FT cost plot path (default: <input_dir>/plots/ft_cost.png)")

    return ap.parse_args()


def _do_bound(cases: list[dict]) -> list[dict]:
    rows = compute_bounds(cases)
    bad = report_bound_violations(rows)
    print(f"[bound] {len(rows)} rows, "
          f"{len(bad)} below predicted lower bound (>5e-3 tol)")
    for r in bad[:5]:
        print(f"  ({r['base']},{r['N']}) m={r['m']} eb={r['eb']}: "
              f"delta={r['delta']:.3f}  "
              f"lb={r['p_succ_predicted_lb']:.3f}  "
              f"meas={r['p_succ_measured']:.3f}")
    return rows


def _do_eh(args: argparse.Namespace) -> list[dict]:
    if args.workdir is None:
        raise SystemExit("--eh requires --workdir DIR with chained_eb*.f.json")
    return run_eh_on_workdir(args.workdir, args.shots_per_batch,
                             args.num_batches, args.seed)


def _do_ft_cost(args: argparse.Namespace, cases: list[dict]
                ) -> list[dict[str, Any]]:
    params = dict(p_phys=args.p_phys, p_th=args.p_th, A=args.A,
                  eps_budget=args.eps_budget, t_per_and=args.t_per_and,
                  parallelism=args.parallelism, ancilla=args.ancilla)
    cases_out = [(c, ft_analyze_case(c, params)) for c in cases]
    cases_out = [(c, r) for c, r in cases_out if r]
    ft_print_report(cases_out, params)
    plot_path = args.ft_plot or (args.input.parent / "plots" / "ft_cost.png")
    plot_path.parent.mkdir(parents=True, exist_ok=True)
    ft_plot_savings(cases_out, plot_path)
    return [{
        "base": c["base"], "N": c["N"],
        "exp_bits": c.get("exp_bits"), "rows": rows,
    } for c, rows in cases_out]


def main() -> None:
    args = _parse_args()
    cases = json.loads(args.input.read_text())

    out: dict[str, Any] = {"input": str(args.input)}
    if args.bound:
        out["bound"] = _do_bound(cases)
    if args.eh:
        out["eh"] = _do_eh(args)
    if args.ft_cost:
        out["ft_cost"] = _do_ft_cost(args, cases)

    if not (args.bound or args.eh or args.ft_cost):
        raise SystemExit("at least one of --bound / --eh / --ft-cost required")

    out_path = args.output or args.input.with_suffix(".analyzed.json")
    out_path.write_text(json.dumps(out, indent=2))
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
