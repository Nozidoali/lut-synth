"""Measure how much AND reduction is achievable by synthesis alone (no approx).

For each modexp case, runs `synth-tt` at several --num-random-starts values
and compares the minimum exact AND count against narrow-approximation
results at a given eb. Tells us how much of "narrow's savings" is really
just "1-start SS is suboptimal".

Usage:
    python scripts/synth_baseline.py
    python scripts/synth_baseline.py --cases 5,33,8 3,35,8 --starts 1 4 16 64
"""
from __future__ import annotations

import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))

from modexp import generate_modexp_truth_table  # noqa: E402
from truthTable import TruthTable  # noqa: E402


def synth_and_count(tt_path: Path, synth_bin: Path,
                    num_random_starts: int, seed: int) -> dict:
    cmd = [str(synth_bin), "--input", str(tt_path),
           "--num-random-starts", str(num_random_starts),
           "--seed", str(seed)]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return json.loads(proc.stdout.strip())


def approx_narrow(tt_path: Path, approx_bin: Path, eb: float) -> dict:
    cmd = [str(approx_bin), "--input", str(tt_path),
           "--output", "/tmp/_approx_dummy.tt",
           "--method", "narrow", "--error-bound", str(eb)]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return json.loads(proc.stdout.strip())


def run_monolithic(base: int, N: int, exp_bits: int,
                   starts_list: list[int], ebs: list[float],
                   workdir: Path, synth_bin: Path, approx_bin: Path) -> dict:
    n_out = math.ceil(math.log2(N))
    patterns, _, _ = generate_modexp_truth_table(base, N, exp_bits)
    tt_path = workdir / f"modexp_{base}_{N}_{exp_bits}_{n_out}.tt"
    TruthTable.from_patterns(patterns, num_inputs=exp_bits).to_file(str(tt_path))

    synth_rows = []
    for starts in starts_list:
        best_and = None
        for seed in range(5):
            res = synth_and_count(tt_path, synth_bin, starts, seed)
            if best_and is None or res["and_count"] < best_and:
                best_and = res["and_count"]
        synth_rows.append({"starts": starts, "best_and": best_and})

    approx_rows = []
    for eb in ebs:
        res = approx_narrow(tt_path, approx_bin, eb)
        approx_rows.append({
            "eb": eb,
            "and_before": res["and_before"],
            "and_after": res["and_after"],
            "lacs_applied": res["lacs_applied"],
        })

    return {
        "base": base, "N": N, "exp_bits": exp_bits,
        "flavor": "monolithic",
        "synth_sweep": synth_rows,
        "narrow_sweep": approx_rows,
    }


def _window_tt_path(workdir: Path, base: int, N: int, i: int,
                    window_bit: int, window_pos: int) -> Path:
    path = workdir / f"lut_w{i}.tt"
    n_out = max(1, math.ceil(math.log2(N)))
    step = pow(base, 1 << window_pos, N)
    L_win = 1 << window_bit
    outputs = [[] for _ in range(n_out)]
    for v in range(L_win):
        val = pow(step, v, N)
        bits = format(val, f"0{n_out}b")
        for j, b in enumerate(bits[::-1]):
            outputs[j].append(b)
    patterns = ["".join(col) for col in outputs]
    TruthTable.from_patterns(patterns, num_inputs=window_bit).to_file(str(path))
    return path


def run_chained(base: int, N: int, w: int, m_total: int,
                starts_list: list[int], ebs: list[float],
                workdir: Path, synth_bin: Path, approx_bin: Path) -> dict:
    k = math.ceil(m_total / w)
    workdir.mkdir(parents=True, exist_ok=True)

    synth_rows = []
    for starts in starts_list:
        total_best = 0
        for i in range(k):
            p = _window_tt_path(workdir, base, N, i, w, i * w)
            best = None
            for seed in range(5):
                res = synth_and_count(p, synth_bin, starts, seed)
                if best is None or res["and_count"] < best:
                    best = res["and_count"]
            total_best += best
        synth_rows.append({"starts": starts, "best_and_sum": total_best})

    approx_rows = []
    for eb in ebs:
        total_before = 0
        total_after = 0
        for i in range(k):
            p = workdir / f"lut_w{i}.tt"
            res = approx_narrow(p, approx_bin, eb)
            total_before += res["and_before"]
            total_after += res["and_after"]
        approx_rows.append({
            "eb": eb,
            "and_before_sum": total_before,
            "and_after_sum": total_after,
        })

    return {
        "base": base, "N": N, "w": w, "m_total": m_total,
        "flavor": "chained", "k_windows": k,
        "synth_sweep": synth_rows,
        "narrow_sweep": approx_rows,
    }


def print_table(row: dict) -> None:
    if row["flavor"] == "monolithic":
        label = f"(base={row['base']}, N={row['N']}, exp_bits={row['exp_bits']})  monolithic"
    else:
        label = (f"(base={row['base']}, N={row['N']}, w={row['w']}, "
                 f"m_total={row['m_total']}, k={row['k_windows']})  chained")
    print(f"\n=== {label} ===")
    print("  # exact synthesis only (no approximation):")
    ref = None
    for s in row["synth_sweep"]:
        key = "best_and" if row["flavor"] == "monolithic" else "best_and_sum"
        val = s[key]
        if ref is None:
            ref = val
            delta = 0
        else:
            delta = val - ref
        print(f"    {s['starts']:>4} random starts (× 5 seeds) → AND={val}"
              f"   (Δ vs 1-start: {delta:+d})")

    baseline = row["synth_sweep"][0][
        "best_and" if row["flavor"] == "monolithic" else "best_and_sum"]
    best_exact = min(
        s["best_and" if row["flavor"] == "monolithic" else "best_and_sum"]
        for s in row["synth_sweep"])
    exact_gain = baseline - best_exact
    print(f"  synthesis-only gain = {exact_gain} AND "
          f"({100*exact_gain/baseline:.1f}%)")

    print("  # narrow (functional approximation, 1 random start baseline):")
    for a in row["narrow_sweep"]:
        before = a.get("and_before") or a.get("and_before_sum")
        after = a.get("and_after") or a.get("and_after_sum")
        saved = before - after
        print(f"    eb={a['eb']:>4}  AND {before}→{after}  saved {saved} "
              f"({100*saved/before:.1f}%)")


def run_audit(cases: list[str], starts_list: list[int], ebs: list[float],
              workdir: Path, synth_bin: Path, approx_bin: Path
              ) -> list[dict[str, Any]]:
    """cases: each is 'mono:base,N,exp_bits' or 'chained:base,N,w,m_total'."""
    workdir.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, Any]] = []
    for c in cases:
        kind, spec = c.split(":")
        parts = [int(x) for x in spec.split(",")]
        if kind == "mono":
            b, N, exp_bits = parts
            case_dir = workdir / f"mono_{b}_{N}_{exp_bits}"
            case_dir.mkdir(parents=True, exist_ok=True)
            row = run_monolithic(b, N, exp_bits, starts_list, ebs, case_dir,
                                 synth_bin, approx_bin)
        elif kind == "chained":
            b, N, w, m = parts
            case_dir = workdir / f"chained_{b}_{N}_w{w}_m{m}"
            case_dir.mkdir(parents=True, exist_ok=True)
            row = run_chained(b, N, w, m, starts_list, ebs, case_dir,
                              synth_bin, approx_bin)
        else:
            raise ValueError(f"unknown kind {kind}; expected mono|chained")
        rows.append(row)
        print_table(row)
    return rows
