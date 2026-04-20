"""Run narrow_and_resub on the HLQCS random-TT benchmark corpus and
record AND-count ratio, integer MAE, and runtime per (benchmark, epsilon).
"""
from __future__ import annotations

import argparse
import json
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
HLQCS_TT = Path("/tmp/hlqcs/benchmarks/truth-tables")
BENCHMARKS = [
    *sorted(HLQCS_TT.glob("random-lut/random_*_m*_s*.tt")),
    *sorted(HLQCS_TT.glob("qlut/truth_tables/gf_mult4.tt")),
    *sorted(HLQCS_TT.glob("qlut/truth_tables/modexp_2_15_4_4.tt")),
    *sorted(HLQCS_TT.glob("qlut/truth_tables/modexp_3_221_4_8.tt")),
    *sorted(HLQCS_TT.glob("qlut/truth_tables/modexp_3_35_8_6.tt")),
]
ERROR_BOUNDS = [0.0, 0.5, 1.0, 2.0, 4.0]


def run_narrow(tt_path: Path, eb: float, approx_bin: Path,
               starts: int = 4) -> dict:
    t0 = time.time()
    proc = subprocess.run(
        [str(approx_bin), "--input", str(tt_path), "--output", "/tmp/_paper.tt",
         "--method", "narrow", "--error-bound", str(eb),
         "--num-random-starts", str(starts)],
        capture_output=True, text=True, timeout=120)
    dt = time.time() - t0
    if proc.returncode != 0:
        return {"failed": True, "stderr": proc.stderr[:200],
                "runtime_s": dt}
    stats = json.loads(proc.stdout.strip())
    stats["runtime_s"] = dt
    return stats


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--approx-bin", type=Path,
                    default=PROJECT_ROOT / "build" / "approx-xag")
    ap.add_argument("--output", type=Path,
                    default=PROJECT_ROOT / "results/paper/narrow_hlqcs/narrow_results.json")
    args = ap.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    for bench in BENCHMARKS:
        for eb in ERROR_BOUNDS:
            stats = run_narrow(bench, eb, args.approx_bin)
            rows.append({"benchmark": bench.name, "eb": eb, **stats})
            print(f"{bench.name:<35} eb={eb}  AND "
                  f"{stats.get('and_before','?')}->{stats.get('and_after','?')}  "
                  f"t={stats.get('runtime_s', 0):.2f}s")
    args.output.write_text(json.dumps(rows, indent=2))
    print(f"wrote {len(rows)} rows to {args.output}")


if __name__ == "__main__":
    main()
