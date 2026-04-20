#!/usr/bin/env bash
# End-to-end chained windowed Shor's flow:
#   1. scripts/shor_chained.py: generate per-window LUTs, synth+approx, compose f̃
#   2. scripts/plot_shor.py: Pareto + per-case panels
#   3. scripts/plot_shor_period.py: period degradation for chosen case
#   4. scripts/model_ft_cost.py: FTQC resource model at given p_phys
#
# Outputs land under results/shor_chained/.

set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$ROOT/results/shor_chained"
JSON="$WORKDIR/shor_chained.json"

CASES="${CASES:-5,33,4,12 3,35,4,12}"
EBS="${EBS:-0.0 0.05 0.1 0.2 0.3 0.5 0.7 1.0 2.0}"
SHOTS="${SHOTS:-2000}"
PLOT_CASE="${PLOT_CASE:-3,35,12}"
P_PHYS="${P_PHYS:-3e-3}"
STARTS="${STARTS:-4}"

source /home/hanyu/anaconda3/etc/profile.d/conda.sh
conda activate quantum

echo "### step 1/4: chained windowed synth + approx + compose ###"
python "$ROOT/scripts/shor_chained.py" \
  --cases $CASES --eb $EBS --shots "$SHOTS" \
  --num-random-starts "$STARTS" \
  --workdir "$WORKDIR"

echo ""
echo "### step 2/4: Pareto + per-case plots ###"
python "$ROOT/scripts/plot_shor.py" \
  --input "$JSON" --outdir "$WORKDIR/plots"

echo ""
echo "### step 3/4: period degradation plot for $PLOT_CASE ###"
python "$ROOT/scripts/plot_shor_period.py" \
  --input "$JSON" --case "$PLOT_CASE" \
  --out "$WORKDIR/plots/period_degradation.png"

echo ""
echo "### step 4/4: FTQC cost model at p_phys=$P_PHYS ###"
python "$ROOT/scripts/model_ft_cost.py" \
  --input "$JSON" --outdir "$WORKDIR/plots" --p-phys "$P_PHYS"

echo ""
echo "=== full flow done ==="
echo "  data    : $JSON"
echo "  plots   : $WORKDIR/plots/"
