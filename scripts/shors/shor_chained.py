from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from shor_chained import run_case  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", nargs="+", default=["5,33,4,12", "3,35,4,12"],
        help="base,N,w,m_total triples")
    ap.add_argument("--eb", nargs="+", type=float,
        default=[0.0, 0.1, 0.3, 0.5, 1.0, 2.0])
    ap.add_argument("--shots", type=int, default=2000)
    ap.add_argument("--workdir", type=Path,
        default=PROJECT_ROOT / "results" / "shor_chained")
    ap.add_argument("--synth", type=Path,
        default=PROJECT_ROOT / "build" / "synth-tt")
    ap.add_argument("--approx", type=Path,
        default=PROJECT_ROOT / "build" / "approx-xag")
    ap.add_argument("--output", type=Path, default=None)
    ap.add_argument("--num-random-starts", type=int, default=4,
        help="SS synthesizer random starts for the exact baseline")
    args = ap.parse_args()

    args.workdir.mkdir(parents=True, exist_ok=True)
    all_out = []
    for c in args.cases:
        parts = [int(x) for x in c.split(",")]
        if len(parts) != 4:
            raise SystemExit(f"--cases must be base,N,w,m_total; got {c}")
        b, N, w, m = parts
        case_dir = args.workdir / f"case_{b}_{N}_w{w}_m{m}"
        print(f"\n=== chained Shor: base={b} N={N} w={w} m={m} ===")
        res = run_case(b, N, w, m, args.eb, case_dir,
                       args.synth, args.approx, args.shots,
                       num_random_starts=args.num_random_starts)
        all_out.append(res)
    out_path = args.output or (args.workdir / "shor_chained.json")
    out_path.write_text(json.dumps(all_out, indent=2))
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
