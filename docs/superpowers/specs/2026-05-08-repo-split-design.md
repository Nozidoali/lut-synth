# Repo split: lut-synth ↔ approx_qlut_simulation

Date: 2026-05-08
Status: design approved, awaiting implementation plan

## Goal

Reduce the lut-synth repo to a focused TT→XAG synthesis library + CLI
tools (exact + approximate). Move all H4 / Shor's / random-TT experiment
code, artifacts, and H4-specific docs into the existing
`third-party/approx_qlut_simulation/` submodule
(`WanHsuanLin/approx_qlut_simulation`), which is then bumped to a new
SHA from lut-synth.

After this refactor:

- lut-synth = library (`src/lut-synth/`) + CLI tools (`tools/`) + tests.
- approx_qlut_simulation = experiment scripts, artifacts, and CCSD(T)
  baselines that consume the lut-synth CLI tools.
- Cross-repo dependency direction is one-way: experiments call into
  lut-synth, never the reverse.

## Non-goals

- No restructuring of `src/lut-synth/` internals (synthesis /
  resynthesis / approximate stay as-is).
- No changes to `mockturtle` or `qlut-benchmarks` submodules.
- No new abstraction layer on the experiment side; scripts remain
  thin wrappers, only path conventions change.
- approx_qlut_simulation is NOT made independently runnable. It
  assumes it sits at `lut-synth/third-party/approx_qlut_simulation/`.
- The `narrow-resub.hpp` doc comment that references "H4 QROM keep" is
  an example, not a dependency, and stays as-is.

## Final layout

### lut-synth (after refactor)

```
lut-synth/
├── CMakeLists.txt
├── README.md                 # rewritten: TT→XAG library + CLI
├── setup.sh                  # rewritten: C++ build only
├── cmake/
├── src/lut-synth/
│   ├── synthesis/
│   ├── resynthesis/
│   ├── approximate/
│   ├── error.{cpp,hpp}
│   ├── string-util.{cpp,hpp}
│   └── truth-table.{cpp,hpp}
├── tools/                    # approx-tt, synth-tt, approx-xag
├── test/                     # Catch2; no H4/Shor refs
├── third-party/
│   ├── mockturtle/                       (submodule, unchanged)
│   ├── qlut-benchmarks/                  (submodule, unchanged)
│   └── approx_qlut_simulation/           (submodule, bumped)
└── docs/
    ├── approximation-methods.md
    └── superpowers/
```

### approx_qlut_simulation (after refactor)

```
approx_qlut_simulation/
├── README.md                 # extended: usage + lut-synth dependency
├── setup.sh                  # new: conda env creation
├── pyproject.toml
├── main.py                   # existing
├── ccsd_t.py                 # existing
├── convert_thc.py            # existing
├── example_thc_conversion.py # existing
├── chemistry/                # ← scripts/chemistry/
├── shors/                    # ← scripts/shors/
├── randomTT/                 # ← scripts/randomTT/
├── common/                   # ← scripts/src/ + plot_paper.py + qsp_qrom_to_verilog.py
├── data/                     # ← lut-synth/data/
├── results/                  # ← lut-synth/results/
├── figures/                  # ← lut-synth/figures/
└── docs/
    └── h4-qrom-structure.md  # ← lut-synth/docs/
```

## Cross-repo invocation convention

All experiment scripts in approx_qlut_simulation call lut-synth CLI
tools through a shared root variable:

```
LUT_SYNTH_ROOT="${LUT_SYNTH_ROOT:-../..}"
$LUT_SYNTH_ROOT/build/approx-tt ...
$LUT_SYNTH_ROOT/build/synth-tt ...
```

The default `../..` is interpreted relative to **the approx_qlut_simulation
repo root** (i.e. the convention is: invoke scripts from
`lut-synth/third-party/approx_qlut_simulation/`, e.g.
`python chemistry/run_sweep.py ...`, not from inside the
subdirectory). With that convention `../..` from approx_qlut_simulation
root resolves to `lut-synth/`. Users can override `LUT_SYNTH_ROOT` to an
absolute path when running from anywhere else. All hardcoded
`../build/...`, `../../build/...`, or absolute path patterns in the
imported scripts are rewritten to use `LUT_SYNTH_ROOT` in phase 1
commit 8.

## Items moved out of lut-synth

| Source                                         | Destination                              |
|------------------------------------------------|------------------------------------------|
| `scripts/chemistry/`                           | `chemistry/`                             |
| `scripts/shors/`                               | `shors/`                                 |
| `scripts/randomTT/`                            | `randomTT/`                              |
| `scripts/src/`, `scripts/plot_paper.py`, `scripts/qsp_qrom_to_verilog.py` | `common/`            |
| `data/`                                        | `data/`                                  |
| `results/` (all `h4_*`, `shor_*`, `thc_*`, `paper`, etc.) | `results/`                    |
| `figures/`                                     | `figures/`                               |
| `docs/h4-qrom-structure.md`                    | `docs/h4-qrom-structure.md`              |
| `setup.sh` Python/conda portion                | `setup.sh` (new)                         |

## Items deleted outright from lut-synth

| Path                                                           | Reason                          |
|----------------------------------------------------------------|---------------------------------|
| `src/lut-synth/h4-config.hpp`                                  | unused; H4-specific constants   |
| `paper/qce2026` (submodule + `.gitmodules` entry)              | already pushed to overleaf      |
| `gurobi.log` (root)                                            | runtime log, should not be tracked |
| `A_Dont-Care-Based_Approach_to_Reducing_the_Multiplicative_Complexity_in_Logic_Networks.pdf` (root) | unrelated PDF |
| Root-level `__pycache__/` / stray Python caches                | not source                      |

