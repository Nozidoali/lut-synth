"""Alias-sampling state-prep infidelity evaluator.

Takes an amplitude distribution and two approximated LUTs (keep + alias)
and computes the marginal probability distribution that alias sampling
would prepare, then returns Bhattacharyya-based infidelity vs the target.

Fidelity definition:
    F = (sum_k sqrt(P_target(k) * P_prep(k)))^2
    infidelity = 1 - F

This matches what the address register sees after tracing out ancillae.
"""
from __future__ import annotations

import json
import math
import re
import sys
from collections import deque
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def amplitudes_to_probs(amps: Dict[int, float], n_bits: int) -> List[float]:
    dim = 1 << n_bits
    probs = [0.0] * dim
    for k, a in amps.items():
        probs[int(k)] = float(a) * float(a)
    s = sum(probs)
    if s <= 0:
        raise ValueError("zero-probability state")
    return [p / s for p in probs]


def build_alias_table(probs: List[float], precision_bits: int
                      ) -> Tuple[List[int], List[int]]:
    N = len(probs)
    max_val = (1 << precision_bits) - 1
    scaled = [p * N for p in probs]
    small: deque = deque()
    large: deque = deque()
    for i in range(N):
        (small if scaled[i] < 1.0 else large).append(i)
    keep = [1.0] * N
    alias = [0] * N
    while small and large:
        s = small.popleft()
        l = large.popleft()
        keep[s] = scaled[s]
        alias[s] = l
        scaled[l] = (scaled[l] + scaled[s]) - 1.0
        (small if scaled[l] < 1.0 else large).append(l)
    while large:
        keep[large.popleft()] = 1.0
    while small:
        keep[small.popleft()] = 1.0
    keep_int = [int(round(max(0.0, min(1.0, k)) * max_val)) for k in keep]
    return keep_int, alias


def prepared_probs(keep_vals: List[int], alias_idx: List[int],
                   precision_bits: int) -> List[float]:
    N = len(keep_vals)
    denom = float(1 << precision_bits)
    contrib = [0.0] * N
    for j in range(N):
        kj = keep_vals[j] / denom
        contrib[j] += kj / N
        contrib[alias_idx[j]] += (1.0 - kj) / N
    return contrib


def bhattacharyya_fidelity(p: List[float], q: List[float]) -> float:
    acc = 0.0
    for a, b in zip(p, q):
        if a > 0 and b > 0:
            acc += math.sqrt(a * b)
    return acc * acc


def total_variation(p: List[float], q: List[float]) -> float:
    """L1/2 distance; matches the PREPARE-oracle coefficient-error
    metric used in the Babbush/Lee THC qubitization analyses.
    energy-error bound: |E - E_approx| <= 2 * lambda * TV."""
    return 0.5 * sum(abs(a - b) for a, b in zip(p, q))


_RE_MODULE = re.compile(r"module\s+\w+\s*\(([^)]*)\)\s*;")
_RE_ASSIGN = re.compile(r"assign\s+(\S+)\s*=\s*(.+?)\s*;")
_RE_INPUT = re.compile(r"input\s+([^;]+);")
_RE_OUTPUT = re.compile(r"output\s+([^;]+);")


def _parse_operand(tok: str) -> Tuple[str, bool]:
    tok = tok.strip().strip("()")
    if tok.startswith("~"):
        return tok[1:].strip(), True
    return tok, False


