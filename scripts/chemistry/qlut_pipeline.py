from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from qlut_pipeline import run_pipeline  # noqa: E402


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Approximate QLUT evaluation pipeline"
    )
    parser.add_argument("--system", default="H4", help="Molecule name")
    parser.add_argument("--thc-rank", type=int, default=10, help="THC rank")
    parser.add_argument(
        "--num-bits", type=int, default=10, help="State preparation bits"
    )
    parser.add_argument(
        "--error-bound", type=float, default=1.0, help="Error bound"
    )
    parser.add_argument(
        "--eps", type=float, default=1e-2, help="Epsilon threshold"
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("results"), help="Output dir"
    )
    parser.add_argument(
        "--approx-tt", type=Path, help="Path to approx-tt binary"
    )
    parser.add_argument(
        "--approx-xag", type=Path, help="Path to approx-xag binary"
    )
    parser.add_argument(
        "--method", choices=["ilp", "narrow"], default="ilp",
        help="Approximator: 'ilp' (approx-tt, Gurobi) or 'narrow' (approx-xag, heuristic)"
    )
    parser.add_argument(
        "--time-limit", type=float, default=60.0, help="ILP time limit (sec)"
    )
    parser.add_argument(
        "--nh", type=int, help="Number of hydrogens (for H_chain)"
    )
    parser.add_argument(
        "--basis", type=str, default=None,
        help="Override basis set (e.g. 'sto-3g' to shrink nmo for rank-limited THC)"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    system_kwargs: dict[str, Any] = {}
    if args.nh is not None:
        system_kwargs["nh"] = args.nh

    run_pipeline(
        system=args.system,
        thc_rank=args.thc_rank,
        num_bits_state_prep=args.num_bits,
        error_bound=args.error_bound,
        output_dir=args.output_dir,
        eps=args.eps,
        approx_tt_binary=args.approx_tt,
        approx_xag_binary=args.approx_xag,
        time_limit=args.time_limit,
        method=args.method,
        basis=args.basis,
        **system_kwargs,
    )


if __name__ == "__main__":
    main()
