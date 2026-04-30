"""Unified Shor's experiment runner.

Subcommands (each writes a sweep JSON under --workdir):

    mono       Monolithic Shor's (single 2^m-entry LUT)
                 --cases base,N,exp_bits ...
    windowed   Chained windowed Shor's (k = ceil(m/w) per-window LUTs)
                 --cases base,N,w,m_total ...
    eb-alloc   Per-window ε_B allocation strategies (single chained case)
                 --case base,N,w,m_total --totals T1 T2 ... --strategies S1 S2 ...
    synth-only Multi-start synthesis audit (no approximation)
                 --cases mono:b,N,m | chained:b,N,w,m  --starts 1 4 16 64

Examples:
    python run.py windowed --cases 5,33,4,12 3,35,4,12 --eb 0 0.1 0.3 1.0
    python run.py mono --cases 2,15,4 3,35,8 --eb 0 0.05 0.1 0.3
    python run.py eb-alloc --case 5,33,4,12 --totals 0.3 0.5 1.0 \\
                           --strategies uniform front_heavy back_heavy
    python run.py synth-only --cases mono:5,33,8 chained:5,33,4,12 --starts 1 4 16 64
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from shor_e2e import run_case as run_mono_case, print_case_table  # noqa: E402
from shor_chained import run_case as run_windowed_case  # noqa: E402
from per_window import run_alloc, print_alloc_table  # noqa: E402
from synth_baseline import run_audit  # noqa: E402


def _add_common_args(p: argparse.ArgumentParser, default_workdir_name: str
                     ) -> None:
    p.add_argument("--eb", nargs="+", type=float,
                   default=[0.0, 0.05, 0.1, 0.3, 1.0],
                   help="error bounds to sweep")
    p.add_argument("--shots", type=int, default=1000,
                   help="Monte Carlo shots per eb point")
    p.add_argument("--workdir", type=Path,
                   default=PROJECT_ROOT / "results" / default_workdir_name)
    p.add_argument("--output", type=Path, default=None,
                   help="JSON output path (default: workdir/<mode>.json)")
    p.add_argument("--synth", type=Path,
                   default=PROJECT_ROOT / "build" / "synth-tt")
    p.add_argument("--approx", type=Path,
                   default=PROJECT_ROOT / "build" / "approx-xag")
    p.add_argument("--num-random-starts", type=int, default=4)


def _parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="mode", required=True)

    mono = sub.add_parser("mono", help="Monolithic Shor's")
    _add_common_args(mono, "shor_mono")
    mono.add_argument("--cases", nargs="+", required=True,
                      help="space-separated 'base,N,exp_bits' triples")
    mono.add_argument("--max-pattern-error", type=int, default=0,
                      help="Cap LACs whose max single-pattern err exceeds this")
    mono.add_argument("--methods", nargs="+", default=["narrow"],
                      choices=["narrow", "resubals", "ilp"])
    mono.add_argument("--approx-tt", type=Path,
                      default=PROJECT_ROOT / "build" / "approx-tt",
                      help="Path to approx-tt binary (ILP)")

    win = sub.add_parser("windowed", help="Chained windowed Shor's")
    _add_common_args(win, "shor_chained")
    win.add_argument("--cases", nargs="+", required=True,
                     help="space-separated 'base,N,w,m_total' tuples")

    alloc = sub.add_parser("eb-alloc", help="Per-window ε_B allocation study")
    _add_common_args(alloc, "per_window_eb")
    alloc.add_argument("--case", type=str, required=True,
                       help="single 'base,N,w,m_total'")
    alloc.add_argument("--totals", nargs="+", type=float, required=True,
                       help="total ε_B budgets (sum across windows)")
    alloc.add_argument("--strategies", nargs="+",
                       default=["uniform", "front_heavy", "back_heavy",
                                "linear_up", "linear_down"])

    synth = sub.add_parser("synth-only", help="Multi-start synthesis audit (no approximation)")
    _add_common_args(synth, "synth_baseline")
    synth.add_argument("--cases", nargs="+", required=True,
                       help="each is 'mono:b,N,m' or 'chained:b,N,w,m'")
    synth.add_argument("--starts", nargs="+", type=int,
                       default=[1, 4, 16, 64])

    return ap.parse_args()


def _output_path(args: argparse.Namespace) -> Path:
    return args.output or (args.workdir / f"{args.mode}.json")


def _run_mono(args: argparse.Namespace) -> list[dict[str, Any]]:
    args.workdir.mkdir(parents=True, exist_ok=True)
    out: list[dict[str, Any]] = []
    for c in args.cases:
        b, N, exp_bits = (int(x) for x in c.split(","))
        case_dir = args.workdir / f"case_{b}_{N}_{exp_bits}"
        result = run_mono_case(b, N, exp_bits, args.eb, case_dir,
                               args.synth, args.approx, args.max_pattern_error,
                               methods=args.methods,
                               approx_tt_bin=args.approx_tt,
                               num_shots=args.shots,
                               num_random_starts=args.num_random_starts)
        if result is None:
            continue
        out.append(result)
        print_case_table(result)
    return out


def _run_windowed(args: argparse.Namespace) -> list[dict[str, Any]]:
    args.workdir.mkdir(parents=True, exist_ok=True)
    out: list[dict[str, Any]] = []
    for c in args.cases:
        parts = [int(x) for x in c.split(",")]
        if len(parts) != 4:
            raise SystemExit(f"--cases must be base,N,w,m_total; got {c}")
        b, N, w, m = parts
        case_dir = args.workdir / f"case_{b}_{N}_w{w}_m{m}"
        print(f"\n=== chained Shor: base={b} N={N} w={w} m={m} ===")
        res = run_windowed_case(b, N, w, m, args.eb, case_dir,
                                args.synth, args.approx, args.shots,
                                num_random_starts=args.num_random_starts)
        out.append(res)
    return out


def _run_alloc(args: argparse.Namespace) -> list[dict[str, Any]]:
    parts = [int(x) for x in args.case.split(",")]
    if len(parts) != 4:
        raise SystemExit(f"--case must be base,N,w,m_total; got {args.case}")
    b, N, w, m = parts
    case_workdir = args.workdir / f"{b}_{N}_w{w}_m{m}"
    rows = run_alloc(b, N, w, m, args.totals, args.strategies,
                     case_workdir, args.synth, args.approx,
                     num_shots=args.shots,
                     num_random_starts=args.num_random_starts)
    print_alloc_table(rows, m)
    return rows


def _run_synth(args: argparse.Namespace) -> list[dict[str, Any]]:
    return run_audit(args.cases, args.starts, args.eb,
                     args.workdir, args.synth, args.approx)


_DISPATCH = {
    "mono": _run_mono,
    "windowed": _run_windowed,
    "eb-alloc": _run_alloc,
    "synth-only": _run_synth,
}


def main() -> None:
    args = _parse_args()
    out_data = _DISPATCH[args.mode](args)
    out_path = _output_path(args)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(out_data, indent=2))
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
