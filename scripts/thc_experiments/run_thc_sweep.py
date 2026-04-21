"""Run the full QROM-narrow sweep on a THC benchmark.

Pipeline:
  1. Load amplitudes → build alias table → keep[], alias[]
  2. Synthesize keep QROM + alias QROM via qsp-compilation SelectSwap
  3. Convert Qiskit circuits → Verilog XAGs
  4. Sweep ε_A: run approx-xag narrow on each; measure fidelity
  5. Emit JSON with per-ε_A AND counts and infidelities

Usage:
  python scripts/thc_experiments/run_thc_sweep.py \\
      --amps /tmp/qspc/benchmarks/thc/THC_water.json \\
      --n-bits 12 --precision-bits 8 --tag h2o_n12 \\
      --ebs 0.0 0.05 0.1 0.2 0.3 0.5
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, "/tmp/qspc/src")

from qsp_qrom_to_verilog import run_converter  # noqa: E402
from eval_qrom_fidelity import (  # noqa: E402
    amplitudes_to_probs,
    bhattacharyya_fidelity,
    build_alias_table,
    prepared_probs,
    simulate_qrom_verilog,
)


def sample_amplitudes(amps_path: str, n_bits: int):
    d = json.loads(Path(amps_path).read_text())
    raw_n = int(d["n_bits"])
    N = 1 << n_bits
    if n_bits > raw_n:
        raise ValueError(f"requested n_bits={n_bits} > source {raw_n}")
    amps = {}
    acc = 0.0
    for k, v in d["amplitudes"].items():
        idx = int(k)
        if idx < N:
            amps[idx] = float(v)
            acc += float(v) ** 2
    if acc <= 0:
        raise ValueError(f"no amplitude in first {N} bins")
    norm = acc ** 0.5
    return {k: v / norm for k, v in amps.items()}, N


def run_approx_xag(binary: str, in_v: str, out_v: str, eb: float,
                    care_file: str = "") -> dict:
    cmd = [binary, "--input-verilog", in_v, "--error-bound", str(eb),
           "--method", "narrow", "--output-verilog", out_v]
    if care_file:
        cmd += ["--care-patterns", care_file]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return json.loads(out.stdout.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--amps", type=str, required=True)
    ap.add_argument("--n-bits", type=int, required=True)
    ap.add_argument("--precision-bits", type=int, default=8)
    ap.add_argument("--tag", type=str, required=True)
    ap.add_argument("--ebs", nargs="+", type=float,
                    default=[0.0, 0.05, 0.1, 0.2, 0.3, 0.5])
    ap.add_argument("--qspc", type=str, default="/tmp/qspc")
    ap.add_argument("--approx-xag", type=str,
                    default=str(ROOT / "build" / "approx-xag"))
    ap.add_argument("--outdir", type=str,
                    default=str(ROOT / "data" / "thc_fidelity"))
    ap.add_argument("--use-dont-care", action="store_true",
                    help="For the alias QROM, pass only rows with "
                         "keep[j] < 2^b - 1 as care patterns. Rows where "
                         "keep saturates are genuine alias-input DCs "
                         "(the comparator always says 'keep' so alias[j] "
                         "is never read).")
    args = ap.parse_args()

    outdir = Path(args.outdir) / args.tag
    outdir.mkdir(parents=True, exist_ok=True)

    amps, N = sample_amplitudes(args.amps, args.n_bits)
    print(f"[{args.tag}] n_bits={args.n_bits} N={N} M={len(amps)}")

    probs = amplitudes_to_probs(amps, args.n_bits)
    keep_exact, alias_exact = build_alias_table(probs, args.precision_bits)

    keep_care_file = ""
    alias_care_file = ""
    if args.use_dont_care:
        max_val = (1 << args.precision_bits) - 1
        alias_cares = [j for j in range(N) if keep_exact[j] < max_val]
        alias_care_file = str(outdir / "alias_care.txt")
        Path(alias_care_file).write_text(
            "\n".join(str(a) for a in alias_cares) + "\n")
        print(f"  alias-QROM care patterns: {len(alias_cares)} / {N} "
              f"({100*(N-len(alias_cares))/N:.1f}% DC)")

    p_prep_exact = prepared_probs(keep_exact, alias_exact, args.precision_bits)
    infid_quant = 1.0 - bhattacharyya_fidelity(probs, p_prep_exact)
    print(f"  quantization-only infidelity: {infid_quant:.4e}")

    keep_v = str(outdir / "keep.v")
    alias_v = str(outdir / "alias.v")
    t0 = time.time()
    keep_stats = run_converter(
        keep_exact, args.n_bits, args.precision_bits, args.qspc, keep_v)
    alias_stats = run_converter(
        alias_exact, args.n_bits, args.n_bits, args.qspc, alias_v)
    t_synth = time.time() - t0
    print(f"  keep XAG: AND={keep_stats['and_count']} "
          f"XOR={keep_stats['xor_count']}")
    print(f"  alias XAG: AND={alias_stats['and_count']} "
          f"XOR={alias_stats['xor_count']}")
    print(f"  synth/convert time: {t_synth:.1f}s")

    results = []
    for eb in args.ebs:
        keep_eb = str(outdir / f"keep_eb{eb}.v")
        alias_eb = str(outdir / f"alias_eb{eb}.v")
        t0 = time.time()
        k_res = run_approx_xag(args.approx_xag, keep_v, keep_eb, eb,
                                keep_care_file)
        a_res = run_approx_xag(args.approx_xag, alias_v, alias_eb, eb,
                                alias_care_file)
        t_narrow = time.time() - t0

        t0 = time.time()
        keep_vals = simulate_qrom_verilog(
            keep_eb, args.n_bits, args.precision_bits, N)
        alias_vals = simulate_qrom_verilog(
            alias_eb, args.n_bits, args.n_bits, N)
        p_prep = prepared_probs(keep_vals, alias_vals, args.precision_bits)
        F = bhattacharyya_fidelity(probs, p_prep)
        t_eval = time.time() - t0

        rec = {
            "eb": eb,
            "keep_and_before": k_res["and_before"],
            "keep_and_after": k_res["and_after"],
            "alias_and_before": a_res["and_before"],
            "alias_and_after": a_res["and_after"],
            "keep_lacs": k_res["lacs_applied"],
            "alias_lacs": a_res["lacs_applied"],
            "fidelity": F,
            "infidelity": 1.0 - F,
            "t_narrow_s": t_narrow,
            "t_eval_s": t_eval,
        }
        results.append(rec)
        print(f"  eb={eb}: keep {k_res['and_before']}→{k_res['and_after']} "
              f"alias {a_res['and_before']}→{a_res['and_after']} "
              f"infid={1-F:.3e} (narrow {t_narrow:.1f}s, eval {t_eval:.1f}s)")

    summary = {
        "tag": args.tag,
        "amps_source": args.amps,
        "n_bits": args.n_bits,
        "precision_bits": args.precision_bits,
        "N": N,
        "M": len(amps),
        "use_dont_care": bool(args.use_dont_care),
        "keep_and_baseline": keep_stats["and_count"],
        "alias_and_baseline": alias_stats["and_count"],
        "quantization_infidelity": infid_quant,
        "results": results,
    }
    (outdir / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"[{args.tag}] wrote {outdir / 'summary.json'}")


if __name__ == "__main__":
    main()
