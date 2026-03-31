from __future__ import annotations

from pathlib import Path

import numpy as np


def read_tt_file(path: str | Path) -> list[str]:
    """Read a .tt file and return one binary string per output bit."""
    with open(path) as f:
        return [line.strip() for line in f if line.strip()]


def decode_to_integers(
    patterns: list[str], n_values: int | None = None
) -> np.ndarray:
    """Decode MSB-first binary patterns to integer values.

    patterns[0] is MSB, patterns[-1] is LSB.
    For each input x in [0, n_values):
        value(x) = sum_{i=0}^{m-1} bit(patterns[i], x) * 2^{m-1-i}
    """
    m = len(patterns)
    if n_values is None:
        n_values = len(patterns[0])
    if m > 63:
        values = [0] * n_values
        for i, pattern in enumerate(patterns):
            bit_weight = 1 << (m - 1 - i)
            for x in range(n_values):
                if pattern[x] == "1":
                    values[x] += bit_weight
        return np.array(values, dtype=object)
    values = np.zeros(n_values, dtype=np.int64)
    for i, pattern in enumerate(patterns):
        bit_weight = 1 << (m - 1 - i)
        for x in range(n_values):
            if pattern[x] == "1":
                values[x] += bit_weight
    return values


def decode_per_register(
    patterns: list[str],
    target_bitsizes: list[int],
    data_shapes: list[list[int]],
    n_values: int | None = None,
) -> list[np.ndarray]:
    """Decode truth table patterns into per-register integer arrays.

    The truth table output bits are ordered by register, with each register
    using target_bitsizes[r] bits. For multi-column registers (2D data),
    each column is encoded separately with its own bitwidth.

    Returns one integer array per original data array, reshaped to match
    the original data_shapes.
    """
    if n_values is None:
        n_values = len(patterns[0])

    bit_offset = 0
    register_values = []

    for reg_idx, shape in enumerate(data_shapes):
        shape_list = list(shape)
        if len(shape_list) == 1:
            n_entries = shape_list[0]
            n_cols = 1
        else:
            n_entries = shape_list[0]
            n_cols = shape_list[1]

        reg_data = np.zeros((n_entries, n_cols), dtype=np.int64)
        for col in range(n_cols):
            bw = target_bitsizes[reg_idx] if reg_idx < len(target_bitsizes) else 1
            col_bits = patterns[bit_offset : bit_offset + bw]
            col_ints = decode_to_integers(col_bits, n_values)
            reg_data[:min(n_entries, n_values), col] = col_ints[:n_entries]
            bit_offset += bw

        if n_cols == 1:
            register_values.append(reg_data.ravel())
        else:
            register_values.append(reg_data)

    return register_values


def compute_tt_bitsizes(original_data: list) -> list[int]:
    """Compute actual bitwidths used by TruthTable.from_qrom_bloq.

    The truth table encodes each data array using the minimum bits
    needed to represent its maximum value, not the target_bitsizes.
    This function replicates that logic so decoding uses matching widths.
    """
    bitsizes = []
    for arr_data in original_data:
        arr = np.asarray(arr_data)
        if arr.ndim <= 1 or (arr.ndim == 2 and arr.shape[1] == 1):
            vmax = int(arr.max()) if arr.size > 0 else 0
            bitsizes.append(max(1, vmax.bit_length()))
        elif arr.ndim == 2 and int(arr.max()) <= 1:
            bitsizes.append(arr.shape[1])
        else:
            total_bw = 0
            for j in range(arr.shape[1]):
                vmax = int(arr[:, j].max()) if arr.shape[0] > 0 else 0
                total_bw += max(1, vmax.bit_length())
            bitsizes.append(total_bw)
    return bitsizes


def dequantize(
    int_values: np.ndarray,
    value_range: tuple[float, float],
    n_bits: int,
) -> np.ndarray:
    """Reverse fixed-point quantization: map integers back to floating-point.

    Quantization maps [min_val, max_val] to [0, 2^n_bits - 1].
    Dequantization: float = min_val + int_val * (max_val - min_val) / (2^n_bits - 1)
    """
    min_val, max_val = value_range
    max_int = (1 << n_bits) - 1
    return min_val + int_values.astype(np.float64) * (max_val - min_val) / max_int


