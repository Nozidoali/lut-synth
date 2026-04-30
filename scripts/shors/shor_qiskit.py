"""qiskit-based Shor's e2e on approximated modexp LUTs.

Two circuit modes:

  --mode reduced   (default, scales to larger cases)
      Per shot: classically sample f-measurement outcome v from f̃'s
      value distribution, build the post-measurement x-state
        |ψ_v⟩ = (1/√|C_v|) Σ_{x ∈ C_v} |x⟩     with C_v = {x : f̃(x) = v}
      qiskit circuit on n_in qubits: Initialize(|ψ_v⟩) → QFT⁻¹ → measure
      Quantum part (state prep + inverse QFT + measurement) is simulated
      by AerSimulator; classical part (continued fraction + gcd) runs
      on numpy/stdlib.

  --mode full      (validation on small cases)
      Build the full entangled oracle U_f as multi-controlled X gates:
        |x⟩|y⟩ → |x⟩|y ⊕ f̃(x)⟩
      Hadamard → U_f → QFT⁻¹ on x → measure x.
      Uses 2^n_in * n_out MCX operations; only tractable for n_in+n_out ≤ 12.

Both modes feed into the same classical Shor's post-processing
(continued-fraction expansion of j/L, then gcd(base^{r/2} ± 1, N)).
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from fractions import Fraction
from pathlib import Path

import numpy as np
from qiskit import ClassicalRegister, QuantumCircuit, QuantumRegister, transpile
from qiskit.circuit.library import QFT
from qiskit_aer import AerSimulator

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "third-party" / "qlut-benchmarks" / "src"))
sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "shors" / "src"))

from truthTable import TruthTable  # noqa: E402
from shor_lib import (                             # noqa: E402
    decode_f_values, period_snr, continued_fraction_denominator,
    try_factor_from_period,
)


def classical_shor_from_j(base: int, N: int, j: int, L: int) -> dict:
    r = continued_fraction_denominator(j, L, N)
    return try_factor_from_period(base, N, r)


def _build_reduced_circuit(class_state: np.ndarray, n_in: int) -> QuantumCircuit:
    x_reg = QuantumRegister(n_in, "x")
    c_reg = ClassicalRegister(n_in, "c")
    qc = QuantumCircuit(x_reg, c_reg)
    qc.initialize(class_state, x_reg)
    qc.append(QFT(n_in, inverse=True, do_swaps=True), x_reg)
    qc.measure(x_reg, c_reg)
    return qc


def run_reduced_qiskit(f: list[int], n_in: int, base: int, N: int,
                       shots: int, seed: int = 0) -> dict:
    """Classical f-measurement + qiskit state prep + inverse QFT + measure.

    Shots are grouped by the sampled f-value v: for each unique v, we
    build one circuit and run all shots_for_v shots in a single batched
    call. This avoids transpile overhead per shot.
    """
    L = 1 << n_in
    arr = np.asarray(f)
    vals, counts = np.unique(arr, return_counts=True)
    probs = counts / L

    rng = np.random.default_rng(seed)
    sim = AerSimulator(method="statevector")
    per_shot_v = rng.choice(len(vals), size=shots, p=probs)
    bincount = np.bincount(per_shot_v, minlength=len(vals))

    succ = 0
    reasons: dict[str, int] = {}
    for v_idx, n_v in enumerate(bincount):
        if n_v == 0:
            continue
        v = int(vals[v_idx])
        mask = (arr == v)
        k = int(mask.sum())
        if k == 0:
            reasons["empty_class"] = reasons.get("empty_class", 0) + int(n_v)
            continue
        state = np.zeros(L, dtype=np.complex128)
        state[mask] = 1.0 / math.sqrt(k)
        qc = _build_reduced_circuit(state, n_in)
        tqc = transpile(qc, sim, seed_transpiler=seed + v_idx)
        result = sim.run(tqc, shots=int(n_v),
                         seed_simulator=seed + v_idx).result()
        counts_j = result.get_counts()
        for bit_str, cnt in counts_j.items():
            j = int(bit_str.replace(" ", ""), 2)
            res = classical_shor_from_j(base, N, j, L)
            if res["success"]:
                succ += cnt
            else:
                reasons[res["reason"]] = reasons.get(res["reason"], 0) + cnt

    p = succ / shots
    return {
        "shots": shots,
        "successes": succ,
        "success_rate": p,
        "expected_trials": float("inf") if p == 0 else 1.0 / p,
        "failure_reasons": reasons,
        "mode": "reduced",
        "num_qubits": n_in,
    }


def _build_full_circuit(f: list[int], n_in: int, n_out: int) -> QuantumCircuit:
    x_reg = QuantumRegister(n_in, "x")
    y_reg = QuantumRegister(n_out, "y")
    c_reg = ClassicalRegister(n_in, "cx")
    qc = QuantumCircuit(x_reg, y_reg, c_reg)

    qc.h(x_reg)
    mask_out = (1 << n_out) - 1
    for x_val in range(1 << n_in):
        fv = f[x_val] & mask_out
        if fv == 0:
            continue
        # Flip x-bits that are 0 in x_val so MCX fires on this x pattern
        flip_x = [i for i in range(n_in) if not ((x_val >> i) & 1)]
        for i in flip_x:
            qc.x(x_reg[i])
        for j in range(n_out):
            if (fv >> j) & 1:
                qc.mcx(list(x_reg), y_reg[j])
        for i in flip_x:
            qc.x(x_reg[i])

    qc.append(QFT(n_in, inverse=True, do_swaps=True), x_reg)
    qc.measure(x_reg, c_reg)
    return qc


def run_full_qiskit(f: list[int], n_in: int, n_out: int,
                    base: int, N: int, shots: int, seed: int = 0) -> dict:
    qc = _build_full_circuit(f, n_in, n_out)
    sim = AerSimulator(method="statevector")
    tqc = transpile(qc, sim, seed_transpiler=seed)
    result = sim.run(tqc, shots=shots, seed_simulator=seed).result()
    counts = result.get_counts()

    L = 1 << n_in
    succ = 0
    reasons: dict[str, int] = {}
    for bitstring, count in counts.items():
        j = int(bitstring.replace(" ", ""), 2)
        res = classical_shor_from_j(base, N, j, L)
        if res["success"]:
            succ += count
        else:
            reasons[res["reason"]] = reasons.get(res["reason"], 0) + count
    p = succ / shots
    return {
        "shots": shots,
        "successes": succ,
        "success_rate": p,
        "expected_trials": float("inf") if p == 0 else 1.0 / p,
        "failure_reasons": reasons,
        "mode": "full",
        "num_qubits": n_in + n_out,
    }


def run_case(base: int, N: int, exp_bits: int, tt_dir: Path,
             ebs: list[float], shots: int, mode: str) -> dict:
    n_out = math.ceil(math.log2(N))
    exact_tt_path = tt_dir / f"modexp_{base}_{N}_{exp_bits}_{n_out}.tt"
    exact_f = decode_f_values(TruthTable.from_file(str(exact_tt_path)))
    r_star = None
    for r in range(2, N):
        if pow(base, r, N) == 1:
            r_star = r
            break
    print(f"  exact_period={r_star}  n_in={exp_bits} n_out={n_out}")

    results = []
    for eb in ebs:
        p_tt = tt_dir / f"approx_narrow_{base}_{N}_eb{eb}.tt"
        if not p_tt.exists():
            print(f"  skip eb={eb}: {p_tt} not found")
            continue
        f = decode_f_values(TruthTable.from_file(str(p_tt)))
        snr = period_snr(f, r_star) if r_star else None
        if mode == "full":
            shots_res = run_full_qiskit(f, exp_bits, n_out, base, N, shots)
        else:
            shots_res = run_reduced_qiskit(f, exp_bits, base, N, shots)
        ch = sum(1 for a, b in zip(f, exact_f) if a != b)
        results.append({
            "eb": eb,
            "patterns_changed": ch,
            "period_snr_margin": snr["margin"] if snr else None,
            "qiskit_shots": shots_res,
        })
        r_ = shots_res
        print(f"  eb={eb:>4}  chg={ch:>4}  P(ok)={r_['success_rate']:.3f}"
              f"  E[tr]={r_['expected_trials']:8.2f}"
              f"  qubits={r_['num_qubits']}")
    return {
        "base": base, "N": N, "exp_bits": exp_bits,
        "mode": mode,
        "exact_period": r_star,
        "sweep": results,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", nargs="+", default=["2,21,6", "5,33,8", "3,35,8"])
    ap.add_argument("--eb", nargs="+", type=float,
                    default=[0.0, 0.1, 0.3, 0.5, 1.0, 2.0])
    ap.add_argument("--shots", type=int, default=1000)
    ap.add_argument("--mode", choices=["reduced", "full"], default="reduced")
    ap.add_argument("--input-root", type=Path,
                    default=PROJECT_ROOT / "results" / "shor_quant")
    ap.add_argument("--output", type=Path, default=None)
    args = ap.parse_args()

    out_path = args.output or (args.input_root / f"shor_qiskit_{args.mode}.json")
    all_out = []
    for c in args.cases:
        b, N, eb = (int(x) for x in c.split(","))
        case_dir = args.input_root / f"case_{b}_{N}_{eb}"
        if not case_dir.exists():
            print(f"skip ({b},{N},{eb}): {case_dir} not found")
            continue
        print(f"\n=== qiskit Shor [{args.mode}]: base={b} N={N} exp_bits={eb} ===")
        res = run_case(b, N, eb, case_dir, args.eb, args.shots, args.mode)
        all_out.append(res)
    out_path.write_text(json.dumps(all_out, indent=2))
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
