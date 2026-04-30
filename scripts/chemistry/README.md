# Chemistry / THC / QROM scripts

## Layout
- `*.py` at this level are entry points (run with `python scripts/chemistry/<name>.py`)
- `src/` holds importable libraries (no CLI)
- `thc_experiments/` is a self-contained THC benchmark sub-pipeline

## Entries
| Script | What it does |
|---|---|
| `qlut_pipeline.py` | Main pipeline: THC tensors → QLUT extract → narrow/ILP approx → QROM accuracy |
| `run_sweep.py` | Configurable ε_B sweep over QLUTs (SS-DC synthesis) |
| `run_4bit_sweep.py` | Aggressive 4-bit state-prep sweep on H4 rank-56 |
| `plot_rank_sweep.py` | Multi-rank × multi-bit H4 sweep with Pareto plot |
| `eval_qrom_fidelity.py` | Alias-sampling state-prep infidelity from approximated keep/alias LUTs |
| `sweep_h4_eb.sh` | Bash sweep over ε_B for H4/sto-3g at fixed num_bits |
| `thc_experiments/run_thc_sweep.py` | Full QROM-narrow sweep on THC benchmarks |
| `thc_experiments/augment_summaries_with_tv.py` | Adds TV distance to summary.json via Verilog sim |
| `thc_experiments/plot_thc_pareto.py` | AND-saved vs state-prep infidelity Pareto |

## Libraries (`src/`)
- `qlut_pipeline.py` — `extract_qluts`, `approximate_qluts`, `synthesize_qluts`, `compute_qrom_errors`, `decode_and_reconstruct`, `evaluate`, `run_pipeline`, `_NumpyEncoder`, `_save_json`, `_thc_tensors_to_hamiltonian`
- `eval_qrom_fidelity.py` — `amplitudes_to_probs`, `build_alias_table`, `prepared_probs`, `bhattacharyya_fidelity`, `total_variation`, `simulate_qrom_verilog`, `eval_infidelity`, `eval_from_verilog`