def load_xag_verilog(path: str):
    """Parse a flat mockturtle-style Verilog XAG and return a simulator.

    Returns
    -------
    (pi_names, po_names, evaluate)
        `evaluate(addr_bits_list)` returns a list of PO values for a given
        assignment of PIs (list of 0/1 matching pi_names order).
    """
    text = Path(path).read_text()
    in_match = _RE_INPUT.search(text)
    out_match = _RE_OUTPUT.search(text)
    if not in_match or not out_match:
        raise ValueError("missing input/output in Verilog")

    pi_names = [s.strip() for s in in_match.group(1).split(",")]
    po_names = [s.strip() for s in out_match.group(1).split(",")]

    assigns: List[Tuple[str, str]] = []
    for m in _RE_ASSIGN.finditer(text):
        assigns.append((m.group(1), m.group(2)))

    op_re = re.compile(
        r"^\s*(~)?\s*([\w\[\]']+)\s*([&|^])\s*(~)?\s*([\w\[\]']+)\s*$"
    )
    ops: List[Tuple[str, object]] = []
    for lhs, rhs in assigns:
        m = op_re.match(rhs)
        if m:
            inv_a = bool(m.group(1))
            a = m.group(2)
            sym = m.group(3)
            inv_b = bool(m.group(4))
            b = m.group(5)
            ops.append((lhs, (sym, a, inv_a, b, inv_b)))
        else:
            a, inv = _parse_operand(rhs)
            ops.append((lhs, ("id", a, inv)))

    def _val(name: str, signals: Dict[str, int]) -> int:
        if name in ("1'b0", "0"):
            return 0
        if name in ("1'b1", "1"):
            return 1
        return signals[name]

    def evaluate(pi_vals: List[int]) -> List[int]:
        signals: Dict[str, int] = {}
        for i, name in enumerate(pi_names):
            signals[name] = pi_vals[i] & 1
        for lhs, op in ops:
            if op[0] == "id":
                _, a, inv = op
                v = _val(a, signals) ^ (1 if inv else 0)
            else:
                sym, a, ia, b, ib = op
                va = _val(a, signals) ^ (1 if ia else 0)
                vb = _val(b, signals) ^ (1 if ib else 0)
                if sym == "&":
                    v = va & vb
                elif sym == "|":
                    v = va | vb
                elif sym == "^":
                    v = va ^ vb
                else:
                    raise ValueError(f"bad op: {sym}")
            signals[lhs] = v
        return [signals[p] for p in po_names]

    return pi_names, po_names, evaluate


def simulate_qrom_verilog(path: str, addr_bits: int, data_bits: int,
                          N: int) -> List[int]:
    pi_names, po_names, evaluate = load_xag_verilog(path)
    assert len(pi_names) == addr_bits
    assert len(po_names) == data_bits
    out = []
    for addr in range(N):
        pi = [(addr >> i) & 1 for i in range(addr_bits)]
        bits = evaluate(pi)
        out.append(sum(bits[i] << i for i in range(data_bits)))
    return out


def eval_infidelity(amps: Dict[int, float], n_bits: int, precision_bits: int,
                    keep_vals: List[int], alias_idx: List[int]) -> Dict:
    p_target = amplitudes_to_probs(amps, n_bits)
    p_prep = prepared_probs(keep_vals, alias_idx, precision_bits)
    F = bhattacharyya_fidelity(p_target, p_prep)
    return {
        "fidelity": F,
        "infidelity": 1.0 - F,
        "total_prep_prob": sum(p_prep),
    }


def eval_from_verilog(amps_path: str, n_bits: int, precision_bits: int,
                      keep_verilog: Optional[str] = None,
                      alias_verilog: Optional[str] = None) -> Dict:
    amps = {int(k): float(v)
            for k, v in json.loads(Path(amps_path).read_text()).get(
                "amplitudes", {}).items()}
    N = 1 << n_bits
    p_target = amplitudes_to_probs(amps, n_bits)
    keep_exact, alias_exact = build_alias_table(p_target, precision_bits)
    keep_vals = (simulate_qrom_verilog(keep_verilog, n_bits, precision_bits, N)
                 if keep_verilog else keep_exact)
    alias_vals = (simulate_qrom_verilog(alias_verilog, n_bits, n_bits, N)
                  if alias_verilog else alias_exact)
    res = eval_infidelity(amps, n_bits, precision_bits, keep_vals, alias_vals)
    res["N"] = N
    res["precision_bits"] = precision_bits
    return res


if __name__ == "__main__":
    import argparse
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
