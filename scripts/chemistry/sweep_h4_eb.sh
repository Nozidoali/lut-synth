#!/usr/bin/env bash
# Sweep error_bound for H4/sto-3g at a fixed num_bits_state_prep.
# Collects (eb, AND_before, AND_after, LACs, acc_err, baseline_E, approx_E, dE_mHa, converged).

NUM_BITS="${NUM_BITS:-10}"
BUDGETS="${BUDGETS:-0.0 0.01 0.05 0.1 0.3 0.5 1.0}"
OUT_ROOT="/home/hanyu/lut-synth/results/h4_sweep_nb${NUM_BITS}"
CSV="$OUT_ROOT/sweep.csv"

mkdir -p "$OUT_ROOT"
echo "eb,and_before,and_after,lacs,acc_err,baseline_Ha,approx_Ha,dE_mHa,converged" > "$CSV"

source /home/hanyu/anaconda3/etc/profile.d/conda.sh
conda activate quantum

for eb in $BUDGETS; do
  dir="$OUT_ROOT/eb_${eb}"
  printf '=== eb=%s ===\n' "$eb"

  python /home/hanyu/lut-synth/scripts/chemistry/qlut_pipeline.py \
    --system H4 --basis sto-3g --thc-rank 4 \
    --num-bits "$NUM_BITS" --error-bound "$eb" \
    --method narrow --output-dir "$dir" > "$dir.log" 2>&1
  rc=$?

  python - "$dir" "$eb" "$CSV" "$rc" <<'PY'
import json, os, sys
dir_, eb, csv, rc = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
cmp_p = os.path.join(dir_, "comparison.json")
and_b = and_a = lacs = ""
acc = base_e = appr_e = dE = ""
conv = "1" if rc == 0 else "0"
approx_stats = None
if os.path.exists(cmp_p):
    d = json.load(open(cmp_p))
    base_e = f"{d['baseline_energies']['ccsd_t_energy']:.8f}"
    appr_e = f"{d['approximate_energies']['ccsd_t_energy']:.8f}"
    dE = f"{(float(appr_e)-float(base_e))*1000:.4f}"
    approx_stats = d.get("approximation_stats", {}).get("results", [])
if approx_stats is None:
    stats_p = os.path.join(dir_, "truth_tables", "approx", "approx_stats.json")
    if os.path.exists(stats_p):
        approx_stats = json.load(open(stats_p))
if approx_stats:
    for s in approx_stats:
        if s.get("method") == "narrow":
            and_b = s.get("and_before", "")
            and_a = s.get("and_after", "")
            lacs = s.get("lacs_applied", "")
            acc = f"{s.get('accumulated_error', 0):.4f}"
            break
with open(csv, "a") as f:
    f.write(f"{eb},{and_b},{and_a},{lacs},{acc},{base_e},{appr_e},{dE},{conv}\n")
print(f"  AND {and_b}→{and_a}  LACs={lacs}  acc_err={acc}  ΔE={dE} mHa  conv={conv}")
PY
done

echo ""
echo "=== summary ==="
column -s, -t "$CSV"
