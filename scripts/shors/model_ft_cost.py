from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from model_ft_cost import analyze_case, plot_savings, print_report  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "shor_e2e.json")
    ap.add_argument("--outdir", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "plots")
    ap.add_argument("--p-phys", type=float, default=1e-3)
    ap.add_argument("--p-th", type=float, default=1e-2)
    ap.add_argument("--A", type=float, default=0.03)
    ap.add_argument("--eps-budget", type=float, default=0.1,
        help="target logical failure rate per quantum shot")
    ap.add_argument("--t-per-and", type=int, default=4,
        help="T-gates emitted per AND node (Toffoli ≈ 4 T with ancilla)")
    ap.add_argument("--parallelism", type=int, default=1,
        help="# T layers merged per depth step (1 = fully serial modexp)")
    ap.add_argument("--ancilla", type=int, default=4,
        help="extra logical ancilla on top of 2·log₂(N)")
    args = ap.parse_args()

    params = dict(p_phys=args.p_phys, p_th=args.p_th, A=args.A,
                  eps_budget=args.eps_budget, t_per_and=args.t_per_and,
                  parallelism=args.parallelism, ancilla=args.ancilla)

    cases = json.loads(args.input.read_text())
    cases_out = [(c, analyze_case(c, params)) for c in cases]
    cases_out = [(c, r) for c, r in cases_out if r]
    print_report(cases_out, params)

    args.outdir.mkdir(parents=True, exist_ok=True)
    plot_savings(cases_out, args.outdir / "ft_cost_model.png")


if __name__ == "__main__":
    main()
