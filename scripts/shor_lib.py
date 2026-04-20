"""Shared helpers for Shor's e2e / chained / qiskit scripts.

Pure computation only — no subprocess, no CLI, no plotting. Move functions
here if they're used in more than one top-level driver script.
"""
from __future__ import annotations

import json
import math
import sys
from fractions import Fraction
from pathlib import Path
from typing import Any

import numpy as np

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))

from truthTable import TruthTable  # noqa: E402


# ---------- TT decoding ----------
def decode_f_values(tt: TruthTable) -> list[int]:
    """Return [f(0), ..., f(2^n − 1)] assuming row 0 is LSB of output integer.

    Don't-care entries ("X") are treated as 0.
    """
    length = 1 << tt.num_inputs
    out: list[int] = []
    for x in range(length):
        val = 0
        for j in range(tt.num_outputs):
            ch = tt.table[j][x]
            if ch == "X":
                ch = "0"
            if ch == "1":
                val |= 1 << j
        out.append(val)
    return out


def load_f_from_tt(path: Path | str) -> list[int]:
    return decode_f_values(TruthTable.from_file(str(path)))


def load_f_from_chained_dump(path: Path | str) -> list[int]:
    """Read a chained_ebX.f.json dump produced by shor_chained.py."""
    data = json.loads(Path(path).read_text())
    return data["approx_f"]


def load_f_at_eb(case_dir: Path | str, base: int, N: int,
                 eb: float | str) -> list[int]:
    """Resolve monolithic .tt or chained .f.json at the given case_dir/eb."""
    case_dir = Path(case_dir)
    mono = case_dir / f"approx_narrow_{base}_{N}_eb{eb}.tt"
    if mono.exists():
        return load_f_from_tt(mono)
    chained = case_dir / f"chained_eb{eb}.f.json"
    if chained.exists():
        return load_f_from_chained_dump(chained)
    raise FileNotFoundError(f"neither {mono} nor {chained}")


# ---------- Period analysis ----------
def period_autocorrelation(f, r: int) -> float:
    L = len(f)
    if r == 0 or r >= L:
        return 0.0
    arr = np.asarray(f) if not isinstance(f, np.ndarray) else f
    return float((arr[: L - r] == arr[r:]).mean())


def period_snr(f: list[int], true_r: int,
               max_sample_r: int = 4096) -> dict[str, Any]:
    """Signal/noise from period autocorrelation.

    Strides r samples at ≤ ``max_sample_r`` points across [1, L) to keep
    cost at O(max_sample_r · L) instead of O(L²) for large L.
    """
    L = len(f)
    signal = period_autocorrelation(f, true_r)
    r_upper = min(L, max_sample_r)
    stride = max(1, (L - 1) // r_upper)
    noise_sum = 0.0
    noise_n = 0
    best_other_r = 0
    best_other_val = 0.0
    r = 1
    while r < L:
        if r % true_r != 0:
            v = period_autocorrelation(f, r)
            noise_sum += v
            noise_n += 1
            if v > best_other_val:
                best_other_val = v
                best_other_r = r
        r += stride
    noise = noise_sum / noise_n if noise_n else 0.0
    return {
        "autocorr_at_true_r": signal,
        "mean_autocorr_noise": noise,
        "snr": signal / noise if noise > 0 else float("inf"),
        "best_spurious_r": best_other_r,
        "best_spurious_autocorr": best_other_val,
        "margin": signal - best_other_val,
        "r_samples": noise_n,
    }


def find_exact_period(base: int, N: int) -> int | None:
    for r in range(2, N):
        if pow(base, r, N) == 1:
            return r
    return None


# ---------- Shor post-processing ----------
def continued_fraction_denominator(j: int, L: int, N: int) -> int:
    """Best rational approximation of j/L with denominator ≤ N."""
    if j == 0:
        return 0
    return Fraction(j, L).limit_denominator(N).denominator


def try_factor_from_period(base: int, N: int, r: int) -> dict[str, Any]:
    """Given a candidate period r, attempt gcd(base^{r/2} ± 1, N) factoring.

    Returns {"success": bool, "r": r, "p": p, "q": q, "reason": str?}.
    Requires r > 0, r even, and base^r ≡ 1 (mod N). The "reason" field is
    set when the attempt fails.
    """
    if r == 0 or r > N:
        return {"success": False, "reason": "no_fraction", "r": r}
    if pow(base, r, N) != 1:
        return {"success": False, "reason": "r_not_order", "r": r}
    if r % 2 != 0:
        return {"success": False, "reason": "r_odd", "r": r}
    half = pow(base, r // 2, N)
    p = math.gcd(half - 1, N)
    q = math.gcd(half + 1, N)
    if 1 < p < N and 1 < q < N and p * q == N:
        return {"success": True, "r": r, "p": p, "q": q}
    return {"success": False, "reason": "trivial_gcd", "r": r}


def shor_one_shot(base: int, N: int, f: list[int],
                  rng: np.random.Generator) -> dict[str, Any]:
    """One textbook-Shor's shot via numpy FFT on a corrupted f̃ table."""
    L = len(f)
    arr = np.array(f)
    vals, counts = np.unique(arr, return_counts=True)
    probs = counts / L
    v = int(rng.choice(vals, p=probs))

    indicator = np.zeros(L, dtype=np.float64)
    mask = arr == v
    k = int(mask.sum())
    indicator[mask] = 1.0 / math.sqrt(k)
    amp = np.fft.fft(indicator) / math.sqrt(L)
    pj = np.abs(amp) ** 2
    s = pj.sum()
    if s <= 0:
        return {"success": False, "reason": "zero_amp"}
    pj = pj / s
    j = int(rng.choice(L, p=pj))

    r = continued_fraction_denominator(j, L, N)
    res = try_factor_from_period(base, N, r)
    res["j"] = j
    return res


def shor_shot_statistics(base: int, N: int, f: list[int],
                         num_shots: int = 1000,
                         seed: int = 0) -> dict[str, Any]:
    rng = np.random.default_rng(seed)
    succ = 0
    reasons: dict[str, int] = {}
    for _ in range(num_shots):
        res = shor_one_shot(base, N, f, rng)
        if res["success"]:
            succ += 1
        else:
            reasons[res["reason"]] = reasons.get(res["reason"], 0) + 1
    p_succ = succ / num_shots
    return {
        "num_shots": num_shots,
        "successes": succ,
        "success_rate": p_succ,
        "expected_trials": float("inf") if p_succ == 0 else 1.0 / p_succ,
        "failure_reasons": reasons,
    }


def shor_factor(base: int, N: int, f: list[int],
                min_match: float = 0.5) -> dict[str, Any]:
    """Deterministic factoring: scan even r values, accept if majority of
    f̃(x) == f̃(x+r) AND gcd check yields nontrivial factor pair.
    """
    n = len(f)
    for r in range(2, n // 2 + 1):
        if r % 2 != 0:
            continue
        matches = sum(1 for x in range(n - r) if f[x] == f[x + r])
        if (n - r) <= 0 or matches / (n - r) < min_match:
            continue
        out = try_factor_from_period(base, N, r)
        if out["success"]:
            out["match_frac"] = matches / (n - r)
            return out
    return {"success": False, "r": 0, "match_frac": 0.0, "p": 0, "q": 0}
