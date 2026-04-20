"""Chained windowed Shor's e2e: k per-window LUTs, each separately approximated.

Real Shor's modexp on an m-bit exponent uses windowed arithmetic
(Gidney 2019): the exponent is split into k = ⌈m/w⌉ windows of w bits,
and for each window i a LUT stores

    LUT_i[v] = base^(v · 2^(i·w)) mod N,    v ∈ [0, 2^w).

The quantum circuit chains them:

    |x⟩|1⟩ → |x⟩ · prod_{i=0}^{k-1} LUT_i[x_window_i] mod N

With all LUTs exact this returns base^x mod N. When each LUT is
approximated (narrow-resub) independently, the chained output
f̃(x) differs from both the exact f AND a monolithic approximation
of the same f, because per-window bit flips compound through
mod-multiplication.

Pipeline per case:
  1. Generate k .tt files (one per window)
  2. Synthesize each → XAG, baseline AND count
  3. For each eb, run approx-xag --method narrow on each
  4. Read back approximated LUTs, compose f̃(x) chained
  5. Run shot-based analysis (numpy FFT or qiskit) → P(ok), E[trials]
  6. Report total AND saved across all LUTs + Shor's metrics

Comparison against monolithic (single 2^m-entry LUT) is available by
rerunning shor_e2e.py on the same (base, N, m) — different f̃ structure.
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any

import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from truthTable import TruthTable  # noqa: E402
from shor_lib import (                             # noqa: E402
    decode_f_values, period_snr, shor_factor, shor_shot_statistics,
    find_exact_period,
)


def generate_window_lut_patterns(base: int, N: int, window_bit: int,
                                 window_pos: int) -> tuple[list[str], int]:
    """Return (patterns, n_out) for LUT_i[v] = base^(v · 2^(i·w)) mod N.

    Patterns follow modexp.py convention: row j is LSB j of the output,
    one column per input pattern v ∈ [0, 2^window_bit).
    """
    n_out = max(1, math.ceil(math.log2(N)))
    L_win = 1 << window_bit
    outputs: list[list[str]] = [[] for _ in range(n_out)]
    step = pow(base, 1 << window_pos, N)
    for v in range(L_win):
        val = pow(step, v, N)
        bits = format(val, f"0{n_out}b")
        for j, b in enumerate(bits[::-1]):
            outputs[j].append(b)
    return ["".join(col) for col in outputs], n_out


def write_window_tt(path: Path, patterns: list[str], n_in: int) -> None:
    TruthTable.from_patterns(patterns, num_inputs=n_in).to_file(str(path))


def read_window_f(path: Path) -> list[int]:
    return decode_f_values(TruthTable.from_file(str(path)))


def compose_chained_f(luts: list[list[int]], w: int, m_total: int,
                      N: int) -> list[int]:
    """f̃(x) = product over windows of LUT_i[window_i] mod N for x ∈ [0, 2^m)."""
    L = 1 << m_total
    out = [0] * L
    k = len(luts)
    mask = (1 << w) - 1
    for x in range(L):
        acc = 1
        for i in range(k):
            shift = i * w
            if shift >= m_total:
                break
            v = (x >> shift) & mask
            if v >= len(luts[i]):
                v = v % len(luts[i])
            acc = (acc * luts[i][v]) % N
        out[x] = acc
    return out


def synth_and_approx_windows(
    base: int, N: int, w: int, m_total: int, eb,
    workdir: Path, synth_bin: Path, approx_bin: Path,
    method: str = "narrow", num_random_starts: int = 4,
) -> dict:
    """eb may be a float (shared across all windows) or a list of length k."""
    """For each window i, write its exact .tt, synthesize, approximate."""
    n_out = max(1, math.ceil(math.log2(N)))
    k = math.ceil(m_total / w)
    workdir.mkdir(parents=True, exist_ok=True)

    if isinstance(eb, (list, tuple)):
        assert len(eb) == k, f"per-window eb list must have length k={k}"
        ebs_per_window = list(eb)
    else:
        ebs_per_window = [float(eb)] * k

    per_window = []
    total_and_before = 0
    total_and_after = 0
    for i in range(k):
        eb_i = ebs_per_window[i]
        patterns, _ = generate_window_lut_patterns(base, N, w, i * w)
        exact_path = workdir / f"lut_w{i}.tt"
        write_window_tt(exact_path, patterns, w)

        synth_proc = subprocess.run(
            [str(synth_bin), "--input", str(exact_path),
             "--num-random-starts", str(num_random_starts)],
            capture_output=True, text=True, check=True)
        synth_stats = json.loads(synth_proc.stdout.strip())
        baseline_and_this = synth_stats.get("and_count", 0)

        approx_path = workdir / f"lut_w{i}_eb{eb_i}.tt"
        cmd = [str(approx_bin), "--input", str(exact_path),
               "--output", str(approx_path),
               "--method", method,
               "--error-bound", str(eb_i),
               "--num-random-starts", str(num_random_starts)]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True,
                                  check=True, timeout=60)
        except subprocess.TimeoutExpired:
            total_and_before += baseline_and_this
            total_and_after += baseline_and_this  # no reduction (fallback to exact)
            per_window.append({"i": i, "failed": True,
                               "reason": "approx timeout",
                               "and_before": baseline_and_this,
                               "and_after": baseline_and_this})
            continue
        except subprocess.CalledProcessError as exc:
            total_and_before += baseline_and_this
            total_and_after += baseline_and_this
            per_window.append({"i": i, "failed": True,
                               "reason": f"approx-xag crash (rc={exc.returncode})",
                               "stderr": (exc.stderr or "")[:200],
                               "and_before": baseline_and_this,
                               "and_after": baseline_and_this})
            continue
        stats = json.loads(proc.stdout.strip())
        total_and_before += stats.get("and_before", synth_stats.get("and_count", 0))
        total_and_after += stats.get("and_after", synth_stats.get("and_count", 0))
        per_window.append({
            "i": i,
            "eb": eb_i,
            "exact_path": str(exact_path),
            "approx_path": str(approx_path),
            "and_before": stats.get("and_before"),
            "and_after": stats.get("and_after"),
            "lacs": stats.get("lacs_applied"),
            "acc_err": stats.get("accumulated_error"),
        })
    return {
        "k_windows": k,
        "total_and_before": total_and_before,
        "total_and_after": total_and_after,
        "windows": per_window,
    }


def _eb_tag(eb):
    if isinstance(eb, (list, tuple)):
        return "pw_" + "_".join(f"{v:g}" for v in eb)
    return f"{eb:g}"


def _build_exact_chain(base: int, N: int, w: int, m_total: int,
                      workdir: Path) -> tuple[list[list[int]], list[int]]:
    exact_luts_dir = workdir / "exact"
    exact_luts_dir.mkdir(exist_ok=True)
    exact_luts: list[list[int]] = []
    for i in range(math.ceil(m_total / w)):
        p = exact_luts_dir / f"lut_w{i}.tt"
        patterns, _ = generate_window_lut_patterns(base, N, w, i * w)
        write_window_tt(p, patterns, w)
        exact_luts.append(read_window_f(p))
    exact_f = compose_chained_f(exact_luts, w, m_total, N)
    return exact_luts, exact_f


def _compose_approx_chain(approx_stats: dict, exact_luts: list[list[int]],
                          w: int, m_total: int, N: int) -> list[int]:
    luts_approx = []
    for win in approx_stats["windows"]:
        if win.get("failed"):
            luts_approx.append(exact_luts[win["i"]])
        else:
            luts_approx.append(read_window_f(Path(win["approx_path"])))
    return compose_chained_f(luts_approx, w, m_total, N)


def _sweep_single_eb(eb, base: int, N: int, w: int, m_total: int,
                     workdir: Path, synth_bin: Path, approx_bin: Path,
                     num_shots: int, num_random_starts: int,
                     exact_luts: list[list[int]], exact_f: list[int],
                     r_star: int | None) -> dict:
    eb_dir = workdir / f"eb{_eb_tag(eb)}"
    approx_stats = synth_and_approx_windows(
        base, N, w, m_total, eb, eb_dir, synth_bin, approx_bin,
        num_random_starts=num_random_starts)
    f_approx = _compose_approx_chain(approx_stats, exact_luts, w, m_total, N)

    f_dump_path = workdir / f"chained_eb{_eb_tag(eb)}.f.json"
    f_dump_path.write_text(json.dumps({
        "base": base, "N": N, "w": w, "m_total": m_total, "eb": eb,
        "exact_f": exact_f, "approx_f": f_approx,
    }))

    changed = sum(1 for a, b in zip(f_approx, exact_f) if a != b)
    max_err = max((abs(a - b) for a, b in zip(f_approx, exact_f)), default=0)
    snr = period_snr(f_approx, r_star) if r_star else None
    shots = shor_shot_statistics(base, N, f_approx, num_shots=num_shots)
    det = shor_factor(base, N, f_approx)

    row = {
        "eb": eb, "method": "narrow_chained",
        "and_before": approx_stats["total_and_before"],
        "and_after": approx_stats["total_and_after"],
        "and_saved": approx_stats["total_and_before"] - approx_stats["total_and_after"],
        "lacs": sum(w.get("lacs", 0) or 0 for w in approx_stats["windows"]),
        "acc_err": max(
            (w.get("acc_err", 0) or 0) for w in approx_stats["windows"]
        ) if approx_stats["windows"] else 0.0,
        "per_window": approx_stats["windows"],
        "patterns_changed": changed, "max_integer_err": max_err,
        "period_exact": r_star, "period_snr": snr,
        "shor_shots": shots, "shor": det, "f_dump": str(f_dump_path),
    }
    print(f"  eb={_eb_tag(eb):>14}  AND {row['and_before']}→"
          f"{row['and_after']}  chg={changed}  "
          f"P(ok)={shots['success_rate']:.3f}  "
          f"E[tr]={shots['expected_trials']:8.2f}  "
          f"det={'✓' if det['success'] else '✗'}")
    return row


def run_case(base: int, N: int, w: int, m_total: int, ebs: list[float],
             workdir: Path, synth_bin: Path, approx_bin: Path,
             num_shots: int, num_random_starts: int = 4) -> dict:
    workdir.mkdir(parents=True, exist_ok=True)
    r_star = find_exact_period(base, N)
    exact_luts, exact_f = _build_exact_chain(base, N, w, m_total, workdir)
    exact_shor = shor_factor(base, N, exact_f)
    print(f"  sanity: exact chained Shor {'✓' if exact_shor['success'] else '✗'} "
          f"r={exact_shor.get('r')}  r*={r_star}")

    results = [_sweep_single_eb(eb, base, N, w, m_total, workdir,
                                 synth_bin, approx_bin, num_shots,
                                 num_random_starts, exact_luts, exact_f,
                                 r_star)
               for eb in ebs]
    baseline_and = results[0]["and_before"] if results else 0

    return {
        "base": base, "N": N, "w": w, "m_total": m_total,
        "exp_bits": m_total,
        "n_in": m_total,
        "n_out": max(1, math.ceil(math.log2(N))),
        "baseline_and": baseline_and or 0,
        "exact_period": r_star,
        "exact_factors": (exact_shor.get("p"), exact_shor.get("q")),
        "methods": ["narrow_chained"],
        "flavor": "chained_windowed",
        "sweep": results,
    }


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
