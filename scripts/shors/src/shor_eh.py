"""Ekera-Hastad-style multi-shot order finding on approximated modexp.

Standard Shor recovers the order r from a single QFT measurement j by
the continued-fraction convergent of j/L; one shot succeeds with
probability roughly phi(r)/r when the QFT signal is clean.

EH-style multi-shot (the order-finding analog of Ekera-Hastad's
short-exponent algorithm) collects s >= 2 independent measurements
j_1, ..., j_s, runs continued fractions on each to produce candidate
denominators d_1, ..., d_s, and combines them by

    r_guess = lcm(d_1, ..., d_s)  capped at N

If the true r divides every d_i (which is often the case even when no
single d_i equals r), the lcm recovers r exactly. This trades quantum
shots for classical post-processing and is robust to per-shot errors
that drop a single shot's denominator below r.

For each (case, eb, s), this runs num_batches Monte-Carlo trials,
each consisting of s shots from the QFT distribution of the
approximated f, and reports the fraction of trials that recover the
true order.

Inputs are the chained_eb*.f.json files produced by shor_chained.py.

Usage:
  python scripts/shor_eh.py \\
      --workdir results/shor_chained \\
      --output results/shor_chained/shor_eh.json \\
      --shots-per-batch 1 2 3 5 \\
      --num-batches 2000
"""
from __future__ import annotations

import json
from collections import defaultdict
from fractions import Fraction
from math import gcd
from pathlib import Path
from typing import Iterable

import numpy as np


def lcm(a: int, b: int) -> int:
    if a == 0 or b == 0:
        return 0
    return a * b // gcd(a, b)


def lcm_list(xs):
    out = 1
    for x in xs:
        if x <= 0:
            continue
        out = lcm(out, x)
    return out


def shor_j_distribution(f: np.ndarray) -> np.ndarray:
    L = len(f)
    match = np.empty(L, dtype=np.float64)
    for delta in range(L):
        match[delta] = float(np.mean(f == np.roll(f, -delta)))
    amp = np.fft.fft(match)
    prob = np.real(amp) / L
    prob = np.clip(prob, 0.0, None)
    prob[0] = 0.0
    s = prob.sum()
    if s <= 0:
        return prob
    return prob / s


def trial_success(js, L, base, N, r_true, max_denom):
    cands = []
    for j in js:
        if j == 0:
            cands.append(0)
            continue
        d = Fraction(int(j), int(L)).limit_denominator(max_denom).denominator
        cands.append(d)
    r_guess = lcm_list(cands)
    if r_guess == 0 or r_guess > max_denom:
        return False, "lcm_fail"
    if pow(int(base), int(r_guess), int(N)) != 1:
        return False, "not_order"
    if r_guess % 2:
        return False, "odd_r"
    half = pow(int(base), r_guess // 2, int(N))
    if half == N - 1:
        return False, "trivial_gcd"
    return True, "ok"


def evaluate_case(f_path: Path, shots_per_batch, num_batches, seed):
    data = json.loads(f_path.read_text())
    base, N = int(data["base"]), int(data["N"])
    f = np.asarray(data["approx_f"], dtype=np.int64)
    L = len(f)
    rng = np.random.default_rng(seed)
    p = shor_j_distribution(f)
    if p.sum() <= 0:
        return {s: {"success_rate": 0.0,
                     "successes": 0,
                     "num_batches": num_batches,
                     "failure_reasons": {"no_signal": num_batches}}
                for s in shots_per_batch}

    r_true = int(data.get("exact_period") or 0)
    if not r_true:
        from math import gcd as _gcd
        v = base % N
        r_true = 1
        while v != 1 and r_true < 200:
            v = (v * base) % N
            r_true += 1
        if v != 1:
            r_true = 0
    out = {}
    for s in shots_per_batch:
        s = int(s)
        succ = 0
        reasons = defaultdict(int)
        # batch s draws
        all_js = rng.choice(L, size=s * num_batches, p=p)
        for b in range(num_batches):
            js = all_js[b * s:(b + 1) * s]
            ok, why = trial_success(js, L, base, N, r_true, N)
            if ok:
                succ += 1
            else:
                reasons[why] += 1
        out[s] = {
            "success_rate": succ / num_batches,
            "successes": succ,
            "num_batches": num_batches,
            "failure_reasons": dict(reasons),
        }
    return out


def run_eh_on_workdir(workdir: Path, shots_per_batch: Iterable[int],
                      num_batches: int, seed: int) -> list[dict]:
    """Scan workdir/case_*/chained_eb*.f.json and run EH analysis on each."""
    summary: list[dict] = []
    for case_dir in sorted(workdir.glob("case_*")):
        for f_path in sorted(case_dir.glob("chained_eb*.f.json")):
            data_pre = json.loads(f_path.read_text())
            base, N = data_pre["base"], data_pre["N"]
            w, m = data_pre["w"], data_pre["m_total"]
            eb = data_pre["eb"]
            print(f"({base},{N}) w={w} m={m} eb={eb}: ", end="", flush=True)
            res = evaluate_case(f_path, list(shots_per_batch),
                                num_batches, seed)
            short = ", ".join(
                f"s={s}:{r['success_rate']:.3f}" for s, r in res.items())
            print(short)
            summary.append({
                "base": base, "N": N, "w": w, "m_total": m, "eb": eb,
                "exact_period": int(data_pre.get("exact_period") or 0),
                "eh": {str(k): v for k, v in res.items()},
            })
    return summary
