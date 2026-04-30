# Shor's pipeline scripts

## Layout
- `run.py`, `analyze.py` are the unified entry points
- `plot_*.py` are plotting utilities
- `src/` holds importable libraries (no CLI)
- `figures/` holds paper figure generators

## Entries

### `run.py` — unified experiment runner
Subcommands:

| Subcommand | What it does |
|---|---|
| `mono` | Monolithic Shor's: single 2^m-entry LUT |
| `windowed` | Chained windowed Shor's: k = ⌈m/w⌉ per-window LUTs |
| `eb-alloc` | Per-window ε_B allocation strategies (uniform / front / back / linear) |
| `synth-only` | Multi-start synthesis audit (no approximation) |

Common args: `--eb`, `--shots`, `--workdir`, `--output`, `--synth`, `--approx`, `--num-random-starts`.

### `analyze.py` — unified analyzer
Reads a sweep JSON and applies opt-in analyses:

| Flag | What it adds |
|---|---|
| `--bound` | Closed-form `(1−δ)²·P_succ(0)` lower bound rows |
| `--eh --workdir DIR` | Ekera-Hastad multi-shot success per `s` |
| `--ft-cost --p-phys X --eps-budget X ...` | FTQC space-time volume columns + plot |

### Plotting
| Script | What it does |
|---|---|
| `plot_shor.py` | Pareto + per-case panels from a sweep JSON |
| `plot_shor_period.py` | Period autocorrelation degradation under approximation |

### Other
| Script | What it does |
|---|---|
| `shor_qiskit.py` | One-off Qiskit cross-check of approximated LUTs (not on critical path) |
| `run_shor_chained_flow.sh` | Orchestrator: windowed → plots → FT cost model |

## Libraries (`src/`)
- `shor_lib.py` — TT decode, period SNR, continued-fraction post-processing, gcd factoring
- `shor_e2e.py` — `run_case` for monolithic Shor's
- `shor_chained.py` — `run_case` for windowed Shor's
- `per_window.py` — `allocate(strategy, k, total)` + `run_alloc`
- `synth_baseline.py` — `run_audit` (synthesis-only multi-start)
- `shor_psucc_bound.py` — `compute_bounds`, `report_bound_violations`
- `shor_eh.py` — Ekera-Hastad analysis (`evaluate_case`, `run_eh_on_workdir`)
- `model_ft_cost.py` — `analyze_case`, `required_distance`, `cycle_error`, `plot_savings`
