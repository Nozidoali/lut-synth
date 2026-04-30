from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from qsp_qrom_to_verilog import run_converter  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=str, required=True,
                    help="JSON file with list of ints, or comma-separated ints")
    ap.add_argument("--addr-bits", type=int, required=True)
    ap.add_argument("--data-bits", type=int, required=True)
    ap.add_argument("--qspc", type=str, default="/tmp/qspc")
    ap.add_argument("--output", type=str, required=True)
    ap.add_argument("--no-select-swap", action="store_true")
    args = ap.parse_args()

    if args.data.endswith(".json"):
        data = json.loads(Path(args.data).read_text())
    else:
        data = [int(x) for x in args.data.split(",")]

    stats = run_converter(
        data, args.addr_bits, args.data_bits,
        args.qspc, args.output,
        use_select_swap=not args.no_select_swap,
    )
    print(json.dumps(stats, indent=2))


if __name__ == "__main__":
    main()
