# lut-synth ↔ approx_qlut_simulation Repo Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move H4 / Shor's / random-TT experiment code, artifacts, and H4-specific docs out of the lut-synth repo into the existing `third-party/approx_qlut_simulation/` submodule. lut-synth shrinks to a focused TT→XAG library + CLI tools (exact + approximate). Strip the H4-specific `--h4` mode from `tools/approx-tt.cpp` and `tools/approx-xag.cpp` and delete `src/lut-synth/h4-config.hpp`.

**Architecture:** Two-phase Option A. Phase 1: 8 semantic commits land in the `WanHsuanLin/approx_qlut_simulation` submodule repo on a feature branch, then merge to its `main`. Phase 2: a single lut-synth commit bumps the submodule pointer, deletes moved-out paths, strips `--h4` from the CLI tools, and rewrites `setup.sh` + `README.md`. Cross-repo invocation is done through `LUT_SYNTH_ROOT` (default `../..` relative to approx_qlut_simulation root). The `paper/qce2026` submodule is intentionally left untouched (the user has in-flight overleaf work there).

**Tech Stack:** Bash, git/git-submodule, C++17 (CMake + Catch2), Python (read-only — no Python tests in this plan).

**Reference spec:** `docs/superpowers/specs/2026-05-08-repo-split-design.md`

---

## Pre-conditions

Before starting:

- Push access to `WanHsuanLin/approx_qlut_simulation` (Phase 1 push step).
- A clean `main` in lut-synth with no uncommitted changes.
- Working C++ baseline build: `cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON && cmake --build build -j8 && ctest --test-dir build --output-on-failure` is green.

---

## Task 1: Pre-flight checks and baseline

**Files:**
- Read: `/home/hanyu/lut-synth/.git/config`, `/home/hanyu/lut-synth/third-party/approx_qlut_simulation/.git`

- [ ] **Step 1: Verify lut-synth working tree is in the expected pre-refactor state.**

```bash
cd /home/hanyu/lut-synth
git status --short
```

Expected (exactly these lines, in any order):

```
 M paper/qce2026
?? A_Dont-Care-Based_Approach_to_Reducing_the_Multiplicative_Complexity_in_Logic_Networks.pdf
```

The `M paper/qce2026` is in-flight overleaf work; the refactor will NOT touch that submodule. The PDF is to-be-deleted in Task 11. Both are accepted for this gate.

If `git status --short` shows ANY OTHER modified or untracked entry (especially under `src/`, `tools/`, `scripts/`, or `docs/superpowers/`), STOP — do not proceed.

- [ ] **Step 2: Capture baseline build is green.**

```bash
cd /home/hanyu/lut-synth
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

Expected: all tests pass. The build artifacts `build/approx-tt`, `build/approx-xag`, `build/synth-tt` exist. If anything fails, STOP and fix before proceeding.

- [ ] **Step 3: Verify push access to the submodule remote.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git remote -v
git ls-remote --heads origin | head
```

Expected: `origin` points at `git@github.com:WanHsuanLin/approx_qlut_simulation.git` and `git ls-remote` returns at least the `main` branch. If `git ls-remote` fails with permission denied, STOP — push access is required for this plan.

- [ ] **Step 4: Record baseline submodule SHA.**

```bash
cd /home/hanyu/lut-synth
git submodule status third-party/approx_qlut_simulation
```

Write down the SHA (e.g. `33c31d1...`). It is the rollback point for Phase 1.

- [ ] **Step 5: Record artifact sizes for Task 7 confirmation.**

```bash
cd /home/hanyu/lut-synth
du -sh data/ figures/ results/ 2>/dev/null
git ls-files data/ figures/ | wc -l
git ls-files results/ | wc -l
```

Expected (current snapshot): `data/` ~22M, `figures/` ~512K, `results/` ~239M; tracked file count: `data+figures` ~18, `results` 0. Confirms `results/` is gitignored; we will NOT commit it to approx_qlut_simulation.

---

## Task 2: Phase 1 commit 1 — import chemistry experiments

**Files:**
- Create in submodule: `chemistry/` (mirrors `lut-synth/scripts/chemistry/`)

- [ ] **Step 1: Check out a feature branch in the submodule on its `main`.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git fetch origin
git checkout main
git pull --ff-only origin main
git checkout -b refactor/import-experiments
```

Expected: branch `refactor/import-experiments` created off latest origin/main.

- [ ] **Step 2: Copy chemistry tree (excluding `__pycache__/`).**

```bash
cd /home/hanyu/lut-synth
mkdir -p third-party/approx_qlut_simulation/chemistry
rsync -a --exclude='__pycache__' --exclude='*.pyc' \
  scripts/chemistry/ third-party/approx_qlut_simulation/chemistry/
```

Expected: contents of `scripts/chemistry/` (qlut_pipeline.py, run_sweep.py, sweep_h4_eb.sh, eval_qrom_fidelity.py, plot_rank_sweep.py, run_4bit_sweep.py, src/, thc_experiments/, README.md) appear under the submodule.

- [ ] **Step 3: Verify file counts match.**

```bash
cd /home/hanyu/lut-synth
src_count=$(find scripts/chemistry -type f -not -path '*__pycache__*' -not -name '*.pyc' | wc -l)
dst_count=$(find third-party/approx_qlut_simulation/chemistry -type f | wc -l)
echo "src=$src_count dst=$dst_count"
test "$src_count" -eq "$dst_count" && echo OK || echo MISMATCH
```

Expected: `OK`.

- [ ] **Step 4: Commit in the submodule.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add chemistry/
git commit -m "Import chemistry experiments from lut-synth/scripts/chemistry"
```

Expected: one new commit on `refactor/import-experiments`. `git log --oneline -1` shows the import message.

---

## Task 3: Phase 1 commit 2 — import shors experiments

**Files:**
- Create in submodule: `shors/` (mirrors `lut-synth/scripts/shors/`)

- [ ] **Step 1: Copy shors tree (excluding `__pycache__/`).**

```bash
cd /home/hanyu/lut-synth
mkdir -p third-party/approx_qlut_simulation/shors
rsync -a --exclude='__pycache__' --exclude='*.pyc' \
  scripts/shors/ third-party/approx_qlut_simulation/shors/
```

Expected: contents of `scripts/shors/` (run.py, analyze.py, plot_shor.py, plot_shor_period.py, shor_qiskit.py, run_shor_chained_flow.sh, src/, figures/, README.md) appear under the submodule.

- [ ] **Step 2: Verify file count match.**