def reverse_alias_sampling(
    theta: np.ndarray,
    alt_mu: np.ndarray,
    alt_nu: np.ndarray,
    keep: np.ndarray,
    num_mu: int,
    num_spat: int,
    keep_bitsize: int,
    flat_data_sum: float,
) -> np.ndarray:
    """Reverse PrepareTHC alias sampling to recover signed coefficients.

    Decodes the QROM registers (theta, alt_mu, alt_nu, keep) back into
    the flat_data array of signed coefficients [zeta_triu | t_l].
    """
    num_ut = num_mu * (num_mu + 1) // 2
    n = num_ut + num_spat
    keep_denom = 1 << keep_bitsize

    alt_flat = np.zeros(n, dtype=np.int64)
    for s in range(n):
        mu_s = int(alt_mu[s])
        nu_s = int(alt_nu[s])
        if nu_s >= num_mu:
            alt_flat[s] = num_ut + mu_s
        else:
            i = min(mu_s, nu_s)
            j = max(mu_s, nu_s)
            alt_flat[s] = i * num_mu - i * (i - 1) // 2 + (j - i)

    p_eff = np.zeros(n, dtype=np.float64)
    for s in range(n):
        k = int(keep[s])
        p_eff[s] += k / (n * keep_denom)
        p_eff[alt_flat[s]] += (keep_denom - k) / (n * keep_denom)

    magnitudes = p_eff * flat_data_sum
    signs = np.where(theta.astype(int) == 0, 1.0, -1.0)
    return signs * magnitudes


def unflatten_to_tensors(
    flat_data_signed: np.ndarray,
    num_mu: int,
    num_spat: int,
) -> tuple[np.ndarray, np.ndarray]:
    """Split signed coefficient array into (zeta_approx, t_l_approx).

    Reverses the flattening: flat_data = [zeta[triu_indices] | t_l].
    Returns symmetric zeta matrix and t_l eigenvalue array.
    """
    num_ut = num_mu * (num_mu + 1) // 2
    zeta_flat = flat_data_signed[:num_ut]
    t_l_approx = flat_data_signed[num_ut:num_ut + num_spat]

    zeta_approx = np.zeros((num_mu, num_mu), dtype=np.float64)
    rows, cols = np.triu_indices(num_mu)
    zeta_approx[rows, cols] = zeta_flat
    zeta_approx[cols, rows] = zeta_flat

    return zeta_approx, t_l_approx


def reconstruct_hamiltonian_from_qrom(
    approx_registers: list[np.ndarray],
    num_mu: int,
    num_spat: int,
    keep_bitsize: int,
    flat_data_sum: float,
    t_l_eigvecs: np.ndarray,
    chi: np.ndarray,
    nuclear_repulsion: float,
) -> dict:
    """Reconstruct Hamiltonian from decoded QROM registers.

    Orchestrates: reverse alias sampling, unflatten to tensors,
    reverse eigendecomposition, and THC tensor contraction.
    """
    theta, alt_theta, alt_mu, alt_nu, keep_vals = approx_registers

    flat_signed = reverse_alias_sampling(
        theta, alt_mu, alt_nu, keep_vals,
        num_mu, num_spat, keep_bitsize, flat_data_sum,
    )

    zeta_approx, t_l_approx = unflatten_to_tensors(
        flat_signed, num_mu, num_spat,
    )

    tpq_prime_approx = t_l_eigvecs @ np.diag(t_l_approx) @ t_l_eigvecs.T

    eri_approx = np.einsum(
        "Pp,Pr,Qq,Qs,PQ->prqs", chi, chi, chi, chi, zeta_approx, optimize=True
    )

    h1e_approx = (
        tpq_prime_approx
        + 0.5 * np.einsum("illj->ij", eri_approx, optimize=True)
        - np.einsum("llij->ij", eri_approx, optimize=True)
    )

    CprP = np.einsum("Pp,Pr->prP", chi, chi, optimize=True)
    h2e_approx = np.einsum(
        "prP,PQ,qsQ->pqrs", CprP, zeta_approx, CprP, optimize=True
    )

    return {
        "h1e": h1e_approx,
        "h2e": h2e_approx,
        "nuclear_repulsion": float(nuclear_repulsion),
        "nmo": int(h1e_approx.shape[0]),
        "thc_rank": int(chi.shape[0]),
    }
