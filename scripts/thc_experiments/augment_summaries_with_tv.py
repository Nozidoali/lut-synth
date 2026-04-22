"""Augment summary.json files with per-eb TV distance.

Re-simulates the stored approximated keep/alias Verilog files (from
the sweep's outdir) to compute TV = (1/2) ||p_target - p_prep||_1
for each eb. TV matches the PREPARE-oracle coefficient-error metric
used in Babbush/Lee THC qubitization bounds: energy-error <= 2 * lambda * TV.

Usage:
  python scripts/thc_experiments/augment_summaries_with_tv.py \\
      --amps /tmp/qspc/benchmarks/thc/THC_water.json --tag h2o_n12_ls
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from eval_qrom_fidelity import (  # noqa: E402
    amplitudes_to_probs, build_alias_table, prepared_probs,
    simulate_qrom_verilog, total_variation,
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--amps", type=str, required=True)
    ap.add_argument("--tag", type=str, required=True)
    ap.add_argument("--outdir", type=str,
                    default=str(ROOT / "data" / "thc_fidelity"))
    args = ap.parse_args()

    outdir = Path(args.outdir) / args.tag
    summary_path = outdir / "summary.json"
    summary = json.loads(summary_path.read_text())
    n_bits = summary["n_bits"]
    precision_bits = summary["precision_bits"]
    N = summary["N"]

    src = json.loads(Path(args.amps).read_text())
    raw_n = int(src["n_bits"])
    amps = {int(k): float(v) for k, v in src["amplitudes"].items()
            if int(k) < (1 << n_bits)}
    norm = (sum(v * v for v in amps.values())) ** 0.5
    if norm <= 0:
        raise SystemExit("empty amplitude window")
    amps = {k: v / norm for k, v in amps.items()}
    probs = amplitudes_to_probs(amps, n_bits)

    lambda_total = src.get("lambda_total")
    lambda_zeta = src.get("lambda_zeta")
    summary["lambda_total"] = lambda_total
    summary["lambda_zeta"] = lambda_zeta
    summary["raw_n_bits"] = raw_n

    for rec in summary["results"]:
        eb = rec["eb"]
        keep_v = outdir / f"keep_eb{eb}.v"
        alias_v = outdir / f"alias_eb{eb}.v"
        if not keep_v.exists() or not alias_v.exists():
            print(f"[{args.tag}] eb={eb}: XAGs missing, skipping")
            continue
        keep_vals = simulate_qrom_verilog(
            str(keep_v), n_bits, precision_bits, N)
        alias_vals = simulate_qrom_verilog(
            str(alias_v), n_bits, n_bits, N)
        p_prep = prepared_probs(keep_vals, alias_vals, precision_bits)
        rec["tv"] = total_variation(probs, p_prep)
        rec["total_prep"] = sum(p_prep)
        print(f"[{args.tag}] eb={eb}: tv={rec['tv']:.3e} "
              f"(infid={rec['infidelity']:.3e})")

    summary_path.write_text(json.dumps(summary, indent=2))
    print(f"[{args.tag}] updated {summary_path}")


if __name__ == "__main__":
    main()