```bash
cd /home/hanyu/lut-synth
src_count=$(find scripts/shors -type f -not -path '*__pycache__*' -not -name '*.pyc' | wc -l)
dst_count=$(find third-party/approx_qlut_simulation/shors -type f | wc -l)
echo "src=$src_count dst=$dst_count"
test "$src_count" -eq "$dst_count" && echo OK || echo MISMATCH
```

Expected: `OK`.

- [ ] **Step 3: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add shors/
git commit -m "Import Shor's experiments from lut-synth/scripts/shors"
```

---

## Task 4: Phase 1 commit 3 — import randomTT benchmark

**Files:**
- Create in submodule: `randomTT/` (mirrors `lut-synth/scripts/randomTT/`)

- [ ] **Step 1: Copy randomTT tree.**

```bash
cd /home/hanyu/lut-synth
mkdir -p third-party/approx_qlut_simulation/randomTT
rsync -a --exclude='__pycache__' --exclude='*.pyc' \
  scripts/randomTT/ third-party/approx_qlut_simulation/randomTT/
```

Expected: contents of `scripts/randomTT/` (run_narrow_on_hlqcs.py, figures/, README.md) appear under the submodule.

- [ ] **Step 2: Verify file count match.**

```bash
cd /home/hanyu/lut-synth
src_count=$(find scripts/randomTT -type f -not -path '*__pycache__*' -not -name '*.pyc' | wc -l)
dst_count=$(find third-party/approx_qlut_simulation/randomTT -type f | wc -l)
echo "src=$src_count dst=$dst_count"
test "$src_count" -eq "$dst_count" && echo OK || echo MISMATCH
```

Expected: `OK`.

- [ ] **Step 3: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add randomTT/
git commit -m "Import randomTT benchmark from lut-synth/scripts/randomTT"
```

---

## Task 5: Phase 1 commit 4 — import shared helpers as `common/`

**Files:**
- Create in submodule: `common/decode.py`, `common/qsp_qrom_to_verilog.py`, `common/plot_paper.py`, `common/__init__.py`

`scripts/src/` contains `decode.py` and `qsp_qrom_to_verilog.py`. Plus root `scripts/qsp_qrom_to_verilog.py` (CLI wrapper) and `scripts/plot_paper.py`. They flatten into a single `common/` package on the destination side, so callers do `from common.decode import ...` (or, with `sys.path` insertion of `common/`, `from decode import ...` to keep current import style).

The Python files in `scripts/chemistry/` already do `from decode import ...` after inserting `scripts/src/` into `sys.path`. To keep diffs minimal in commit 8, we will preserve the bare `from decode import` style — meaning callers will insert `<aqs_root>/common/` into `sys.path`.

- [ ] **Step 1: Copy helpers, flattening the `src/` layer.**

```bash
cd /home/hanyu/lut-synth
mkdir -p third-party/approx_qlut_simulation/common
cp scripts/src/decode.py              third-party/approx_qlut_simulation/common/
cp scripts/src/qsp_qrom_to_verilog.py third-party/approx_qlut_simulation/common/qsp_qrom_to_verilog_lib.py
cp scripts/qsp_qrom_to_verilog.py     third-party/approx_qlut_simulation/common/qsp_qrom_to_verilog_cli.py
cp scripts/plot_paper.py              third-party/approx_qlut_simulation/common/plot_paper.py
touch third-party/approx_qlut_simulation/common/__init__.py
```

Note: the CLI wrapper `scripts/qsp_qrom_to_verilog.py` and the library `scripts/src/qsp_qrom_to_verilog.py` share a name. Renaming the CLI to `qsp_qrom_to_verilog_cli.py` and the lib to `qsp_qrom_to_verilog_lib.py` removes the collision; we will fix the CLI's `from qsp_qrom_to_verilog import run_converter` import in the path-fix commit (Task 9).

- [ ] **Step 2: Verify all four files copied.**

```bash
ls -1 third-party/approx_qlut_simulation/common/
```

Expected output (in some order):
```
__init__.py
decode.py
plot_paper.py
qsp_qrom_to_verilog_cli.py
qsp_qrom_to_verilog_lib.py
```

- [ ] **Step 3: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add common/
git commit -m "Import shared helpers as common/ package"
```

---

## Task 6: Phase 1 commit 5 — import tracked artifacts (data/, figures/) and update .gitignore

**Files:**
- Create in submodule: `data/`, `figures/`
- Modify in submodule: `.gitignore`

- [ ] **Step 1: Copy only git-tracked files under `data/` and `figures/` (preserves `.gitignore` boundaries).**

```bash
cd /home/hanyu/lut-synth
git ls-files data/ figures/ | while read f; do
  mkdir -p "third-party/approx_qlut_simulation/$(dirname "$f")"
  cp "$f" "third-party/approx_qlut_simulation/$f"
