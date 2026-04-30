from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from eval_qrom_fidelity import eval_from_verilog  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--amps", type=str, required=True,
                    help="JSON file with n_bits + amplitudes dict")
    ap.add_argument("--precision-bits", type=int, required=True)
    ap.add_argument("--keep-verilog", type=str, default=None)
    ap.add_argument("--alias-verilog", type=str, default=None)
    ap.add_argument("--n-bits", type=int, default=None)
    args = ap.parse_args()

    d = json.loads(Path(args.amps).read_text())
    n_bits = args.n_bits if args.n_bits is not None else int(d["n_bits"])
    res = eval_from_verilog(args.amps, n_bits, args.precision_bits,
                            args.keep_verilog, args.alias_verilog)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
