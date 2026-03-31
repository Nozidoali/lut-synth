"""Aggressive quantization (4-bit) sweep on H4 (cc-pVTZ, rank-56 THC).

Loads pre-computed BFGS-optimized THC tensors, extracts QLUTs with
num_bits_state_prep=4, then sweeps error_bound values to measure
ILP approximation effectiveness on the coarser truth tables.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

_project_root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_project_root / "pipeline"))

from qlut_pipeline import (
    _NumpyEncoder,
    _save_json,
    _thc_tensors_to_hamiltonian,
    approximate_qluts,
    compute_qrom_errors,
    decode_and_reconstruct,
    evaluate,
    extract_qluts,
)

THC_PATH = _project_root / "results" / "pipeline_h4_e2e" / "thc_H4_rank56_bfgs.npz"
HAM_PATH = _project_root / "results" / "pipeline_h4_e2e" / "hamiltonian_H4.npz"
OUTPUT_DIR = _project_root / "results" / "pipeline_h4_4bit"
APPROX_BINARY = _project_root / "build" / "approx-tt"

NUM_BITS_STATE_PREP = 4
EPS = 1e-4
NOCC = 2
ERROR_BOUNDS = [1000, 2000, 3000, 4000, 5000, 10000]
TIME_LIMIT = 120.0


def load_thc_ham() -> dict:
    data = dict(np.load(THC_PATH, allow_pickle=True))
    return {
        "h1e": data["h1e"],
        "thc_chi": data["thc_chi"],
        "thc_zeta": data["thc_zeta"],
        "nuclear_repulsion": float(data["nuclear_repulsion"]),
        "nmo": int(data["nmo"]),
        "thc_rank": int(data["thc_rank"]),
        "nocc": NOCC,
    }


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    print("Loading cached THC tensors...")
    thc_ham = load_thc_ham()
    print(f"  nmo={thc_ham['nmo']}, thc_rank={thc_ham['thc_rank']}")

    tt_dir = OUTPUT_DIR / "truth_tables"
    cache_path = OUTPUT_DIR / "baseline_cache.json"

    if cache_path.exists():
        print("\nLoading cached baseline and quantization results...")
        with open(cache_path) as f:
            cache = json.load(f)
        baseline_energy = cache["baseline_energy"]
        exact_energy = cache["exact_energy"]
        quant_delta = cache["quant_delta"]
        with open(tt_dir / "extraction_metadata.json") as f:
            metadata = json.load(f)
        extraction = {
            "metadata": metadata,
            "t_l_eigvecs": np.load(tt_dir / "extraction_extra.npz")["t_l_eigvecs"],
        }
    else:
        print(f"\nStage 1: Extracting QLUTs with num_bits_state_prep={NUM_BITS_STATE_PREP}...")
        extraction = extract_qluts(thc_ham, NUM_BITS_STATE_PREP, EPS, tt_dir)
        metadata = extraction["metadata"]

        print("\nStage 2: Evaluating THC baseline (no quantization)...")
        baseline_ham = _thc_tensors_to_hamiltonian(
            h1e=np.asarray(thc_ham["h1e"]),
            thc_chi=np.asarray(thc_ham["thc_chi"]),
            thc_zeta=np.asarray(thc_ham["thc_zeta"]),
            nuclear_repulsion=thc_ham.get("nuclear_repulsion", 0.0),
        )
        baseline_ham["nocc"] = NOCC
        baseline_energy = evaluate(baseline_ham)

        print("\nStage 3: Decoding exact (quantized, no approx) TTs...")
        exact_ham = decode_and_reconstruct(tt_dir, thc_ham, extraction)
        exact_ham["nocc"] = NOCC
        exact_energy = evaluate(exact_ham)
        quant_delta = exact_energy["ccsd_t_energy"] - baseline_energy["ccsd_t_energy"]

        _save_json(cache_path, {
            "baseline_energy": baseline_energy,
            "exact_energy": exact_energy,
            "quant_delta": quant_delta,
        })

    for node_meta in metadata["nodes"]:
        print(f"  {node_meta['filename']}: {node_meta['num_inputs']}in -> "
              f"{node_meta['num_outputs']}out, bitsizes={node_meta['tt_bitsizes']}")
    print(f"  THC baseline CCSD(T): {baseline_energy['ccsd_t_energy']:.10f} Ha")
    print(f"  THC+quant CCSD(T):    {exact_energy['ccsd_t_energy']:.10f} Ha")
    print(f"  Quantization delta:   {quant_delta:.10f} Ha ({quant_delta:.4e})")

    print(f"\nStage 4: Sweeping error bounds: {ERROR_BOUNDS}")
    sweep_results = []

    for eb in ERROR_BOUNDS:
        print(f"\n--- error_bound = {eb} ---")
        approx_subdir = tt_dir / f"approx_eb{eb}"
        approx_subdir.mkdir(parents=True, exist_ok=True)

        approx_result = approximate_qluts(
            tt_dir, eb, APPROX_BINARY, TIME_LIMIT
        )

        src_dir = Path(approx_result["approx_dir"])
        for f in src_dir.glob("*.tt"):
            dest = approx_subdir / f.name
            dest.write_text(f.read_text())
        for f in src_dir.glob("*.json"):
            dest = approx_subdir / f.name
            dest.write_text(f.read_text())

        total_bits_flipped = sum(
            r.get("bits_flipped", 0) for r in approx_result.get("results", [])
        )
        print(f"  Total bits flipped: {total_bits_flipped}")

        qrom_errors = compute_qrom_errors(tt_dir, approx_subdir, metadata)
        for node_err in qrom_errors:
            for reg in node_err["registers"]:
                if reg["num_changed"] > 0:
                    print(f"    {node_err['node']} reg{reg['register_index']} "
                          f"({reg['bitwidth']}b): {reg['num_changed']}/{reg['num_entries']} "
                          f"changed, max_err={reg['max_abs_error']}")

        approx_ham = decode_and_reconstruct(approx_subdir, thc_ham, extraction)
        approx_ham["nocc"] = NOCC
        try:
            approx_energy = evaluate(approx_ham)
            approx_delta = approx_energy["ccsd_t_energy"] - exact_energy["ccsd_t_energy"]
            total_delta = approx_energy["ccsd_t_energy"] - baseline_energy["ccsd_t_energy"]
            print(f"  Approx CCSD(T):     {approx_energy['ccsd_t_energy']:.10f} Ha")
            print(f"  Approx delta:       {approx_delta:.10f} Ha ({approx_delta:.4e})")
            print(f"  Total delta:        {total_delta:.10f} Ha ({total_delta:.4e})")
            converged = True
        except RuntimeError as exc:
            print(f"  CCSD failed: {exc}")
            approx_energy = {"ccsd_t_energy": float("nan")}
            approx_delta = float("nan")
            total_delta = float("nan")
            converged = False

        entry = {
            "error_bound": eb,
            "bits_flipped": total_bits_flipped,
            "approx_stats": approx_result.get("results", []),
            "baseline_ccsd_t": baseline_energy["ccsd_t_energy"],
            "quant_ccsd_t": exact_energy["ccsd_t_energy"],
            "approx_ccsd_t": approx_energy.get("ccsd_t_energy", float("nan")),
            "quant_delta": quant_delta,
            "approx_delta": approx_delta,
            "total_delta": total_delta,
            "qrom_errors": qrom_errors,
            "converged": converged,
        }
        sweep_results.append(entry)

    print("\n" + "=" * 70)
    print("SUMMARY: 4-bit quantization sweep on H4 (cc-pVTZ, rank-56 THC)")
    print("=" * 70)
    print(f"THC baseline CCSD(T):  {baseline_energy['ccsd_t_energy']:.10f} Ha")
    print(f"4-bit quant CCSD(T):   {exact_energy['ccsd_t_energy']:.10f} Ha")
    print(f"Quantization delta:    {quant_delta:.6e} Ha")
    print(f"10-bit quant delta was: 1.97e-04 Ha (from previous run)")
    print()
    print(f"{'EB':>6}  {'Bits Flipped':>12}  {'Approx Delta':>14}  {'Total Delta':>14}  {'Approx CCSD(T)':>16}  {'Conv':>4}")
    print("-" * 80)
    for entry in sweep_results:
        conv = "Y" if entry.get("converged", True) else "N"
        ad = entry['approx_delta']
        td = entry['total_delta']
        ae = entry['approx_ccsd_t']
        ad_str = f"{ad:>14.6e}" if not (ad != ad) else "           N/A"
        td_str = f"{td:>14.6e}" if not (td != td) else "           N/A"
        ae_str = f"{ae:>16.10f}" if not (ae != ae) else "             N/A"
        print(f"{entry['error_bound']:>6}  {entry['bits_flipped']:>12}  "
              f"{ad_str}  {td_str}  {ae_str}  {conv:>4}")

    report = {
        "system": {"name": "H4", "nmo": 56, "nocc": NOCC, "basis": "cc-pVTZ"},
        "parameters": {
            "thc_rank": 56,
            "num_bits_state_prep": NUM_BITS_STATE_PREP,
            "eps": EPS,
            "error_bounds": ERROR_BOUNDS,
            "time_limit": TIME_LIMIT,
        },
        "baseline_10bit": {
            "num_bits_state_prep": 10,
            "quant_delta": 1.97e-4,
            "approx_delta": 0.0,
            "note": "from previous run",
        },
        "energies": {
            "thc_baseline": baseline_energy,
            "thc_quant_4bit": exact_energy,
        },
        "deltas": {
            "quant_4bit": quant_delta,
        },
        "sweep_results": sweep_results,
    }
    _save_json(OUTPUT_DIR / "4bit_sweep_report.json", report)
    print(f"\nReport saved to {OUTPUT_DIR / '4bit_sweep_report.json'}")


if __name__ == "__main__":
    main()