done
```

Expected: `data/thc_fidelity/.gitignore`, 11 `data/thc_fidelity/*/summary.json`, and 6 `figures/thc_pareto*` files copied (~18 files total).

- [ ] **Step 2: Update submodule `.gitignore` to exclude `results/` and Python build crud (idempotent).**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
cat > .gitignore <<'EOF'
__pycache__/
*.pyc
*.npz
.venv/
uv.lock
build/
results/
EOF
```

- [ ] **Step 3: Verify what would be added.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git status --short data/ figures/ .gitignore
git diff --stat --staged 2>/dev/null || true
```

Expected: ~18 new files under `data/` + `figures/`; modified `.gitignore`. No `results/` files staged.

- [ ] **Step 4: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add .gitignore data/ figures/
git commit -m "Import tracked experiment artifacts (data/, figures/) and exclude results/"
```

---

## Task 7: Phase 1 commit 6 — import H4 docs

**Files:**
- Create in submodule: `docs/h4-qrom-structure.md`

- [ ] **Step 1: Copy doc.**

```bash
cd /home/hanyu/lut-synth
mkdir -p third-party/approx_qlut_simulation/docs
cp docs/h4-qrom-structure.md third-party/approx_qlut_simulation/docs/
```

- [ ] **Step 2: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add docs/h4-qrom-structure.md
git commit -m "Import H4 QROM structure docs"
```

---

## Task 8: Phase 1 commit 7 — add setup.sh and update README

**Files:**
- Create in submodule: `setup.sh`
- Modify in submodule: `README.md`

- [ ] **Step 1: Author `setup.sh` in approx_qlut_simulation. This is the conda-env half of the old lut-synth `setup.sh`.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
cat > setup.sh <<'EOF'
#!/usr/bin/env bash
#
# Sets up the conda environment for the approx_qlut_simulation experiments
# (chemistry pipeline, Shor's flow, randomTT benchmark, CCSD(T) baseline).
#
# Assumes lut-synth is the parent (repo lives at
# lut-synth/third-party/approx_qlut_simulation) and that lut-synth has
# already been built (`./setup.sh` in lut-synth, or `cmake --build build`).
# Override LUT_SYNTH_ROOT if your layout differs.
#
# Flags:
#   --env NAME    conda env name (default: lut-synth)
#
# Requirements: conda. Optional: an existing lut-synth build at
# $LUT_SYNTH_ROOT/build/.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

ENV_NAME="lut-synth"
LUT_SYNTH_ROOT="${LUT_SYNTH_ROOT:-$ROOT/../..}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --env) ENV_NAME="$2"; shift 2 ;;
    -h|--help) sed -n '3,18p' "$0"; exit 0 ;;
    *) echo "Unknown flag: $1" >&2; exit 1 ;;
  esac
done

log() { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }

if ! command -v conda >/dev/null 2>&1; then
  echo "conda not found; install conda first." >&2
  exit 1
fi

source "$(conda info --base)/etc/profile.d/conda.sh"

if conda env list | awk '{print $1}' | grep -qx "$ENV_NAME"; then
  log "Reusing existing conda env '$ENV_NAME'"
else
  log "Creating conda env '$ENV_NAME' (python=3.11)"
  conda create -y -n "$ENV_NAME" python=3.11
fi

conda activate "$ENV_NAME"

log "Installing conda-forge packages (pyscf, numpy<2.3, scipy, attrs)"
conda install -y -c conda-forge \
  "pyscf>=2.12" \
  "numpy<2.3" \
  "scipy" \
  "attrs"

log "Installing pip packages (qualtran, cirq-core, numba<0.62)"
python -m pip install --quiet --upgrade pip
python -m pip install --quiet "qualtran" "cirq-core>=1.4" "numba<0.62"

log "Sanity check: importing pipeline deps"
python - <<'PY'
import numpy, pyscf, qualtran, cirq
from qualtran.bloqs.chemistry.thc import PrepareTHC
print(f"  numpy    {numpy.__version__}")
print(f"  pyscf    {pyscf.__version__}")
print(f"  cirq     {cirq.__version__}")
print("  qualtran OK, PrepareTHC importable")
PY

log "Python env '$ENV_NAME' ready. Activate with: conda activate $ENV_NAME"
log "LUT_SYNTH_ROOT is set to: $LUT_SYNTH_ROOT"
if [[ ! -x "$LUT_SYNTH_ROOT/build/approx-tt" ]]; then
  log "WARNING: $LUT_SYNTH_ROOT/build/approx-tt not found."
  log "Build lut-synth first: (cd \$LUT_SYNTH_ROOT && ./setup.sh)"
fi
EOF
chmod +x setup.sh
```

- [ ] **Step 2: Replace README with the extended version covering structure, dependency on lut-synth, LUT_SYNTH_ROOT, and per-subdir entry points. The existing README content (CCSD(T) baseline section) is preserved at the bottom.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
mv README.md README.baseline.md
cat > README.md <<'EOF'
# approx_qlut_simulation

Experiment harness for approximate-LUT synthesis: chemistry pipeline
(PySCF → THC → qualtran `PrepareTHC` → `.tt` → approximation → CCSD(T)),
Shor's algorithm flow (modexp LUTs → narrow approx → period decode →
factoring success), random-TT benchmarks against HLQCS, and the
CCSD(T) baseline tooling.

## Layout

```
approx_qlut_simulation/
├── setup.sh              # conda env for the Python deps
├── pyproject.toml        # CCSD(T) baseline (uv)
├── main.py               # CCSD(T) baseline entry point
├── ccsd_t.py
├── convert_thc.py
├── chemistry/            # H4 / QROM / sweeps / fidelity
├── shors/                # modexp / windowed Shor's / period analysis
├── randomTT/             # HLQCS-comparison benchmark
├── common/               # shared helpers (decode, qsp_qrom_to_verilog, plot_paper)
├── data/                 # input fixtures (thc_fidelity summaries)
├── figures/              # paper figures (thc_pareto*)
└── docs/h4-qrom-structure.md
```

## Dependency on lut-synth

This repo expects to live at `lut-synth/third-party/approx_qlut_simulation/`
and call the lut-synth C++ CLI tools (`approx-tt`, `synth-tt`,
`approx-xag`) via the `LUT_SYNTH_ROOT` environment variable.

Default `LUT_SYNTH_ROOT=../..` (resolves correctly when scripts are
invoked from this repo's root). Override to an absolute path if you
have a different layout:

```bash
export LUT_SYNTH_ROOT=/path/to/lut-synth
```

Build lut-synth first:

```bash
(cd "$LUT_SYNTH_ROOT" && ./setup.sh)        # builds C++ + tests
```

## Setup (Python env)

```bash
./setup.sh                # creates conda env "lut-synth"
conda activate lut-synth
```

## Quick examples

Chemistry pipeline (H4, sto-3g, fast smoke test):

```bash
python chemistry/qlut_pipeline.py \
  --system H4 --basis sto-3g --thc-rank 4 \
  --num-bits 6 --error-bound 1.0 \
  --method narrow --output-dir results/h4_smoke
```

Shor's chained windowed flow:

```bash
bash shors/run_shor_chained_flow.sh
```

CCSD(T) baseline (see `README.baseline.md` for full options):

```bash
uv run main.py --system H4
```

Random-TT vs HLQCS:

```bash
python randomTT/run_narrow_on_hlqcs.py
```

## Output convention

By default, scripts write to `./results/<experiment>/`. The `results/`
directory is gitignored — commit only artifacts you intend to share.

## See also

- `docs/h4-qrom-structure.md` — H4 QROM 5-register layout and the
  `keep` precision register, which is what gets approximated.
- Upstream lut-synth (`$LUT_SYNTH_ROOT`) — TT→XAG library, the
  `approx-tt` (Gurobi ILP) and `approx-xag` (narrow / ResubALS)
  approximators, and the `synth-tt` resynthesizer.
EOF
```

- [ ] **Step 3: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add setup.sh README.md README.baseline.md
git commit -m "Add setup.sh and rewrite README for the experiment harness"
```

---

## Task 9: Phase 1 commit 8 — fix paths to use LUT_SYNTH_ROOT

**Files modified in submodule:**
- `chemistry/sweep_h4_eb.sh`
- `chemistry/run_sweep.py`
- `chemistry/run_4bit_sweep.py`
- `chemistry/plot_rank_sweep.py`
- `chemistry/src/qlut_pipeline.py`
- `chemistry/thc_experiments/run_thc_sweep.py`
- `shors/run_shor_chained_flow.sh`
- `shors/run.py`
- `shors/src/synth_baseline.py`
- `shors/src/shor_chained.py`
- `shors/src/shor_e2e.py`
- `randomTT/run_narrow_on_hlqcs.py`
- `common/qsp_qrom_to_verilog_cli.py`

The pattern across the Python files: an existing `_project_root = Path(__file__).resolve().parents[N]` (or `PROJECT_ROOT = ...` / `ROOT = ...`) historically resolved to lut-synth root. After the move, the same call resolves one level deeper than approx_qlut_simulation, so each file gets a `parents[N+1]` and an `LUT_SYNTH_ROOT` env-var override. References to `/scripts/src` flatten to `common/`; references to `/scripts/chemistry/src` and `/scripts/shors/src` flatten to local `src/`.

- [ ] **Step 1: Inventory remaining hardcoded paths inside the submodule tree.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
grep -rEn '/home/[A-Za-z]+/'                  chemistry/ shors/ randomTT/ common/ | grep -v __pycache__
grep -rEn 'PROJECT_ROOT|_project_root|^ROOT'  chemistry/ shors/ randomTT/ common/ | grep -v __pycache__
grep -rEn 'scripts/(chemistry|shors|randomTT|src)/' chemistry/ shors/ randomTT/ common/ | grep -v __pycache__
```

Save the output. Every match below must be fixed; the final scan in Step 13 should return empty.

- [ ] **Step 2: Fix `chemistry/sweep_h4_eb.sh` (3 absolute paths).**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation/chemistry
sed -i \
  -e 's|OUT_ROOT="/home/hanyu/lut-synth/results/h4_sweep_nb${NUM_BITS}"|OUT_ROOT="${OUT_ROOT:-results/h4_sweep_nb${NUM_BITS}}"|' \
  -e 's|source /home/hanyu/anaconda3/etc/profile.d/conda.sh|source "$(conda info --base)/etc/profile.d/conda.sh"|' \
  -e 's|python /home/hanyu/lut-synth/scripts/chemistry/qlut_pipeline.py|python chemistry/qlut_pipeline.py|' \
  sweep_h4_eb.sh
grep -nE '/home/|scripts/' sweep_h4_eb.sh || echo "clean"
```

Expected final line: `clean`.

- [ ] **Step 3: Fix `shors/run_shor_chained_flow.sh` (ROOT pivot + conda + scripts/ paths).**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation/shors
python3 - <<'PY'
from pathlib import Path
p = Path("run_shor_chained_flow.sh")
text = p.read_text()
text = text.replace(
    'ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"',
    'AQS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"'
)
text = text.replace('$ROOT/results/shor_chained', '$AQS_ROOT/results/shor_chained')
text = text.replace('$ROOT/scripts/shors/', '$AQS_ROOT/shors/')
text = text.replace(
    'source /home/hanyu/anaconda3/etc/profile.d/conda.sh',
    'source "$(conda info --base)/etc/profile.d/conda.sh"'
)
p.write_text(text)
PY
grep -nE '/home/|scripts/shors|\$ROOT\b' run_shor_chained_flow.sh || echo "clean"
```

Expected: `clean`.

- [ ] **Step 4: Apply the standard `_project_root` rewrite to each Python file.**

Run this single migration script — it edits every Python file in one pass with the correct per-file depths and path flattening. Read it before running so you understand each substitution.

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
python3 - <<'PY'
"""Phase 1 commit 8 — Python path migration.

Strategy:
- Replace the existing `_project_root = Path(__file__).resolve().parents[N]`
  (or PROJECT_ROOT / ROOT) with a depth-(N+1) default plus LUT_SYNTH_ROOT
  override.
- Flatten `_project_root / "scripts" / "src"` -> `_aqs_root / "common"`
  (the new shared-helpers location).
- Flatten `_project_root / "scripts" / "chemistry" / "src"` -> file-local
  `src/` (since chemistry/src/ stays adjacent to chemistry/*.py after move).
- Flatten `_project_root / "scripts" / "shors" / "src"` -> file-local
  `src/` for shors/.
- Flatten `_project_root / "third-party" / "approx_qlut_simulation"`
  -> `_aqs_root` (the script's own repo root).
- `_project_root / "third-party" / "qlut-benchmarks" / "src"` is preserved
  (still lives in lut-synth).
- Output paths under `_project_root / "results"` move to
  `_aqs_root / "results"`.
- Build paths under `_project_root / "build"` stay (lut-synth/build).
"""
from pathlib import Path
import re

# (relative_path, original parents[N], new depth, aqs_depth)
TARGETS = [
    # chemistry/<file>.py: new parents[3]=lut-synth, parents[1]=aqs_root
    ("chemistry/run_sweep.py",                   2, 3, 1),
    ("chemistry/run_4bit_sweep.py",              2, 3, 1),
    ("chemistry/plot_rank_sweep.py",             2, 3, 1),
    # chemistry/src/<file>.py and chemistry/thc_experiments/<file>.py:
    # new parents[4]=lut-synth, parents[2]=aqs_root
    ("chemistry/src/qlut_pipeline.py",           3, 4, 2),
    ("chemistry/thc_experiments/run_thc_sweep.py", 3, 4, 2),
    # shors/<file>.py: parents[3]=lut-synth, parents[1]=aqs_root
    ("shors/run.py",                             2, 3, 1),
    # shors/src/<file>.py: parents[4]=lut-synth, parents[2]=aqs_root
    ("shors/src/synth_baseline.py",              3, 4, 2),
    ("shors/src/shor_chained.py",                3, 4, 2),
    ("shors/src/shor_e2e.py",                    3, 4, 2),
    # randomTT/<file>.py: parents[3]=lut-synth, parents[1]=aqs_root
    ("randomTT/run_narrow_on_hlqcs.py",          2, 3, 1),
]

PROJECT_ROOT_NAMES = ("_project_root", "PROJECT_ROOT", "ROOT")

def ensure_import_os(text: str) -> str:
    if re.search(r'^\s*import os\b', text, flags=re.M):
        return text
    # Insert `import os` after the first import line (or at top).
    m = re.search(r'^(import|from) .*$', text, flags=re.M)
    if not m:
        return "import os\n" + text
    end = m.end()
    return text[:end] + "\nimport os" + text[end:]

def rewrite(rel: str, old_depth: int, new_depth: int, aqs_depth: int) -> None:
    p = Path(rel)
    text = p.read_text()
    original = text

    # 1) project_root assignment: replace any of the three name forms
    for name in PROJECT_ROOT_NAMES:
        pattern = rf'^({name})\s*=\s*Path\(__file__\)\.resolve\(\)\.parents\[{old_depth}\]\s*$'
        repl = (
            f'\\1 = Path(os.environ.get(\n'
            f'    "LUT_SYNTH_ROOT",\n'
            f'    str(Path(__file__).resolve().parents[{new_depth}])\n'
            f'))\n'
            f'_aqs_root = Path(__file__).resolve().parents[{aqs_depth}]'
        )
        new_text, n = re.subn(pattern, repl, text, flags=re.M)
        if n:
            text = new_text
            break
    else:
        raise RuntimeError(f"{rel}: no project_root assignment matched")

    # 2) Path flattening — apply against any of the three name prefixes
    for name in PROJECT_ROOT_NAMES:
        text = text.replace(
            f'{name} / "scripts" / "src"',
            '_aqs_root / "common"',
        )
        text = text.replace(
            f'{name} / "scripts" / "chemistry" / "src"',
            'Path(__file__).resolve().parent / "src"',
        )
        text = text.replace(
            f'{name} / "scripts" / "shors" / "src"',
            'Path(__file__).resolve().parent / "src"',
        )
        text = text.replace(
            f'{name} / "third-party" / "approx_qlut_simulation"',
            '_aqs_root',
        )
        text = text.replace(
            f'{name} / "results"',
            '_aqs_root / "results"',
        )

    # 3) Make sure `import os` is present.
    text = ensure_import_os(text)

    if text != original:
        p.write_text(text)
        print(f"rewrote {rel}")
    else:
        print(f"unchanged {rel} (manual fix may still be needed)")

for rel, old, new, aqs in TARGETS:
    rewrite(rel, old, new, aqs)
PY
```

Expected: a `rewrote <file>` line for every TARGETS entry. If any reports `unchanged`, open that file and apply the rewrite manually.

- [ ] **Step 5: Verify each rewritten file's `_project_root` default points to lut-synth.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
python3 - <<'PY'
from pathlib import Path
TARGETS = [
    ("chemistry/run_sweep.py",                   3),
    ("chemistry/run_4bit_sweep.py",              3),
    ("chemistry/plot_rank_sweep.py",             3),
    ("chemistry/src/qlut_pipeline.py",           4),
    ("chemistry/thc_experiments/run_thc_sweep.py", 4),
    ("shors/run.py",                             3),
    ("shors/src/synth_baseline.py",              4),
    ("shors/src/shor_chained.py",                4),
    ("shors/src/shor_e2e.py",                    4),
    ("randomTT/run_narrow_on_hlqcs.py",          3),
]
for rel, depth in TARGETS:
    here = Path(rel).resolve()
    root = here.parents[depth]
    ok = root.name == "lut-synth"
    print(f"{'OK ' if ok else 'BAD'} {rel} parents[{depth}] = {root}")
PY
```

Expected: every line starts with `OK `. Any `BAD` means the depth in TARGETS is wrong for that file — fix and re-run Step 4.

- [ ] **Step 6: Fix the renamed import in `common/qsp_qrom_to_verilog_cli.py`.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation/common
sed -i 's|from qsp_qrom_to_verilog import run_converter|from qsp_qrom_to_verilog_lib import run_converter|' qsp_qrom_to_verilog_cli.py
grep -n run_converter qsp_qrom_to_verilog_cli.py
```

Expected: shows `from qsp_qrom_to_verilog_lib import run_converter`.

- [ ] **Step 7: Spot-check one rewritten file.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
sed -n '1,25p' chemistry/src/qlut_pipeline.py
```

Expected: shows `import os`, the new `_project_root = Path(os.environ.get(...))` block, and the `_aqs_root = Path(__file__).resolve().parents[2]` line. The old `parents[3]` literal is gone.

- [ ] **Step 8: Sanity-import `decode` from common/.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
python -c "import sys; sys.path.insert(0, 'common'); import decode; print('OK', decode.__file__)"
```

Expected: `OK /home/hanyu/lut-synth/third-party/approx_qlut_simulation/common/decode.py`. (If conda env not yet active, may need `conda activate lut-synth` first; pure-stdlib `decode.py` should not require it.)

- [ ] **Step 9: Final hardcoded-path scan.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
grep -rEn '/home/[A-Za-z]+/|scripts/(chemistry|shors|randomTT|src)/' \
  chemistry/ shors/ randomTT/ common/ 2>/dev/null | grep -v __pycache__
```

Expected: empty output.

- [ ] **Step 10: Spot-check the original `_project_root` literal depths are gone.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
grep -rEn 'parents\[2\]|parents\[3\]' chemistry/ shors/ randomTT/ common/ 2>/dev/null | grep -v __pycache__
```

Expected: only `parents[3]` or `parents[4]` references inside the new `os.environ.get` block (the depth is now `[N+1]`). No bare top-level `_project_root = ... parents[2]` left.

- [ ] **Step 11: Commit.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git add -A chemistry/ shors/ randomTT/ common/
git diff --staged --stat | tail
git commit -m "Rewire paths: LUT_SYNTH_ROOT env-var override and flatten common/"
```

---

## Task 10: Push and merge approx_qlut_simulation PR

**Files:** none (remote operation)

- [ ] **Step 1: Verify the 8 commits are present locally.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git log --oneline origin/main..HEAD
```

Expected: 8 commits, in order:
1. Import chemistry experiments from lut-synth/scripts/chemistry
2. Import Shor's experiments from lut-synth/scripts/shors
3. Import randomTT benchmark from lut-synth/scripts/randomTT
4. Import shared helpers as common/ package
5. Import tracked experiment artifacts (data/, figures/) and exclude results/
6. Import H4 QROM structure docs
7. Add setup.sh and rewrite README for the experiment harness
8. Rewire paths: LUT_SYNTH_ROOT env-var override and relative paths

- [ ] **Step 2: Push branch.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git push -u origin refactor/import-experiments
```

Expected: push succeeds; remote branch created.

- [ ] **Step 3: Open and merge PR on GitHub.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
gh pr create \
  --title "Import experiments from lut-synth" \
  --body "Imports chemistry, shors, randomTT, common helpers, tracked artifacts, H4 docs, setup.sh, and rewires paths to use LUT_SYNTH_ROOT. Companion to a single lut-synth commit that bumps this submodule pointer and removes the now-duplicate paths."
gh pr merge --merge --delete-branch
```

Expected: PR opens, merges via merge commit, branch deleted on remote. If `gh` is unavailable, do the equivalent through the GitHub UI.

- [ ] **Step 4: Update local `main` to merged state and record `NEW_SHA`.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
git fetch origin
git checkout main
git pull --ff-only origin main
NEW_SHA=$(git rev-parse HEAD)
echo "$NEW_SHA"
```

Write down `NEW_SHA`. It's needed for Phase 2.

---

## Task 11: lut-synth Phase 2 — single commit cleanup

**Files:**
- Modify: `tools/approx-tt.cpp` (strip `--h4` mode)
- Modify: `tools/approx-xag.cpp` (strip `--h4` mode)
- Delete: `src/lut-synth/h4-config.hpp`
- Delete: `scripts/`, `data/`, `figures/`, `docs/h4-qrom-structure.md`
- Delete: `gurobi.log`, `A_Dont-Care-Based_Approach_to_Reducing_the_Multiplicative_Complexity_in_Logic_Networks.pdf`
- Delete: `paper/qce2026` submodule (and the `paper/` dir if empty)
- Modify: `.gitmodules` (remove paper entry)
- Modify: `setup.sh` (C++-only)
- Modify: `README.md` (drop chemistry pipeline references, point to submodule)
- Modify: `.gitignore` (drop now-dead entries)
- Submodule pointer: bump `third-party/approx_qlut_simulation` to `NEW_SHA`

All staged and committed in a single commit.

- [ ] **Step 1: Bump the submodule pointer.**

```bash
cd /home/hanyu/lut-synth
git -C third-party/approx_qlut_simulation rev-parse HEAD
```

Confirm it equals `NEW_SHA` from Task 10. The pointer is staged together with the rest of the changes at the end.

- [ ] **Step 2: Strip `--h4` mode from `tools/approx-tt.cpp`.**

Open `/home/hanyu/lut-synth/tools/approx-tt.cpp` and apply these surgical edits:

a. Remove the include line:
   - Before: `#include "lut-synth/h4-config.hpp"`
   - After: (deleted)

b. Remove the H4 fields from `Args`:
   - Before:
     ```cpp
     std::vector<uint32_t> registers;
     std::vector<uint32_t> lock_indices;
     bool h4 = false;
     uint32_t rank = 0;
     uint32_t precision = 0;
     bool verbose = false;
     ```
   - After:
     ```cpp
     std::vector<uint32_t> registers;
     std::vector<uint32_t> lock_indices;
     bool verbose = false;
     ```

c. Remove the `--h4`, `--rank`, `--precision` parser branches:
   - Before:
     ```cpp
     } else if (arg == "--h4") {
         args.h4 = true;
     } else if (arg == "--rank" && i + 1 < argc) {
         args.rank = static_cast<uint32_t>(std::stoul(argv[++i]));
     } else if (arg == "--precision" && i + 1 < argc) {
         args.precision = static_cast<uint32_t>(std::stoul(argv[++i]));
     } else if (arg == "--verbose" || arg == "-v") {
     ```
   - After:
     ```cpp
     } else if (arg == "--verbose" || arg == "-v") {
     ```

d. Remove the unused alias:
   - Before:
     ```cpp
     using lut_synth::ceil_log2;
     using lut_synth::compute_h4_locked_bits;
     ```
   - After: (delete both lines; `ceil_log2` is also no longer referenced after the H4 block is gone)

e. Update the usage string:
   - Before:
     ```cpp
     std::cerr << "Usage: approx-tt --input <file> --output <file> "
               << "[--error-bound <val>] [--time-limit <sec>] "
               << "[--registers 1,1,6,6,6] [--lock 0,1] "
               << "[--h4 --rank <R> --precision <B>] [--verbose]\n";
     ```
   - After:
     ```cpp
     std::cerr << "Usage: approx-tt --input <file> --output <file> "
               << "[--error-bound <val>] [--time-limit <sec>] "
               << "[--registers 1,1,6,6,6] [--lock 0,1] [--verbose]\n";
     ```

f. Remove the validation + H4 mode in `main()`:
   - Before:
     ```cpp
     if (args.h4 && (!args.registers.empty() || !args.lock_indices.empty())) {
         std::cerr << "Error: --h4 is mutually exclusive with --registers and --lock\n";
         return 1;
     }
     if (args.h4 && (args.rank < 2 || args.precision < 1)) {
         std::cerr << "Error: --h4 requires --rank >= 2 and --precision >= 1\n";
         return 1;
     }

     lut_synth::TruthTable tt;
     tt.read(args.input);

     lut_synth::approximate::TTApproxParams params;
     params.error_bound = args.error_bound;
     params.time_limit = args.time_limit;
     params.verbose = args.verbose;

     if (args.h4) {
         uint32_t bw_mu = ceil_log2(args.rank);
         params.register_bitsizes = {1, 1, bw_mu, bw_mu, args.precision};

         uint32_t locked_bits = compute_h4_locked_bits(args.rank);
         uint32_t num_outputs = static_cast<uint32_t>(tt.get_tts().size());
         uint32_t expected_outputs = locked_bits + args.precision;
         if (num_outputs != expected_outputs) {
             std::cerr << "Error: H4 mode expects " << expected_outputs
                       << " outputs (got " << num_outputs << ") for rank="
                       << args.rank << " precision=" << args.precision << "\n";
             return 1;
         }
         params.locked_outputs.assign(num_outputs, false);
         for (uint32_t i = 0; i < locked_bits; ++i) {
             params.locked_outputs[i] = true;
         }
     } else {
         params.register_bitsizes = args.registers;
         if (!args.lock_indices.empty()) {
             uint32_t num_outputs = static_cast<uint32_t>(tt.get_tts().size());
             params.locked_outputs.assign(num_outputs, false);
             for (uint32_t idx : args.lock_indices) {
                 if (idx < num_outputs) {
                     params.locked_outputs[idx] = true;
                 }
             }
         }
     }
     ```
   - After:
     ```cpp
     lut_synth::TruthTable tt;
     tt.read(args.input);

     lut_synth::approximate::TTApproxParams params;
     params.error_bound = args.error_bound;
     params.time_limit = args.time_limit;
     params.verbose = args.verbose;
     params.register_bitsizes = args.registers;
     if (!args.lock_indices.empty()) {
         uint32_t num_outputs = static_cast<uint32_t>(tt.get_tts().size());
         params.locked_outputs.assign(num_outputs, false);
         for (uint32_t idx : args.lock_indices) {
             if (idx < num_outputs) {
                 params.locked_outputs[idx] = true;
             }
         }
     }
     ```

Verify the file compiles (deferred to step 9 build gate).

- [ ] **Step 3: Strip `--h4` mode from `tools/approx-xag.cpp`.**

Apply the same five-part edit. Open `/home/hanyu/lut-synth/tools/approx-xag.cpp` and:

a. Remove `#include "lut-synth/h4-config.hpp"`.
b. Remove `bool h4`, `uint32_t rank`, `uint32_t precision` fields from `Args`.
c. Remove the `--h4`, `--rank`, `--precision` parser branches.
d. Remove `using lut_synth::compute_h4_locked_bits;` (and `using lut_synth::ceil_log2;` if it becomes unreferenced).
e. Update the usage string to drop `[--h4 --rank R --precision P]`.
f. Replace the H4-mode block in `main()` with the equivalent generic-flag-only path: keep `params.register_bitsizes = args.registers;` and the existing `lock_indices` assignment.

Inspect with:

```bash
grep -n "h4\|H4\|rank\|precision\|h4-config" /home/hanyu/lut-synth/tools/approx-xag.cpp
```

Expected: no matches. (`approximate` and other unrelated tokens are fine — the grep above is a sanity check, not exact.)

- [ ] **Step 4: Delete `h4-config.hpp`. Leave `paper/qce2026` submodule untouched.**

```bash
cd /home/hanyu/lut-synth
git rm src/lut-synth/h4-config.hpp
```

Verify:

```bash
ls src/lut-synth/h4-config.hpp 2>/dev/null && echo "STILL THERE" || echo "gone"
git submodule status paper/qce2026
```

Expected: `gone`; `paper/qce2026` submodule still listed (its pointer drift remains as `M paper/qce2026` in `git status` and is NOT staged in this commit — see Step 11).

- [ ] **Step 5: Delete moved-out paths.**

```bash
cd /home/hanyu/lut-synth
git rm -r scripts data figures docs/h4-qrom-structure.md
git rm gurobi.log A_Dont-Care-Based_Approach_to_Reducing_the_Multiplicative_Complexity_in_Logic_Networks.pdf
# results/ is gitignored — physically remove (optional, since it's untracked):
rm -rf results
# Stray __pycache__ directories at root:
find . -path './third-party' -prune -o -type d -name '__pycache__' -print -exec rm -rf {} + 2>/dev/null || true
```

Verify:

```bash
git status --short | grep -E '^D' | head
ls scripts data figures 2>/dev/null && echo "STILL THERE" || echo "gone"
```

Expected: `D` lines for all the deleted paths; the `ls` says `gone`.

- [ ] **Step 6: Rewrite `setup.sh` to C++-only.**

```bash
cd /home/hanyu/lut-synth
cat > setup.sh <<'EOF'
#!/usr/bin/env bash
#
# One-click C++ setup for lut-synth: clones submodules, configures and
# builds with tests + tools, optionally runs ctest.
#
# Flags:
#   --no-test    skip ctest
#   --jobs N     parallelism for cmake --build (default: nproc)
#
# Requirements: git, cmake >= 3.16, a C++17 compiler.
# Optional: Gurobi (auto-detected via cmake/FindGUROBI.cmake, enables ILP
# in approx-tt).
#
# For the experiments harness (Python, conda, chemistry pipeline,
# Shor's flow), see third-party/approx_qlut_simulation/setup.sh.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

DO_TEST=1
JOBS="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-test) DO_TEST=0; shift ;;
    --jobs)    JOBS="$2"; shift 2 ;;
    -h|--help) sed -n '3,15p' "$0"; exit 0 ;;
    *) echo "Unknown flag: $1" >&2; exit 1 ;;
  esac
done

log() { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }

log "Initializing git submodules"
git submodule update --init --recursive

log "Configuring cmake (BUILD_TESTS=ON, BUILD_TOOLS=ON)"
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON

log "Building (jobs=$JOBS)"
cmake --build build -j"$JOBS"

if [[ "$DO_TEST" == 1 ]]; then
  log "Running ctest"
  ctest --test-dir build --output-on-failure
fi

log "Setup complete"
EOF
chmod +x setup.sh
```

- [ ] **Step 7: Rewrite `README.md`.**

```bash
cd /home/hanyu/lut-synth
cat > README.md <<'EOF'
# lut-synth

Truth-table to XAG logic synthesis library: exact and approximate.
Extracted from HLQCS.

## Getting Started

```bash
git clone --recursive git@github.com:Nozidoali/lut-synth.git
cd lut-synth
./setup.sh
```

`./setup.sh` clones submodules, builds the C++ library + CLI tools,
and runs ctest. Flags: `--no-test`, `--jobs N`.

## Manual build

```bash
git submodule update --init --recursive
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

This produces `build/approx-tt`, `build/synth-tt`, `build/approx-xag`,
and `build/liblut-synth.a`.

## Requirements

- C++17 compiler, CMake >= 3.16
- mockturtle (submodule in `third-party/`)
- Gurobi (optional, auto-detected; enables ILP-based approximation in
  `approx-tt`)

## Command-line tools

```bash
build/approx-tt  --input my.tt --output my.approx.tt \
                 --error-bound 1.0 --time-limit 60 \
                 --registers 1,1,6,6,6 --lock 0,1,2,3,4,5

build/approx-xag --input my.tt --output my.xag \
                 --method narrow --error-bound 1.0

build/synth-tt   --input my.approx.tt --num-random-starts 1
```

`--registers` is a comma-separated list of per-register bit-widths;
`--lock` lists the output bit indices that must be reproduced exactly
(no don't-cares).

## Experiments

End-to-end experiments — chemistry pipeline (PySCF → THC → qualtran
`PrepareTHC` → CCSD(T)), Shor's algorithm flow, random-TT benchmark —
live in the `third-party/approx_qlut_simulation/` submodule, which
calls back into this repo's CLI tools via the `LUT_SYNTH_ROOT`
environment variable. See its README for usage.

## Layout

```
lut-synth/
├── src/lut-synth/        # library: synthesis/, resynthesis/, approximate/
├── tools/                # CLI: approx-tt, approx-xag, synth-tt
├── test/                 # Catch2 unit tests
├── third-party/
│   ├── mockturtle/                       (submodule)
│   ├── qlut-benchmarks/                  (submodule)
│   └── approx_qlut_simulation/           (submodule, experiments)
└── docs/
    └── approximation-methods.md
```
EOF
```

- [ ] **Step 8: Tidy `.gitignore`.**

```bash
cd /home/hanyu/lut-synth
cat > .gitignore <<'EOF'
# Build
build/
build-debug/

# IDE / editor
.cache/
.vscode/
.idea/
*.swp
*.swo
*~

# Python
__pycache__/
*.pyc

# OS
.DS_Store
Thumbs.db

# Tool artifacts
gurobi.log
.claude/scheduled_tasks.lock

# LaTeX build artifacts
paper/*/*.aux
paper/*/*.log
paper/*/*.bbl
paper/*/*.blg
paper/*/*.out
paper/*/*.fls
paper/*/*.fdb_latexmk
paper/*/*.synctex.gz
EOF
```

(Removed: `results/` exclusion since the dir no longer exists. LaTeX
artifacts kept since `paper/qce2026` submodule stays.)

- [ ] **Step 9: Build + ctest gate. Must be green before commit.**

```bash
cd /home/hanyu/lut-synth
rm -rf build
cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds, build succeeds (with the H4 includes/usings/calls fully removed in steps 2–3), `approx-tt`, `approx-xag`, `synth-tt` all link, all tests pass. If anything fails, fix in place — DO NOT commit a red build.

- [ ] **Step 10: Smoke-test the rebuilt CLI tools.**

```bash
cd /home/hanyu/lut-synth
build/approx-tt 2>&1 | head -5
build/approx-xag 2>&1 | head -5
build/synth-tt 2>&1 | head -5
```

Expected: usage strings print without segfault. None of them mention `--h4` / `--rank` / `--precision` any more.

- [ ] **Step 11: Stage everything and review the diff.**

```bash
cd /home/hanyu/lut-synth
git add -A tools/ src/ setup.sh README.md .gitignore third-party/approx_qlut_simulation
git status
git diff --staged --stat | tail
```

Expected: stage summary lists the deleted directories, modified C++ files, modified setup/README/gitignore, and the submodule pointer bump. The `paper/qce2026` submodule pointer drift must NOT be staged — it stays as ` M paper/qce2026` (unstaged) in the working tree both before and after the commit. Verify with `git diff --staged paper/qce2026` (should print nothing).

- [ ] **Step 12: Single commit.**

```bash
cd /home/hanyu/lut-synth
git commit -m "$(cat <<'EOF'
Split experiments into approx_qlut_simulation submodule

- Bump third-party/approx_qlut_simulation pointer to import the
  chemistry, shors, randomTT, common helpers, tracked artifacts,
  H4 docs, setup.sh, and the path rewiring (LUT_SYNTH_ROOT).
- Strip H4-specific --h4/--rank/--precision mode from
  tools/approx-tt and tools/approx-xag; delete src/lut-synth/h4-config.hpp.
- Remove now-duplicated paths: scripts/, data/, figures/,
  docs/h4-qrom-structure.md, gurobi.log, root-level PDF.
- Rewrite setup.sh as C++-only and README.md to point at the
  experiments submodule for Python / conda / chemistry usage.
EOF
)"
```

Expected: one new lut-synth commit containing all the above.

- [ ] **Step 13: Sanity-check post-commit state.**

```bash
cd /home/hanyu/lut-synth
git log --stat -1
git submodule status
ls scripts data figures 2>/dev/null && echo "STILL THERE" || echo "gone"
ls src/lut-synth/h4-config.hpp 2>/dev/null && echo "STILL THERE" || echo "gone"
grep -n h4-config tools/approx-tt.cpp tools/approx-xag.cpp 2>/dev/null || echo "no h4 includes"
git status --short
```

Expected: deleted dirs/file are `gone`; submodule status shows `mockturtle`, `qlut-benchmarks`, `approx_qlut_simulation`, AND `paper/qce2026` (untouched); `no h4 includes`. `git status --short` should show ` M paper/qce2026` (the in-flight pointer drift, unstaged) as the only entry.

---

## Task 12: End-to-end smoke test

**Files:** none (validates that the new submodule scripts can call the rebuilt lut-synth CLIs).

- [ ] **Step 1: Construct a tiny truth-table fixture.**

The `.tt` format is one row per output, each row a string of `0`/`1`/`X` covering all 2^n input minterms (n is inferred from row length, which must be a power of two). All rows in a file must have the same length.

```bash
mkdir -p /tmp/lut_split_smoke
# Two outputs over 4 inputs (16 chars per row):
#   row 1 — outputs the 4th input bit
#   row 2 — outputs the 1st input bit (alternating)
cat > /tmp/lut_split_smoke/in.tt <<'EOF'
0000000011111111
0101010101010101
EOF
```

- [ ] **Step 2: Invoke approx-tt directly via `LUT_SYNTH_ROOT` to confirm the env-var convention works.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
LUT_SYNTH_ROOT="$(cd ../.. && pwd)"
"$LUT_SYNTH_ROOT/build/approx-tt" \
  --input /tmp/lut_split_smoke/in.tt \
  --output /tmp/lut_split_smoke/out.tt \
  --registers 2,2 --error-bound 0.0 \
  --time-limit 5
echo "exit=$?"
ls -l /tmp/lut_split_smoke/out.tt
```

Expected: `approx-tt` prints a JSON line containing `"solved":true` (or `"solved":false` if Gurobi is not installed — that is also acceptable; the binary linked and ran), exits 0, and writes `out.tt`. The whole call demonstrates the `LUT_SYNTH_ROOT/build/approx-tt` invocation pattern that the experiment scripts use.

- [ ] **Step 3: Confirm a representative experiment script can find lut-synth via `LUT_SYNTH_ROOT`.**

```bash
cd /home/hanyu/lut-synth/third-party/approx_qlut_simulation
python -c "
import os, sys
from pathlib import Path
os.environ.setdefault('LUT_SYNTH_ROOT', str(Path('../..').resolve()))
sys.path.insert(0, 'shors/src')
sys.path.insert(0, 'common')
import shor_e2e  # should import without errors
print('shor_e2e import: OK')
print('LUT_SYNTH_ROOT =', os.environ['LUT_SYNTH_ROOT'])
"
```

Expected: `shor_e2e import: OK` and `LUT_SYNTH_ROOT` resolves to the lut-synth root.

- [ ] **Step 4: Done. Push lut-synth (optional, leave to user).**

```bash
cd /home/hanyu/lut-synth
git log --oneline -1
```

Inform the user the refactor is complete locally and offer to push.
The user controls when (and if) to push to `origin/main`.

---

## Rollback

If anything goes badly wrong:

- **During Phase 1 (Tasks 2–9):** `git -C third-party/approx_qlut_simulation checkout main && git branch -D refactor/import-experiments`. No remote impact yet.
- **After Phase 1 push but before merge (Task 10):** close the PR, `git push origin --delete refactor/import-experiments`.
- **After Phase 1 merge but before Phase 2 commit (Task 11):** revert the merge commit on approx_qlut_simulation `main` and re-bump the submodule on lut-synth to the old SHA recorded in Task 1 step 4.
- **After Phase 2 commit:** `git -C /home/hanyu/lut-synth revert HEAD` produces a clean revert; the submodule pointer also reverts.
