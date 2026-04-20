"""Fault-tolerant resource model for narrow-approximated Shor's modexp.

Space-time volume analysis with explicit depth accounting:

  N_T(eb)      = T-gate count (≈ AND count × T_per_AND)
  D            = logical circuit depth (layers) ≈ N_T / parallelism
  Q            = active logical qubits ≈ 2·log₂(N) + ancilla
  E_trials(eb) = 1 / P(ok)           (from QFT Monte Carlo in shor_e2e.py)
  ε_cycle(d)   = A · (p_phys / p_th)^((d+1)/2)   (surface code)
  constraint   : Q · D · d · ε_cycle(d) ≤ ε_run_budget
                 → pick smallest odd integer d satisfying it
  V_shot       = Q · D · d³          (qubit-cycles × code area)
  V_factor     = E_trials · V_shot

The d³ factor comes from: D layers × d cycles/layer × d² physical
qubits per logical qubit × Q logical qubits. Cutting N_T reduces
depth D linearly AND can push d down discretely; both effects
multiply into the d³·N_T scaling.

Ratio V_factor(eb) / V_factor(eb=0) < 1 means approximation nets savings
AFTER paying the E[trials] tax.

Plots are saved under results/shor_quant/plots/.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def cycle_error(d: int, p_phys: float, p_th: float, A: float) -> float:
    return A * (p_phys / p_th) ** ((d + 1) / 2.0)


def required_distance(Q: int, D: int, eps_budget: float,
                      p_phys: float, p_th: float, A: float,
                      d_min: int = 3, d_max: int = 199) -> int:
    """Smallest odd d with Q · D · d · ε_cycle(d) ≤ eps_budget.

    Error budget: Q logical qubits × D depth layers × d cycles/layer,
    each qubit-cycle contributes ε_cycle(d) logical error.
    """
    d = d_min if d_min % 2 == 1 else d_min + 1
    while d <= d_max:
        if Q * D * d * cycle_error(d, p_phys, p_th, A) <= eps_budget:
            return d
        d += 2
    return d_max


def v_factor(Q: int, D: int, d: int, E_trials: float) -> float:
    """V = E_trials × Q × D × d^3 (physical qubit-cycles)."""
    if math.isinf(E_trials):
        return float("inf")
    return E_trials * Q * D * (d ** 3)


def analyze_case(case: dict, params: dict) -> list[dict]:
    t_per_and = params["t_per_and"]
    parallelism = params["parallelism"]
    N = case["N"]
    Q = max(3, 2 * max(1, int(math.ceil(math.log2(N)))) + params["ancilla"])
    rows = []
    base_row = None
    for r in case["sweep"]:
        if r.get("failed"):
            continue
        p = r["shor_shots"]["success_rate"]
        if p <= 0:
            continue
        N_T = r["and_after"] * t_per_and
        D = max(1, int(math.ceil(N_T / parallelism)))
        E_trials = 1.0 / p
        d = required_distance(Q, D, params["eps_budget"],
                              params["p_phys"], params["p_th"], params["A"])
        V = v_factor(Q, D, d, E_trials)
        row = {
            "eb": r["eb"],
            "and_after": r["and_after"],
            "N_T": N_T,
            "D_depth": D,
            "Q": Q,
            "E_trials": E_trials,
            "d": d,
            "V_shot": Q * D * (d ** 3),
            "V_factor": V,
        }
        rows.append(row)
        if base_row is None and r["eb"] == 0.0:
            base_row = row

    if base_row is None and rows:
        base_row = rows[0]
    if base_row:
        for r in rows:
            r["V_factor_ratio"] = r["V_factor"] / base_row["V_factor"]
    return rows


def print_report(cases_out: list[tuple[dict, list[dict]]], params: dict) -> None:
    print(f"FTQC resource model parameters:")
    print(f"  physical error p_phys = {params['p_phys']:.0e}")
    print(f"  threshold      p_th   = {params['p_th']:.0e}")
    print(f"  prefactor      A      = {params['A']}")
    print(f"  run budget     ε_run  = {params['eps_budget']}")
    print(f"  T per AND             = {params['t_per_and']}")
    print()
    for case, rows in cases_out:
        label = f"(base={case['base']}, N={case['N']}, exp_bits={case['exp_bits']})"
        Q_disp = rows[0]["Q"] if rows else 0
        print(f"\n=== {label}  r*={case['exact_period']}  Q={Q_disp} ===")
        print(f"{'eb':>5} {'AND':>4} {'N_T':>5} {'D':>5} {'d':>3} {'E[tr]':>6} "
              f"{'V_shot':>11} {'V_factor':>12} {'V_ratio':>8}")
        for r in rows:
            print(f"{r['eb']:>5} {r['and_after']:>4} {r['N_T']:>5} "
                  f"{r['D_depth']:>5} {r['d']:>3} {r['E_trials']:>6.2f} "
                  f"{r['V_shot']:>11} {r['V_factor']:>12.1f} "
                  f"{r.get('V_factor_ratio', 1.0):>8.3f}")


def plot_savings(cases_out: list[tuple[dict, list[dict]]], out: Path) -> None:
    fig, axes = plt.subplots(1, 3, figsize=(14, 4.2))
    ax1, ax2, ax3 = axes
    for case, rows in cases_out:
        if not rows:
            continue
        label = f"(base={case['base']}, N={case['N']})"
        ebs = [r["eb"] for r in rows]
        ax1.plot(ebs, [r["d"] for r in rows], "o-", label=label)
        ax2.plot(ebs, [r["V_shot"] for r in rows], "o-", label=label)
        ax3.plot(ebs, [r.get("V_factor_ratio", 1.0) for r in rows], "o-", label=label)
    ax1.set_title("Required code distance d(eb)")
    ax1.set_xlabel("error bound eb"); ax1.set_ylabel("d (odd integer)")
    ax1.grid(True, alpha=0.3); ax1.legend(fontsize=8)

    ax2.set_title("Per-shot space-time volume  V_shot = N_T · d²")
    ax2.set_xlabel("error bound eb"); ax2.set_ylabel("V_shot")
    ax2.set_yscale("log")
    ax2.grid(True, alpha=0.3); ax2.legend(fontsize=8)

    ax3.set_title("Total factoring cost ratio  V_factor(eb) / V_factor(0)")
    ax3.set_xlabel("error bound eb"); ax3.set_ylabel("ratio (<1 = savings)")
    ax3.axhline(1.0, color="gray", lw=0.7, ls="--", alpha=0.6)
    ax3.grid(True, alpha=0.3); ax3.legend(fontsize=8)

    fig.suptitle("Narrow approximation: reduces N_T → reduces d → saves volume "
                 "even after paying E[trials] tax")
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    print(f"wrote {out}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "shor_e2e.json")
    ap.add_argument("--outdir", type=Path,
        default=PROJECT_ROOT / "results" / "shor_quant" / "plots")
    ap.add_argument("--p-phys", type=float, default=1e-3)
    ap.add_argument("--p-th", type=float, default=1e-2)
    ap.add_argument("--A", type=float, default=0.03)
    ap.add_argument("--eps-budget", type=float, default=0.1,
        help="target logical failure rate per quantum shot")
    ap.add_argument("--t-per-and", type=int, default=4,
        help="T-gates emitted per AND node (Toffoli ≈ 4 T with ancilla)")
    ap.add_argument("--parallelism", type=int, default=1,
        help="# T layers merged per depth step (1 = fully serial modexp)")
    ap.add_argument("--ancilla", type=int, default=4,
        help="extra logical ancilla on top of 2·log₂(N)")
    args = ap.parse_args()

    params = dict(p_phys=args.p_phys, p_th=args.p_th, A=args.A,
                  eps_budget=args.eps_budget, t_per_and=args.t_per_and,
                  parallelism=args.parallelism, ancilla=args.ancilla)

    cases = json.loads(args.input.read_text())
    cases_out = [(c, analyze_case(c, params)) for c in cases]
    cases_out = [(c, r) for c, r in cases_out if r]
    print_report(cases_out, params)

    args.outdir.mkdir(parents=True, exist_ok=True)
    plot_savings(cases_out, args.outdir / "ft_cost_model.png")


if __name__ == "__main__":
    main()
