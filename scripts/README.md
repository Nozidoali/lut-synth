# scripts/

Application-organized layout. Each application folder has its own entry
points (CLI scripts) and `src/` libraries.

```
scripts/
├── shors/        — Shor's algorithm / modexp pipeline
├── chemistry/    — chemistry / THC / QROM pipeline
├── randomTT/     — method-layer experiments (HLQCS, narrow schematic)
├── src/          — cross-application shared libraries
├── plot_paper.py — aggregates results across apps
└── qsp_qrom_to_verilog.py — CLI wrapper for src/qsp_qrom_to_verilog
```

## Entry vs library

- **Entry scripts** sit at folder root. Have `if __name__ == "__main__"`.
  Import their lib counterpart from `src/` and add CLI on top.
- **Libraries** live in `src/`. No `argparse`, no `main()`. Import them
  via `from <name> import ...` after adding the right `src/` to
  `sys.path`.

## Shared libraries (`scripts/src/`)
- `decode.py` — TT file I/O, decoding (used by chemistry pipeline)
- `qsp_qrom_to_verilog.py` — Qiskit SelectSwap QROM → Verilog XAG
