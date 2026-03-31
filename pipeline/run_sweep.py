"""Configurable error-bound sweep with SS-DC synthesis evaluation.

Loads pre-computed THC tensors, extracts QLUTs, then sweeps error_bound
values measuring ILP approximation and don't-care-aware AND gate counts.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

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
    synthesize_qluts,
)


def _load_thc_ham(thc_path: Path, nocc: int) -> dict[str, Any]:
    data = dict(np.load(thc_path, allow_pickle=True))
    return {
        "h1e": data["h1e"],
        "thc_chi": data["thc_chi"],
        "thc_zeta": data["thc_zeta"],
        "nuclear_repulsion": float(data["nuclear_repulsion"]),
        "nmo": int(data["nmo"]),
        "thc_rank": int(data["thc_rank"]),
        "nocc": nocc,
    }


def _propagate_dont_cares(
    original_dir: Path, approx_dir: Path, output_dir: Path,
) -> None:
    """Restore X (don't-care) entries from original TTs into approximated TTs.

    approx-tt writes only 0/1, so don't-care positions from the original
    need to be restored for SS-DC synthesis on approximated truth tables.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    for orig_file in sorted(original_dir.glob("*.tt")):
        approx_file = approx_dir / orig_file.name
        if not approx_file.exists():
            continue
        orig_lines = orig_file.read_text().splitlines()
        approx_lines = approx_file.read_text().splitlines()
        result_lines = []
        for orig_line, approx_line in zip(orig_lines, approx_lines):
            chars = []
            for orig_ch, approx_ch in zip(orig_line, approx_line):
                if orig_ch in ("X", "x"):
                    chars.append("X")
                else:
                    chars.append(approx_ch)
            result_lines.append("".join(chars))
        out_path = output_dir / orig_file.name
        out_path.write_text("\n".join(result_lines) + "\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Configurable error-bound sweep with SS-DC synthesis"
    )
    parser.add_argument(
        "--thc-tensor", type=Path, required=True,
        help="Path to pre-computed THC .npz file",
    )
    parser.add_argument(
        "--num-bits", type=int, default=10,
        help="State preparation bits (default: 10)",
    )
    parser.add_argument(
        "--error-bounds", type=int, nargs="+", default=[1000, 5000],
        help="Error bounds to sweep (default: 1000 5000)",
    )
    parser.add_argument(
        "--time-limit", type=float, default=120.0,
        help="ILP solver time limit in seconds (default: 120)",
    )
    parser.add_argument(
        "--eps", type=float, default=1e-4,
        help="Eigenvalue cutoff (default: 1e-4)",
    )
    parser.add_argument(
        "--nocc", type=int, default=2,
        help="Number of occupied orbitals (default: 2)",
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("results/pipeline_sweep"),
        help="Output directory (default: results/pipeline_sweep)",
    )
    parser.add_argument(
        "--num-random-starts", type=int, default=1,
        help="SS synthesis random starts (default: 1)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    approx_binary = _project_root / "build" / "approx-tt"
    synth_binary = _project_root / "build" / "synth-tt"

    print("Loading THC tensors...")
    thc_ham = _load_thc_ham(args.thc_tensor, args.nocc)
    thc_rank = thc_ham["thc_rank"]
    print(f"  nmo={thc_ham['nmo']}, thc_rank={thc_rank}")

    tt_dir = output_dir / "truth_tables"
    cache_path = output_dir / "baseline_cache.json"

    if cache_path.exists():
        print("\nLoading cached baseline and quantization results...")
        with open(cache_path) as f:
            cache = json.load(f)
        baseline_energy = cache["baseline_energy"]
        exact_energy = cache["exact_energy"]
        quant_delta = cache["quant_delta"]
        exact_synth_result = cache.get("exact_synth_result")
        with open(tt_dir / "extraction_metadata.json") as f:
            metadata = json.load(f)
        extraction = {
            "metadata": metadata,
            "t_l_eigvecs": np.load(tt_dir / "extraction_extra.npz")["t_l_eigvecs"],
        }
    else:
        print(f"\nExtracting QLUTs with num_bits_state_prep={args.num_bits}...")
        extraction = extract_qluts(thc_ham, args.num_bits, args.eps, tt_dir)
        metadata = extraction["metadata"]

        print("\nEvaluating THC baseline (no quantization)...")
        baseline_ham = _thc_tensors_to_hamiltonian(
            h1e=np.asarray(thc_ham["h1e"]),
            thc_chi=np.asarray(thc_ham["thc_chi"]),
            thc_zeta=np.asarray(thc_ham["thc_zeta"]),
            nuclear_repulsion=thc_ham.get("nuclear_repulsion", 0.0),
        )
        baseline_ham["nocc"] = args.nocc
        try:
            baseline_energy = evaluate(baseline_ham)
        except Exception as exc:
            print(f"  Baseline CCSD(T) failed: {exc}")
            baseline_energy = {"ccsd_t_energy": float("nan")}

        print("\nDecoding exact (quantized, no approx) TTs...")
        exact_ham = decode_and_reconstruct(tt_dir, thc_ham, extraction)
        exact_ham["nocc"] = args.nocc
        try:
            exact_energy = evaluate(exact_ham)
        except Exception as exc:
            print(f"  Exact CCSD(T) failed: {exc}")
            exact_energy = {"ccsd_t_energy": float("nan")}
        bl = baseline_energy["ccsd_t_energy"]
        ex = exact_energy["ccsd_t_energy"]
        quant_delta = ex - bl if not (bl != bl or ex != ex) else float("nan")

        print("\nSynthesizing exact TTs with SS-DC...")
        exact_synth = synthesize_qluts(
            tt_dir, synth_binary,
            num_random_starts=args.num_random_starts,
        )
        exact_synth_result = exact_synth

        _save_json(cache_path, {
            "baseline_energy": baseline_energy,
            "exact_energy": exact_energy,
            "quant_delta": quant_delta,
            "exact_synth_result": exact_synth_result,
        })

    for node_meta in metadata["nodes"]:
        print(f"  {node_meta['filename']}: {node_meta['num_inputs']}in -> "
              f"{node_meta['num_outputs']}out, bitsizes={node_meta['tt_bitsizes']}")
    bl_e = baseline_energy["ccsd_t_energy"]
    ex_e = exact_energy["ccsd_t_energy"]
    bl_str = f"{bl_e:.10f}" if not (bl_e != bl_e) else "N/A"
    ex_str = f"{ex_e:.10f}" if not (ex_e != ex_e) else "N/A"
    qd_str = f"{quant_delta:.10f} Ha ({quant_delta:.4e})" if not (quant_delta != quant_delta) else "N/A"
    print(f"  THC baseline CCSD(T): {bl_str} Ha")
    print(f"  THC+quant CCSD(T):    {ex_str} Ha")
    print(f"  Quantization delta:   {qd_str}")

    exact_and_total = 0
    if exact_synth_result:
        exact_and_total = exact_synth_result.get("total_and_count", 0)
        print(f"  Exact TT AND count (SS-DC): {exact_and_total}")

    print(f"\nSweeping error bounds: {args.error_bounds}")
    sweep_results = []

    for eb in args.error_bounds:
        print(f"\n--- error_bound = {eb} ---")
        approx_subdir = tt_dir / f"approx_eb{eb}"
        approx_subdir.mkdir(parents=True, exist_ok=True)

        approx_result = approximate_qluts(
            tt_dir, eb, approx_binary, args.time_limit
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

        dc_dir = approx_subdir / "with_dc"
        _propagate_dont_cares(tt_dir, approx_subdir, dc_dir)

        approx_synth = synthesize_qluts(
            dc_dir, synth_binary,
            num_random_starts=args.num_random_starts,
        )
        approx_and_total = approx_synth.get("total_and_count", 0)
        print(f"  Approx TT AND count (SS-DC): {approx_and_total}")

        qrom_errors = compute_qrom_errors(tt_dir, approx_subdir, metadata)
        for node_err in qrom_errors:
            for reg in node_err["registers"]:
                if reg["num_changed"] > 0:
                    print(f"    {node_err['node']} reg{reg['register_index']} "
                          f"({reg['bitwidth']}b): {reg['num_changed']}/{reg['num_entries']} "
                          f"changed, max_err={reg['max_abs_error']}")

        approx_ham = decode_and_reconstruct(approx_subdir, thc_ham, extraction)
        approx_ham["nocc"] = args.nocc
        try:
            approx_energy = evaluate(approx_ham)
            bl = baseline_energy["ccsd_t_energy"]
            ex = exact_energy["ccsd_t_energy"]
            ae = approx_energy["ccsd_t_energy"]
            approx_delta = ae - ex if not (ae != ae or ex != ex) else float("nan")
            total_delta = ae - bl if not (ae != ae or bl != bl) else float("nan")
            print(f"  Approx CCSD(T):     {ae:.10f} Ha")
            if not (approx_delta != approx_delta):
                print(f"  Approx delta:       {approx_delta:.10f} Ha ({approx_delta:.4e})")
            converged = True
        except Exception as exc:
            print(f"  CCSD failed: {exc}")
            approx_energy = {"ccsd_t_energy": float("nan")}
            approx_delta = float("nan")
            total_delta = float("nan")
            converged = False

        entry = {
            "error_bound": eb,
            "bits_flipped": total_bits_flipped,
            "exact_and_count": exact_and_total,
            "approx_and_count": approx_and_total,
            "approx_stats": approx_result.get("results", []),
            "approx_synth_results": approx_synth.get("results", []),
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

    print("\n" + "=" * 90)
    print("SUMMARY")
    print("=" * 90)
    bl_e = baseline_energy["ccsd_t_energy"]
    ex_e = exact_energy["ccsd_t_energy"]
    print(f"THC baseline CCSD(T):  {bl_e:.10f} Ha" if not (bl_e != bl_e) else "THC baseline CCSD(T):  N/A")
    print(f"Quant CCSD(T):         {ex_e:.10f} Ha" if not (ex_e != ex_e) else "Quant CCSD(T):         N/A")
    qd = quant_delta
    print(f"Quantization delta:    {qd:.6e} Ha" if not (qd != qd) else "Quantization delta:    N/A")
    print()
    header = (f"{'EB':>6}  {'Bits Flipped':>12}  {'AND(exact-dc)':>13}  "
              f"{'AND(approx-dc)':>14}  {'Approx Delta':>14}  {'Conv':>4}")
    print(header)
    print("-" * len(header))
    for entry in sweep_results:
        conv = "Y" if entry.get("converged", True) else "N"
        ad = entry["approx_delta"]
        ad_str = f"{ad:>14.6e}" if not (ad != ad) else "           N/A"
        print(f"{entry['error_bound']:>6}  {entry['bits_flipped']:>12}  "
              f"{entry['exact_and_count']:>13}  {entry['approx_and_count']:>14}  "
              f"{ad_str}  {conv:>4}")

    report = {
        "parameters": {
            "thc_tensor": str(args.thc_tensor),
            "thc_rank": thc_rank,
            "num_bits_state_prep": args.num_bits,
            "eps": args.eps,
            "nocc": args.nocc,
            "error_bounds": args.error_bounds,
            "time_limit": args.time_limit,
            "num_random_starts": args.num_random_starts,
        },
        "energies": {
            "thc_baseline": baseline_energy,
            "thc_quant": exact_energy,
        },
        "deltas": {
            "quantization": quant_delta,
        },
        "exact_synthesis": exact_synth_result,
        "sweep_results": sweep_results,
    }
    report_path = output_dir / "sweep_report.json"
    _save_json(report_path, report)
    print(f"\nReport saved to {report_path}")


if __name__ == "__main__":
    main()
