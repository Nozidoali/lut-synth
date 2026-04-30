# Shor's pipeline scripts

## Layout
- `*.py` at this level are entry points (run with `python scripts/shors/<name>.py`)
- `src/` holds importable libraries (no CLI)
- `figures/` holds paper figure generators

## Entries
| Script | What it does |
|---|---|
| `shor_e2e.py` | Monolithic Shor's: modexp TT → narrow approx → period post-processing |
| `shor_chained.py` | Windowed Shor's: k per-window LUTs, each approximated independently |
| `shor_qiskit.py` | Qiskit cross-validation of approximated LUTs |
| `shor_eh.py` | Ekera-Hastad multi-shot order finding on chained outputs |
| `shor_psucc_bound.py` | Validates theoretical (1−δ)²·P_succ(0) ≤ P_succ(ε) bound |
| `synth_baseline.py` | Multi-start synthesis audit (isolates synth variance from approx benefit) |
| `per_window_eb_sweep.py` | Per-window ε_B allocation strategies for chained Shor's |
| `model_ft_cost.py` | FTQC space-time volume model V = E[trials]·Q·D·d³ |
| `plot_shor.py` | Pareto + per-case panels from sweep JSON |
| `plot_shor_period.py` | Period autocorrelation degradation under approximation |
| `run_shor_chained_flow.sh` | Orchestrator: chained → plots → FT cost model |

## Libraries (`src/`)
- `shor_lib.py` — TT decode, period SNR, continued-fraction post-processing, gcd factoring
- `shor_chained.py` — `run_case()` for chained windowed pipeline (CLI wrapper above imports it)
- `model_ft_cost.py` — `analyze_case`, `required_distance`, `cycle_error`, `plot_savings`
