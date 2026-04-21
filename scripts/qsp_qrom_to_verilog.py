"""Convert qsp-compilation SelectSwap QROM (Qiskit x/cx/ccx) to Verilog XAG.

Walks the Qiskit circuit symbolically, keeps one XAG literal per qubit, and
emits a Verilog file with explicit AND/XOR/NOT assigns. Format matches what
mockturtle's verilog_reader expects.

Gate translation:
  x  q       : signal[q] ^= 1
  cx c t     : signal[t] = xor(signal[t], signal[c])
  ccx c0 c1 t: signal[t] = xor(signal[t], and(signal[c0], signal[c1]))

Address qubits (input_qubits) seed the PIs; all other qubits start at const 0.
Data qubits (output_qubits) drive the POs.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


CONST0 = 0
CONST1 = 1


@dataclass
class XAG:
    num_pis: int
    gates: List[Tuple[str, int, int]]

    def node_count(self) -> int:
        return 1 + self.num_pis + len(self.gates)

    def and_count(self) -> int:
        return sum(1 for g in self.gates if g[0] == "and")

    def xor_count(self) -> int:
        return sum(1 for g in self.gates if g[0] == "xor")


class XAGBuilder:
    def __init__(self, num_pis: int) -> None:
        self.num_pis = num_pis
        self.gates: List[Tuple[str, int, int]] = []
        self._hash: dict = {}

    def pi(self, i: int) -> int:
        assert 0 <= i < self.num_pis
        return 2 * (i + 1)

    def const0(self) -> int:
        return CONST0

    def const1(self) -> int:
        return CONST1

    def not_(self, lit: int) -> int:
        return lit ^ 1

    def _mk_node(self, op: str, a: int, b: int) -> int:
        node_id = 1 + self.num_pis + len(self.gates)
        self.gates.append((op, a, b))
        return 2 * node_id

    def and_(self, a: int, b: int) -> int:
        if a == CONST0 or b == CONST0:
            return CONST0
        if a == CONST1:
            return b
        if b == CONST1:
            return a
        if a == b:
            return a
        if a ^ b == 1:
            return CONST0
        lo, hi = (a, b) if a < b else (b, a)
        key = ("and", lo, hi)
        if key in self._hash:
            return self._hash[key]
        lit = self._mk_node("and", lo, hi)
        self._hash[key] = lit
        return lit

    def xor_(self, a: int, b: int) -> int:
        if a == CONST0:
            return b
        if b == CONST0:
            return a
        if a == CONST1:
            return self.not_(b)
        if b == CONST1:
            return self.not_(a)
        if a == b:
            return CONST0
        if a ^ b == 1:
            return CONST1
        inv = (a & 1) ^ (b & 1)
        a &= ~1
        b &= ~1
        lo, hi = (a, b) if a < b else (b, a)
        key = ("xor", lo, hi)
        if key in self._hash:
            return self._hash[key] ^ inv
        lit = self._mk_node("xor", lo, hi)
        self._hash[key] = lit
        return lit ^ inv

    def build(self) -> XAG:
        return XAG(num_pis=self.num_pis, gates=list(self.gates))


def qiskit_qrom_to_xag(qc, input_qubits: List[int], output_qubits: List[int]):
    n = qc.num_qubits
    builder = XAGBuilder(len(input_qubits))
    signal = [CONST0] * n
    for i, q in enumerate(input_qubits):
        signal[q] = builder.pi(i)

    for inst in qc.data:
        name = inst.operation.name
        qs = [qc.find_bit(q).index for q in inst.qubits]
        if name == "x":
            (q,) = qs
            signal[q] = builder.not_(signal[q])
        elif name == "cx":
            c, t = qs
            signal[t] = builder.xor_(signal[t], signal[c])
        elif name == "ccx":
            c0, c1, t = qs
            a = builder.and_(signal[c0], signal[c1])
            signal[t] = builder.xor_(signal[t], a)
        else:
            raise ValueError(f"unsupported gate: {name}")

    pos = [signal[q] for q in output_qubits]
    return builder.build(), pos


def _lit_name(lit: int, num_pis: int) -> Tuple[str, bool]:
    node = lit >> 1
    inv = bool(lit & 1)
    if node == 0:
        return ("1'b1" if inv else "1'b0", False)
    if node <= num_pis:
        return (f"pi_{node - 1}", inv)
    return (f"w_{node - num_pis - 1}", inv)


def _fmt(lit: int, num_pis: int) -> str:
    name, inv = _lit_name(lit, num_pis)
    if name.startswith("1'b"):
        return name
    return f"~{name}" if inv else name


def write_verilog(xag: XAG, po_lits: List[int], path: str,
                  module_name: str = "top") -> None:
    n_pi = xag.num_pis
    n_po = len(po_lits)
    n_gate = len(xag.gates)
    pi_names = [f"pi_{i}" for i in range(n_pi)]
    po_names = [f"po_{i}" for i in range(n_po)]
    wire_names = [f"w_{k}" for k in range(n_gate)]

    lines = []
    lines.append(f"module {module_name}( "
                 + ", ".join(pi_names + po_names) + " ) ;")
    if pi_names:
        lines.append("input " + ", ".join(pi_names) + " ;")
    lines.append("output " + ", ".join(po_names) + " ;")
    if wire_names:
        lines.append("wire " + ", ".join(wire_names) + " ;")

    for k, (op, a, b) in enumerate(xag.gates):
        aa = _fmt(a, n_pi)
        bb = _fmt(b, n_pi)
        sym = "&" if op == "and" else "^"
        lines.append(f"assign w_{k} = {aa} {sym} {bb} ;")

    for i, lit in enumerate(po_lits):
        rhs = _fmt(lit, n_pi)
        lines.append(f"assign po_{i} = {rhs} ;")

    lines.append("endmodule")
    Path(path).write_text("\n".join(lines) + "\n")


def simulate_xag(xag: XAG, po_lits: List[int], pi_vals: List[int]) -> List[int]:
    assert len(pi_vals) == xag.num_pis
    values = [0] * (1 + xag.num_pis + len(xag.gates))
    values[0] = 0
    for i, v in enumerate(pi_vals):
        values[i + 1] = v & 1
    for k, (op, a, b) in enumerate(xag.gates):
        va = values[a >> 1] ^ (a & 1)
        vb = values[b >> 1] ^ (b & 1)
        if op == "and":
            values[1 + xag.num_pis + k] = va & vb
        else:
            values[1 + xag.num_pis + k] = va ^ vb
    return [values[l >> 1] ^ (l & 1) for l in po_lits]


def run_converter(data: List[int], addr_bits: int, data_bits: int,
                  qspc_path: str, out_path: str,
                  use_select_swap: bool = True) -> dict:
    sys.path.insert(0, str(Path(qspc_path) / "src"))
    from qsp.qrom.synthesis import synthesize_qrom  # noqa: E402

    res = synthesize_qrom(
        data, addr_bits, data_bits,
        use_select_swap=use_select_swap, clifford_t=False,
    )
    xag, pos = qiskit_qrom_to_xag(res.qc, res.input_qubits, res.output_qubits)
    write_verilog(xag, pos, out_path)
    return {
        "num_entries": len(data),
        "addr_bits": addr_bits,
        "data_bits": data_bits,
        "num_pis": xag.num_pis,
        "num_pos": len(pos),
        "and_count": xag.and_count(),
        "xor_count": xag.xor_count(),
        "node_count": xag.node_count(),
        "qiskit_ccx": res.qc.count_ops().get("ccx", 0),
        "qiskit_cx": res.qc.count_ops().get("cx", 0),
        "qiskit_x": res.qc.count_ops().get("x", 0),
    }


if __name__ == "__main__":
    import argparse
    import json

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
