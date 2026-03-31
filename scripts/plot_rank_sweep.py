#!/usr/bin/env python3
"""Multi-rank H4 sweep and comparison plot.

Generates THC tensors at various ranks (via truncation or SVD),
runs error-bound sweeps for each (rank, bits) combination, and produces
a comparison plot of naive quantization vs ILP approximation.

Usage:
    python scripts/plot_rank_sweep.py           # full run
    python scripts/plot_rank_sweep.py --plot-only  # plot existing results
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESULTS_DIR = PROJECT_ROOT / "results"
TENSOR_DIR = RESULTS_DIR / "thc_h4_ranks"
SOURCE_TENSOR = RESULTS_DIR / "pipeline_h4_e2e" / "thc_H4_rank56_bfgs.npz"
HAMILTONIAN = RESULTS_DIR / "pipeline_h4_e2e" / "hamiltonian_H4.npz"

RANKS = [56, 100, 200, 400]
BITS = [4, 6, 8, 10, 12, 14, 16]
NOCC = 2
EPS = 1e-4

sys.path.insert(0, str(PROJECT_ROOT / "pipeline"))
from decode import decode_per_register, read_tt_file


def generate_tensors() -> None:
    """Generate THC tensors for all ranks.

    rank == 56: use BFGS-optimized source tensor.
    rank < 56: truncate (keep top-K chi rows by L2 norm).
    rank > 56: pad with near-zero chi rows and zero zeta entries.
               This preserves the BFGS-optimized physics while giving
               the correct QROM sizing for higher ranks.
    """
    TENSOR_DIR.mkdir(parents=True, exist_ok=True)
    source = np.load(SOURCE_TENSOR, allow_pickle=True)
    source_rank = int(source["thc_rank"])

    for rank in RANKS:
        out_path = TENSOR_DIR / f"thc_H4_rank{rank}.npz"
        if out_path.exists():
            continue

        if rank == source_rank:
            shutil.copy2(SOURCE_TENSOR, out_path)
            print(f"  rank={rank}: copied source tensor")
        elif rank < source_rank:
            chi_full = source["thc_chi"]
            zeta_full = source["thc_zeta"]
            chi_norms = np.linalg.norm(chi_full, axis=1)
            idx = np.argsort(-chi_norms)[:rank]
            chi_trunc = chi_full[idx, :]
            zeta_trunc = zeta_full[np.ix_(idx, idx)]
            np.savez_compressed(
                out_path,
                thc_chi=chi_trunc, thc_zeta=zeta_trunc,
                h1e=source["h1e"],
                nuclear_repulsion=source["nuclear_repulsion"],
                nmo=source["nmo"], thc_rank=np.array(rank),
            )
            print(f"  rank={rank}: truncated, |zeta|_max="
                  f"{np.max(np.abs(zeta_trunc)):.2f}")
        else:
            chi_src = source["thc_chi"]
            zeta_src = source["thc_zeta"]
            nmo = int(source["nmo"])
            extra = rank - source_rank
            np.random.seed(rank)
            chi_pad = np.vstack([chi_src,
                                 np.random.randn(extra, nmo) * 1e-10])
            zeta_pad = np.zeros((rank, rank))
            zeta_pad[:source_rank, :source_rank] = zeta_src
            np.savez_compressed(
                out_path,
                thc_chi=chi_pad, thc_zeta=zeta_pad,
                h1e=source["h1e"],
                nuclear_repulsion=source["nuclear_repulsion"],
                nmo=source["nmo"], thc_rank=np.array(rank),
            )
            print(f"  rank={rank}: padded from rank={source_rank}")


def _tensor_path(rank: int) -> Path:
    if rank == 56:
        return SOURCE_TENSOR
    return TENSOR_DIR / f"thc_H4_rank{rank}.npz"


def _sweep_dir(rank: int, bits: int) -> Path:
    return RESULTS_DIR / f"sweep_h4_rank{rank}_{bits}bit"


def _load_thc_ham(path: Path) -> dict:
    data = dict(np.load(path, allow_pickle=True))
    return {
        "h1e": data["h1e"],
        "thc_chi": data["thc_chi"],
        "thc_zeta": data["thc_zeta"],
        "nuclear_repulsion": float(data["nuclear_repulsion"]),
        "nmo": int(data["nmo"]),
        "thc_rank": int(data["thc_rank"]),
        "nocc": NOCC,
    }


def compute_quantization_eb(rank: int, num_bits: int) -> int:
    """Compute equivalent error bound by comparing N-bit vs 16-bit TTs.

    Extracts truth tables at both N-bit and 16-bit precision, decodes
    per-register integer values, rescales the 16-bit reference to the
    N-bit range, and computes the mean absolute error per node.
    Returns the maximum per-node error as the error bound.
    """
    from qlut_pipeline import extract_qluts

    ref_bits = 16
    thc_ham = _load_thc_ham(_tensor_path(rank))

    cache_dir = TENSOR_DIR / f"eb_cache_rank{rank}"
    nbit_dir = cache_dir / f"tt_{num_bits}bit"
    ref_dir = cache_dir / f"tt_{ref_bits}bit"

    for bv, out_dir in [(num_bits, nbit_dir), (ref_bits, ref_dir)]:
        if not (out_dir / "extraction_metadata.json").exists():
            print(f"    Extracting {bv}-bit TTs for rank={rank}...")
            extract_qluts(thc_ham, bv, EPS, out_dir)

    with open(nbit_dir / "extraction_metadata.json") as f:
        meta_n = json.load(f)
    with open(ref_dir / "extraction_metadata.json") as f:
        meta_r = json.load(f)

    max_node_error = 0.0
    for node_n, node_r in zip(meta_n["nodes"], meta_r["nodes"]):
        patterns_n = read_tt_file(nbit_dir / node_n["filename"])
        patterns_r = read_tt_file(ref_dir / node_r["filename"])

        try:
            regs_n = decode_per_register(
                patterns_n, node_n["tt_bitsizes"], node_n["data_shapes"]
            )
            regs_r = decode_per_register(
                patterns_r, node_r["tt_bitsizes"], node_r["data_shapes"]
            )
        except (OverflowError, ValueError):
            continue

        node_error = 0.0
        for vals_n, vals_r, bw_n, bw_r in zip(
            regs_n, regs_r, node_n["tt_bitsizes"], node_r["tt_bitsizes"]
        ):
            if bw_n == bw_r:
                continue
            v_n = vals_n.ravel().astype(np.float64)
            v_r = vals_r.ravel().astype(np.float64)
            n = min(len(v_n), len(v_r))
            if np.any(np.isnan(v_n[:n])) or np.any(np.isnan(v_r[:n])):
                continue
            max_n = (1 << bw_n) - 1
            max_r = (1 << bw_r) - 1
            if max_r == 0 or max_n == 0:
                continue
            v_r_scaled = v_r[:n] * max_n / max_r
            node_error += float(np.mean(np.abs(v_n[:n] - v_r_scaled)))

        max_node_error = max(max_node_error, node_error)

    eb = max(1, min(10000, int(np.ceil(max_node_error))))
    print(f"    Equivalent EB: rank={rank}, {num_bits}-bit -> {eb}")
    return eb


def run_experiments() -> None:
    """Run sweep experiments for all (rank, bits) combinations.

    Skips configurations that already have a sweep_report.json.
    Computes the equivalent error bound for each new configuration
    and calls run_sweep.py as a subprocess.
    """
    run_sweep_script = PROJECT_ROOT / "pipeline" / "run_sweep.py"

    for rank in RANKS:
        for bits in BITS:
            out_dir = _sweep_dir(rank, bits)
            if (out_dir / "sweep_report.json").exists():
                print(f"  rank={rank}, {bits}-bit: exists, skipping")
                continue

            print(f"\n  Computing EB for rank={rank}, {bits}-bit...")
            eb = compute_quantization_eb(rank, bits)

            print(f"  Running sweep rank={rank}, {bits}-bit, EB={eb}...")
            cmd = [
                sys.executable, str(run_sweep_script),
                "--thc-tensor", str(_tensor_path(rank)),
                "--num-bits", str(bits),
                "--error-bounds", str(eb),
                "--nocc", str(NOCC),
                "--eps", str(EPS),
                "--output-dir", str(out_dir),
            ]
            subprocess.run(cmd, check=True)


def collect_results() -> dict:
    """Read all sweep_report.json files into a structured dict.

    Returns dict[rank][bits] with keys: quant_delta, approx_delta,
    qroamclean_exact_and, qroamclean_approx_and, qrom_exact_and,
    qrom_approx_and.
    """
    data: dict[int, dict] = {}
    for rank in RANKS:
        data[rank] = {}
        for bits in BITS:
            path = _sweep_dir(rank, bits) / "sweep_report.json"
            if not path.exists():
                print(f"  Missing: rank={rank}, {bits}-bit")
                continue

            with open(path) as f:
                report = json.load(f)

            qd = report["deltas"]["quantization"]

            qrc_ex = 0
            qr_ex = 0
            for node in report.get("exact_synthesis", {}).get("results", []):
                fn = node["filename"]
                if "QROAMClean" in fn:
                    qrc_ex += node["and_count"]
                elif "QROM" in fn:
                    qr_ex += node["and_count"]

            ad = float("nan")
            qrc_ap = qrc_ex
            qr_ap = qr_ex
            for sr in report.get("sweep_results", []):
                if sr.get("converged", False):
                    ad = sr.get("approx_delta", float("nan"))
                    for node in sr.get("approx_synth_results", []):
                        fn = node["filename"]
                        if "QROAMClean" in fn:
                            qrc_ap = node["and_count"]
                        elif "QROM" in fn:
                            qr_ap = node["and_count"]
                    break

            data[rank][bits] = {
                "quant_delta": qd,
                "approx_delta": ad,
                "qroamclean_exact_and": qrc_ex,
                "qroamclean_approx_and": qrc_ap,
                "qrom_exact_and": qr_ex,
                "qrom_approx_and": qr_ap,
            }

    return data


def _isnan(v: float) -> bool:
    return v != v


def plot_results(data: dict) -> None:
    """Create comparison plot across ranks.

    Row 1: Energy error vs quantization bits (log scale).
        - Solid: naive quantization error |quant_delta|
        - Dashed: total error after ILP |quant_delta + approx_delta|
        - Dotted horizontal: chemical accuracy (1.6 mHa)
    Row 2: AND gate count vs bits.
        - Blue solid/dashed: QROAMClean exact/ILP
        - Orange solid/dashed: QROM exact/ILP
    """
    ranks_with_data = [r for r in RANKS if data.get(r)]
    if not ranks_with_data:
        print("No data to plot.")
        return

    fig, axes = plt.subplots(
        2, len(ranks_with_data),
        figsize=(4 * len(ranks_with_data), 8),
        sharex=True, squeeze=False,
    )
    chem_acc = 1.6e-3

    for col, rank in enumerate(ranks_with_data):
        ax_e = axes[0, col]
        ax_a = axes[1, col]

        b_arr: list[int] = []
        qd_vals: list[float | None] = []
        td_vals: list[float | None] = []
        qrc_ex_arr: list[int] = []
        qrc_ap_arr: list[int] = []
        qr_ex_arr: list[int] = []
        qr_ap_arr: list[int] = []

        for bits in BITS:
            d = data.get(rank, {}).get(bits)
            if d is None:
                continue
            b_arr.append(bits)

            qd = d["quant_delta"]
            ad = d["approx_delta"]
            qd_vals.append(abs(qd) if not _isnan(qd) else None)
            if not _isnan(qd) and not _isnan(ad):
                td_vals.append(abs(qd + ad))
            else:
                td_vals.append(None)

            qrc_ex_arr.append(d["qroamclean_exact_and"])
            qrc_ap_arr.append(d["qroamclean_approx_and"])
            qr_ex_arr.append(d["qrom_exact_and"])
            qr_ap_arr.append(d["qrom_approx_and"])

        if not b_arr:
            continue

        pts_q = [(b, v) for b, v in zip(b_arr, qd_vals)
                  if v is not None and v > 0]
        if pts_q:
            bq, vq = zip(*pts_q)
            ax_e.plot(bq, vq, "o-", color="tab:blue", label="Naive quant.")

        pts_t = [(b, v) for b, v in zip(b_arr, td_vals)
                  if v is not None and v > 0]
        if pts_t:
            bt, vt = zip(*pts_t)
            ax_e.plot(bt, vt, "s--", color="tab:red", label="ILP approx.")

        ax_e.axhline(chem_acc, color="gray", ls=":", alpha=0.7,
                      label="Chem. acc.")
        ax_e.set_yscale("log")
        ax_e.set_title(f"rank={rank}")
        ax_e.set_xticks(BITS)
        if col == 0:
            ax_e.set_ylabel("|Energy error| (Ha)")
        ax_e.legend(fontsize=7)

        ax_a.plot(b_arr, qrc_ex_arr, "o-", color="tab:blue",
                  label="QROAMClean exact")
        ax_a.plot(b_arr, qrc_ap_arr, "s--", color="tab:blue", alpha=0.6,
                  label="QROAMClean ILP")
        ax_a.plot(b_arr, qr_ex_arr, "o-", color="tab:orange",
                  label="QROM exact")
        ax_a.plot(b_arr, qr_ap_arr, "s--", color="tab:orange", alpha=0.6,
                  label="QROM ILP")
        ax_a.set_xticks(BITS)
        ax_a.set_xlabel("Quantization bits")
        if col == 0:
            ax_a.set_ylabel("AND gate count")
        ax_a.legend(fontsize=7)

    fig.tight_layout()
    out_path = RESULTS_DIR / "plot_rank_comparison.png"
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Plot saved to {out_path}")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Multi-rank H4 sweep and comparison plot"
    )
    parser.add_argument(
        "--plot-only", action="store_true",
        help="Skip experiments, just plot existing results",
    )
    args = parser.parse_args()

    if not args.plot_only:
        print("Step 1: Generating THC tensors...")
        generate_tensors()

        print("\nStep 2-3: Running experiments...")
        run_experiments()

    print("\nCollecting results...")
    data = collect_results()

    print("\nStep 4: Plotting...")
    plot_results(data)


if __name__ == "__main__":
    main()
