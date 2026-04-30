"""End-to-end Shor's algorithm with narrow AND-resub approximation.

Pipeline per case (base, N, exp_bits):
  1. Generate the modexp truth table f(x) = base^x mod N.
  2. Synthesize the exact TT to an XAG (baseline AND count).
  3. For each error_bound, run narrow_and_resub via approx-xag, write
     the approximated TT, decode back to integers.
  4. Run Shor's classical post-processing on the approximated f̃(x):
     find a plausible even period r by majority match, then try
     gcd(base^{r/2} ± 1, N). Report whether factoring succeeds.

No quantum simulator is used; we only exercise the classical post-processing
that Shor's runs after quantum period finding, applied to an
approximation-corrupted f̃(x) table. This is a cleaner Boolean testbed than
chemistry because success is a binary (factors found vs not).

Usage:
    python scripts/shor_e2e.py
    python scripts/shor_e2e.py --cases 2,15,4 3,35,8 --eb 0 0.01 0.05 0.1 0.3 1.0
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any

_project_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_project_root / "third-party" / "qlut-benchmarks" / "src"))
sys.path.insert(0, str(_project_root / "scripts" / "shors" / "src"))

from modexp import generate_modexp_truth_table
from truthTable import TruthTable

from shor_lib import (
    decode_f_values,
    period_autocorrelation,
    period_snr,
    continued_fraction_denominator,
    shor_one_shot,
    shor_shot_statistics,
    shor_factor,
)


DEFAULT_CASES = [
    (2, 15, 4),
    (3, 35, 8),
    (5, 77, 6),
]
DEFAULT_EB = [0.0, 0.01, 0.05, 0.1, 0.3, 1.0]


WALL_TIMEOUT = 60  # seconds per method invocation


def _run_approx_tt(approx_tt_bin: Path, tt_in: Path, tt_out: Path,
                   eb: float, time_limit: float = 20.0) -> dict[str, Any]:
    """Call approx-tt (Gurobi ILP). Returns approx_stats + 'method'='ilp'."""
    cmd = [str(approx_tt_bin), "--input", str(tt_in),
           "--output", str(tt_out),
           "--error-bound", str(eb),
           "--time-limit", str(time_limit)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=WALL_TIMEOUT)
    except subprocess.TimeoutExpired:
        return {"method": "ilp", "failed": True, "stderr": f"wall timeout {WALL_TIMEOUT}s"}
    if proc.returncode != 0:
        return {"method": "ilp", "failed": True, "stderr": proc.stderr[:400]}
    try:
        stats = json.loads(proc.stdout.strip())
    except json.JSONDecodeError:
        return {"method": "ilp", "failed": True, "stderr": proc.stdout[:400]}
    stats["method"] = "ilp"
    return stats


def _run_approx_xag(approx_xag_bin: Path, tt_in: Path, tt_out: Path,
                    method: str, eb: float,
                    max_pattern_error: int = 0,
                    num_random_starts: int = 4) -> dict[str, Any]:
    cmd = [str(approx_xag_bin), "--input", str(tt_in),
           "--output", str(tt_out),
           "--method", method,
           "--error-bound", str(eb),
           "--num-random-starts", str(num_random_starts)]
    if max_pattern_error > 0 and method == "narrow":
        cmd += ["--max-pattern-error", str(max_pattern_error)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=WALL_TIMEOUT)
    except subprocess.TimeoutExpired:
        return {"method": method, "failed": True,
                "stderr": f"wall timeout {WALL_TIMEOUT}s"}
    if proc.returncode != 0:
        return {"method": method, "failed": True, "stderr": proc.stderr[:400]}
    return json.loads(proc.stdout.strip())


def _prepare_exact_modexp_tt(base: int, N: int, exp_bits: int,
                             workdir: Path) -> tuple[Path, list[int], dict]:
    workdir.mkdir(parents=True, exist_ok=True)
    patterns, n_in, n_out = generate_modexp_truth_table(base, N, exp_bits)
    tt_path = workdir / f"modexp_{base}_{N}_{n_in}_{n_out}.tt"
    TruthTable.from_patterns(patterns, num_inputs=n_in).to_file(str(tt_path))
    exact_tt = TruthTable.from_file(str(tt_path))
    exact_f = decode_f_values(exact_tt)
    exact_shor = shor_factor(base, N, exact_f)
    return tt_path, exact_f, exact_shor


def _evaluate_approx_file(approx_path: Path, exact_f: list[int],
                          base: int, N: int, true_r: int,
                          num_shots: int) -> dict[str, Any]:
    approx_tt = TruthTable.from_file(str(approx_path))
    approx_f = decode_f_values(approx_tt)
    return {
        "approx_f": approx_f,
        "max_integer_err": max(abs(a - b) for a, b in zip(approx_f, exact_f)),
        "patterns_changed": sum(1 for a, b in zip(approx_f, exact_f) if a != b),
        "shor_deterministic": shor_factor(base, N, approx_f),
        "period_snr": period_snr(approx_f, true_r),
        "shor_shots": shor_shot_statistics(base, N, approx_f,
                                           num_shots=num_shots),
    }


def _approx_and_evaluate(method: str, eb: float, tt_path: Path,
                         approx_path: Path, approx_bin: Path,
                         approx_tt_bin: Path | None,
                         max_pattern_error: int, num_random_starts: int,
                         exact_f: list[int], base: int, N: int,
                         true_r: int, num_shots: int) -> dict[str, Any]:
    if method == "ilp":
        if approx_tt_bin is None:
            return {"eb": eb, "method": method, "failed": True,
                    "stderr": "approx_tt_bin not provided"}
        stats = _run_approx_tt(approx_tt_bin, tt_path, approx_path, eb)
    else:
        stats = _run_approx_xag(approx_bin, tt_path, approx_path, method,
                                eb, max_pattern_error,
                                num_random_starts=num_random_starts)
    entry: dict[str, Any] = {"eb": eb, "method": method}
    if stats.get("failed"):
        entry["failed"] = True
        entry["stderr"] = stats.get("stderr", "")
        return entry
    evald = _evaluate_approx_file(approx_path, exact_f, base, N,
                                  true_r, num_shots)
    entry.update({
        "and_before": stats.get("and_before", stats.get("and_count")),
        "and_after": stats.get("and_after", stats.get("and_count")),
        "lacs": stats.get("lacs_applied", stats.get("bits_flipped", 0)),
        "acc_err": stats.get("accumulated_error",
                             stats.get("weighted_mean_error", 0.0)),
        "max_integer_err": evald["max_integer_err"],
        "patterns_changed": evald["patterns_changed"],
        "period_exact": true_r,
        "shor": evald["shor_deterministic"],
        "period_snr": evald["period_snr"],
        "shor_shots": evald["shor_shots"],
    })
    if method == "ilp":
        entry["ilp_extra"] = {
            "bits_flipped": stats.get("bits_flipped"),
            "ss_and_estimate": stats.get("ss_and_estimate"),
            "ss_and_actual": stats.get("ss_and_actual"),
            "monomials_before": stats.get("monomials_before"),
            "monomials_after": stats.get("monomials_after"),
        }
    return entry


def run_case(base: int, N: int, exp_bits: int,
             error_bounds: list[float], workdir: Path,
             synth_bin: Path, approx_bin: Path,
             max_pattern_error: int = 0,
             methods: list[str] | None = None,
             approx_tt_bin: Path | None = None,
             num_shots: int = 1000,
             num_random_starts: int = 4) -> dict[str, Any]:
    tt_path, exact_f, exact_shor = _prepare_exact_modexp_tt(
        base, N, exp_bits, workdir)
    if not exact_shor["success"]:
        print(f"  skip: exact modexp with base={base} cannot factor N={N} "
              f"(base^(r/2) ≡ ±1 mod N); try a different base")
        return None
    synth_res = subprocess.run(
        [str(synth_bin), "--input", str(tt_path),
         "--num-random-starts", str(num_random_starts)],
        capture_output=True, text=True, check=True)
    baseline_and = json.loads(synth_res.stdout.strip())["and_count"]

    methods = methods or ["narrow"]
    results: list[dict[str, Any]] = []
    for eb in error_bounds:
        for method in methods:
            approx_path = workdir / f"approx_{method}_{base}_{N}_eb{eb}.tt"
            entry = _approx_and_evaluate(
                method, eb, tt_path, approx_path, approx_bin, approx_tt_bin,
                max_pattern_error, num_random_starts,
                exact_f, base, N, exact_shor["r"], num_shots)
            results.append(entry)

    _, n_in, n_out = generate_modexp_truth_table(base, N, exp_bits)
    return {
        "base": base, "N": N, "exp_bits": exp_bits,
        "n_in": n_in, "n_out": n_out,
        "baseline_and": baseline_and,
        "exact_period": exact_shor["r"],
        "exact_factors": (exact_shor["p"], exact_shor["q"]),
        "methods": methods,
        "sweep": results,
    }


def print_case_table(case: dict[str, Any]) -> None:
    print(f"\n=== base={case['base']} N={case['N']} exp_bits={case['exp_bits']} "
          f"(n_in={case['n_in']}, n_out={case['n_out']}, "
          f"baseline AND={case['baseline_and']}, "
          f"true period={case['exact_period']}, "
          f"factors={case['exact_factors']}) ===")
    print(f"{'eb':>5} {'method':>8} {'AND':>9} {'chg':>5} "
          f"{'snr':>6} {'margin':>6} {'P(ok)':>6} {'E[trials]':>10} "
          f"{'det_ok':>6}")
    for r in case["sweep"]:
        if r.get("failed"):
            print(f"{r['eb']:>5} {r['method']:>8} FAILED: {r.get('stderr','')[:60]}")
            continue
        snr = r["period_snr"]["snr"]
        margin = r["period_snr"]["margin"]
        psr = r["shor_shots"]["success_rate"]
        et = r["shor_shots"]["expected_trials"]
        et_str = "∞" if math.isinf(et) else f"{et:.1f}"
        snr_str = "∞" if math.isinf(snr) else f"{snr:.2f}"
        det_ok = "✓" if r["shor"]["success"] else "✗"
        print(f"{r['eb']:>5} {r['method']:>8} "
              f"{r['and_before']:>4}→{r['and_after']:<4} "
              f"{r['patterns_changed']:>5} "
              f"{snr_str:>6} {margin:>6.2f} "
              f"{psr:>6.3f} {et_str:>10} {det_ok:>6}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cases", nargs="+", default=None,
        help="space-separated 'base,N,exp_bits' triples; default: preset")
    parser.add_argument("--eb", nargs="+", type=float, default=None,
        help="error bounds to sweep; default: 0 0.01 0.05 0.1 0.3 1.0")
    parser.add_argument("--workdir", type=Path,
        default=_project_root / "results" / "shor_e2e")
    parser.add_argument("--synth", type=Path,
        default=_project_root / "build" / "synth-tt")
    parser.add_argument("--approx", type=Path,
        default=_project_root / "build" / "approx-xag")
    parser.add_argument("--output", type=Path, default=None,
        help="JSON output path; default workdir/shor_e2e.json")
    parser.add_argument("--max-pattern-error", type=int, default=0,
        help="Cap LACs whose max single-pattern integer error exceeds this (0 = disabled)")
    parser.add_argument("--methods", nargs="+",
        default=["narrow"],
        choices=["narrow", "resubals", "ilp"],
        help="Approximators to compare per eb point")
    parser.add_argument("--approx-tt", type=Path,
        default=_project_root / "build" / "approx-tt",
        help="Path to approx-tt binary (ILP)")
    parser.add_argument("--num-shots", type=int, default=1000,
        help="Shor's quantum shots simulated per eb point")
    parser.add_argument("--num-random-starts", type=int, default=4,
        help="SS synthesizer random starts for exact baseline (min across 1 seed)")
    args = parser.parse_args()

    if args.cases:
        cases = []
        for c in args.cases:
            b, N, e = (int(x) for x in c.split(","))
            cases.append((b, N, e))
    else:
        cases = DEFAULT_CASES
    ebs = args.eb if args.eb is not None else DEFAULT_EB

    args.workdir.mkdir(parents=True, exist_ok=True)
    all_results: list[dict[str, Any]] = []
    for base, N, exp_bits in cases:
        case_dir = args.workdir / f"case_{base}_{N}_{exp_bits}"
        result = run_case(base, N, exp_bits, ebs, case_dir,
                          args.synth, args.approx, args.max_pattern_error,
                          methods=args.methods, approx_tt_bin=args.approx_tt,
                          num_shots=args.num_shots,
                          num_random_starts=args.num_random_starts)
        if result is None:
            continue
        all_results.append(result)
        print_case_table(result)

    out_path = args.output or (args.workdir / "shor_e2e.json")
    out_path.write_text(json.dumps(all_results, indent=2))
    print(f"\nWrote {out_path}")


if __name__ == "__main__":
    main()