## Migration sequence — Option A

Single direction: submodule first, then lut-synth. Submodule SHA is
always recoverable from any lut-synth checkout, so no "missing
content" window exists.

### Phase 1 — populate approx_qlut_simulation

Operate inside `third-party/approx_qlut_simulation/` (or a fresh clone
of the submodule repo). All commits land on a feature branch, e.g.
`refactor/import-experiments`, then merge to `main`.

1. **Branch.** `git checkout -b refactor/import-experiments` from `main`.
2. **commit 1** — `import chemistry experiments`: copy `scripts/chemistry/*` → `chemistry/`.
3. **commit 2** — `import shors experiments`: copy `scripts/shors/*` → `shors/`.
4. **commit 3** — `import randomTT benchmark`: copy `scripts/randomTT/*` → `randomTT/`.
5. **commit 4** — `import shared helpers`: copy `scripts/src/*`, `scripts/plot_paper.py`, `scripts/qsp_qrom_to_verilog.py` → `common/`. Update any `from src.<x>` imports to `from common.<x>`.
6. **commit 5** — `import experiment artifacts`: copy `data/`, `results/`, `figures/`. Update `.gitignore` to exclude `__pycache__/`, scratch outputs, and any oversized intermediates flagged in pre-commit review.
7. **commit 6** — `import H4 docs`: copy `docs/h4-qrom-structure.md` → `docs/`.
8. **commit 7** — `add setup.sh and update README`: extract conda env / pip install logic from lut-synth/setup.sh into a new local setup.sh; expand README with usage and the `LUT_SYNTH_ROOT` convention.
9. **commit 8** — `fix paths to use LUT_SYNTH_ROOT`: grep all imported scripts for `../build/`, `../../build/`, `../../scripts/`, absolute paths under `/home/`, etc.; rewrite to `${LUT_SYNTH_ROOT:-../..}/...`.
10. Push branch, open PR, merge. Record merge commit SHA as `NEW_SHA`.

### Phase 2 — lut-synth cleanup (single commit)

Operate in lut-synth root.

11. **Bump submodule pointer.** `git submodule update --remote third-party/approx_qlut_simulation` (or `git -C third-party/approx_qlut_simulation checkout NEW_SHA`).
12. **Delete moved-out paths.** `git rm -r scripts data results figures docs/h4-qrom-structure.md src/lut-synth/h4-config.hpp gurobi.log A_Dont-Care-Based_*.pdf`. Remove any root-level `__pycache__/`.
13. **Drop `paper/qce2026` submodule.** Edit `.gitmodules` to remove the entry, `git rm paper/qce2026`, remove `paper/` if empty, clean `.git/modules/paper/qce2026`.
14. **Rewrite `setup.sh`.** Strip out Python env / conda / pip install branches; keep submodule init, cmake configure/build, optional ctest. Flags retained: `--cpp-only` becomes default behavior (so the flag itself can be removed), `--no-test`, `--jobs N`. `--python-only` and `--env NAME` are removed.
15. **Rewrite `README.md`.** Drop chemistry-pipeline references. Add a short "Experiments" section that points readers to `third-party/approx_qlut_simulation/` and explains the `LUT_SYNTH_ROOT` convention.
16. **Update `.gitignore`** if needed (e.g. `build/`, `build-debug/` already covered; ensure no stale exclusions referencing deleted paths).
17. **Verify build.** `cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON && cmake --build build -j8 && ctest --test-dir build --output-on-failure`. Must be green before commit.
18. **Single commit.** `refactor: split experiments into approx_qlut_simulation submodule`. Body lists deletions, the bumped SHA, and the new README/setup.sh shape.

## Risks and mitigations

- **Submodule push permissions.** Phase 1 requires write access to
  `WanHsuanLin/approx_qlut_simulation`. Verify before starting; if
  unavailable, prepare a fork and reroute remote at the end.
- **Path drift in imported scripts.** Phase 1 commit 8 must catch every
  hardcoded path. Use `grep -rE '(\.\./)+build/|/home/'` over the
  imported tree as a pre-commit check.
- **Large artifacts.** `results/` contains ~25 directories
  (`h4_*`, `shor_*`, `thc_*`, etc.); some may be large. Before commit
  5, run `du -sh results/* data/* figures/*` and confirm with the user
  which subtrees to track vs. `.gitignore` vs. move to LFS.
- **Irreversible deletions in lut-synth.** `paper/qce2026` submodule
  removal and `results/` deletion are confirmed-once actions. Verify
  pre-deletion inventory matches what landed in `NEW_SHA`.
- **Tests must stay green.** Phase 2 step 17 is a hard gate. If ctest
  fails, fix before committing — never commit a red build.
- **Rollback.** Phase 1 is revertable until merge; phase 2 is a single
  lut-synth commit, `git revert` clears it.

## Acceptance criteria

- `cmake -B build -DBUILD_TESTS=ON -DBUILD_TOOLS=ON && cmake --build build -j8 && ctest` passes on lut-synth post-refactor.
- `lut-synth/scripts/`, `lut-synth/data/`, `lut-synth/results/`, `lut-synth/figures/`, `lut-synth/docs/h4-qrom-structure.md`, `lut-synth/src/lut-synth/h4-config.hpp`, `lut-synth/paper/qce2026` no longer exist.
- `git -C third-party/approx_qlut_simulation log --oneline` on lut-synth `HEAD` shows the new commits and the chemistry/shors/randomTT/common/data/results/figures/docs trees.
- A representative experiment script in approx_qlut_simulation runs end-to-end against `$LUT_SYNTH_ROOT/build/approx-tt` (smoke test, not full sweep).
- lut-synth README and setup.sh do not mention chemistry, H4, Shor's, or conda.
