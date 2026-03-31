from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

import numpy as np

_project_root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_project_root / "third-party" / "approx_qlut_simulation"))
sys.path.insert(0, str(_project_root / "third-party" / "qlut-benchmarks" / "src"))

from decode import (
    compute_tt_bitsizes,
    decode_per_register,
    decode_to_integers,
    read_tt_file,
    reconstruct_hamiltonian_from_qrom,
)


def _import_ccsd_t():
    import ccsd_t
    return ccsd_t


def _patch_jax_config():
    """Patch jax.config for openfermion compatibility with JAX >= 0.4.31."""
    import types
    if "jax.config" not in sys.modules:
        import jax
        config_module = types.ModuleType("jax.config")
        config_module.config = jax.config
        sys.modules["jax.config"] = config_module


def _import_convert_thc():
    _patch_jax_config()
    import convert_thc
    return convert_thc


def _thc_tensors_to_hamiltonian(
    h1e: np.ndarray,
    thc_chi: np.ndarray,
    thc_zeta: np.ndarray,
    nuclear_repulsion: float = 0.0,
) -> dict[str, Any]:
    """Reconstruct Hamiltonian from THC tensors (no openfermion dependency)."""
    h1e = np.asarray(h1e, dtype=np.float64)
    thc_chi = np.asarray(thc_chi, dtype=np.float64)
    thc_zeta = np.asarray(thc_zeta, dtype=np.float64)
    nmo = h1e.shape[0]
    thc_rank = thc_chi.shape[0]

    CprP = np.einsum("Pp,Pr->prP", thc_chi, thc_chi, optimize=True)
    h2e = np.einsum("prP,PQ,qsQ->pqrs", CprP, thc_zeta, CprP, optimize=True)

    return {
        "h1e": h1e,
        "h2e": h2e,
        "nuclear_repulsion": float(nuclear_repulsion),
        "nmo": int(nmo),
        "thc_rank": int(thc_rank),
    }


class _NumpyEncoder(json.JSONEncoder):
    def default(self, obj: Any) -> Any:
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        if isinstance(obj, tuple):
            return list(obj)
        return super().default(obj)


def _save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2, cls=_NumpyEncoder)


def create_problem(system: str, **kwargs: Any) -> dict[str, Any]:
    """Stage 1: Build molecule and compute Hamiltonian."""
    ccsd_t = _import_ccsd_t()
    mol = ccsd_t.get_molecule(system, **kwargs)
    ham = ccsd_t.get_hamiltonian(mol)
    return ham


def _fit_zeta(
    chi: np.ndarray, h2e: np.ndarray,
) -> np.ndarray:
    """Fit zeta given fixed chi using pseudoinverse (no Kronecker product)."""
    nmo = h2e.shape[0]
    thc_rank = chi.shape[0]
    G_mat = np.einsum(
        "Pp,Pr->prP", chi, chi, optimize=True
    ).reshape(nmo * nmo, thc_rank)
    G_pinv = np.linalg.pinv(G_mat)
    h2e_mat = h2e.reshape(nmo * nmo, nmo * nmo)
    return G_pinv @ h2e_mat @ G_pinv.T


def _thc_residual(
    chi: np.ndarray, zeta: np.ndarray, h2e: np.ndarray,
) -> float:
    """Compute THC approximation residual."""
    G = np.einsum("Pp,Pr->prP", chi, chi, optimize=True)
    h2e_approx = np.einsum("prP,PQ,qsQ->pqrs", G, zeta, G, optimize=True)
    return 0.5 * float(np.sum((h2e - h2e_approx) ** 2))


def _thc_via_svd(
    h2e: np.ndarray, thc_rank: int
) -> tuple[np.ndarray, np.ndarray, float]:
    """THC factorization via eigendecomposition + pseudoinverse + optional BFGS.

    Factorizes h2e(p,q,r,s) into chi(P,p) and zeta(P,Q) such that
    h2e(p,q,r,s) ~ sum_{PQ} chi(P,p)*chi(P,r) * zeta(P,Q) * chi(Q,q)*chi(Q,s)

    Uses eigendecomposition for chi, pseudoinverse for zeta.
    For small systems (nmo <= 10), refines with L-BFGS-B.
    """
    nmo = h2e.shape[0]
    h2e_mat = h2e.reshape(nmo * nmo, nmo * nmo)
    h2e_sym = 0.5 * (h2e_mat + h2e_mat.T)

    rank = min(thc_rank, nmo * nmo)
    eigvals, eigvecs = np.linalg.eigh(h2e_sym)
    idx = np.argsort(np.abs(eigvals))[::-1][:rank]
    eigvals_sel = eigvals[idx]
    L = eigvecs[:, idx] * np.sqrt(np.abs(eigvals_sel))[np.newaxis, :]
    L = L.reshape(nmo, nmo, rank)

    chi = np.zeros((thc_rank, nmo))
    for mu in range(rank):
        u_vals, u_vecs = np.linalg.eigh(L[:, :, mu])
        k = np.argmax(np.abs(u_vals))
        if np.abs(u_vals[k]) > 1e-14:
            chi[mu, :] = u_vecs[:, k] * np.sqrt(np.abs(u_vals[k]))
    for mu in range(rank, thc_rank):
        chi[mu, :] = np.random.randn(nmo) * 0.01

    nonzero_rows = np.linalg.norm(chi, axis=1) > 1e-14
    if not np.all(nonzero_rows):
        print(f"  Warning: {np.sum(~nonzero_rows)} zero chi rows, reducing effective rank")
        chi = chi[nonzero_rows]
        thc_rank = chi.shape[0]

    zeta = _fit_zeta(chi, h2e)
    residual = _thc_residual(chi, zeta, h2e)

    if nmo <= 10:
        from scipy.optimize import minimize

        h2e_flat = h2e.ravel()
        n_chi = thc_rank * nmo

        def objective(params: np.ndarray) -> float:
            chi_p = params[:n_chi].reshape(thc_rank, nmo)
            zeta_p = params[n_chi:].reshape(thc_rank, thc_rank)
            G_p = np.einsum("Pp,Pr->prP", chi_p, chi_p, optimize=True)
            h2e_a = np.einsum(
                "prP,PQ,qsQ->pqrs", G_p, zeta_p, G_p, optimize=True
            )
            return 0.5 * np.sum((h2e_a.ravel() - h2e_flat) ** 2)

        x0 = np.concatenate([chi.ravel(), zeta.ravel()])
        result = minimize(objective, x0, method="L-BFGS-B", options={
            "maxiter": 2000, "ftol": 1e-15, "gtol": 1e-10,
        })
        chi = result.x[:n_chi].reshape(thc_rank, nmo)
        zeta = result.x[n_chi:].reshape(thc_rank, thc_rank)
        residual = result.fun

    return chi, zeta, residual


def compute_thc(
    ham: dict[str, Any],
    thc_rank: int,
    mol_name: str,
    output_dir: Path,
) -> dict[str, Any]:
    """Stage 2: Compute THC decomposition of the Hamiltonian."""
    output_dir.mkdir(parents=True, exist_ok=True)
    npz_path = output_dir / f"hamiltonian_{mol_name}.npz"
    np.savez_compressed(npz_path, h1e=ham["h1e"], h2e=ham["h2e"])

    try:
        convert_thc = _import_convert_thc()
        thc_ham = convert_thc.hamiltonian_npz_to_thc(
            npz_path=npz_path,
            thc_rank=thc_rank,
            mol_name=mol_name,
            nuclear_repulsion=ham["nuclear_repulsion"],
            verbose=True,
        )
        convert_thc.save_thc_hamiltonian(
            thc_ham, output_dir / f"thc_{mol_name}_rank{thc_rank}.npz"
        )
    except (ImportError, Exception) as exc:
        print(f"openfermion CP3 unavailable ({exc}), using SVD fallback")
        h2e = np.asarray(ham["h2e"])
        chi, zeta, residual = _thc_via_svd(h2e, thc_rank)
        thc_ham = {
            "h1e": np.asarray(ham["h1e"]),
            "nuclear_repulsion": float(ham["nuclear_repulsion"]),
            "thc_chi": chi,
            "thc_zeta": zeta,
            "nmo": int(ham["nmo"]),
            "thc_rank": thc_rank,
            "thc_residual": float(residual),
        }
        np.savez_compressed(
            output_dir / f"thc_{mol_name}_rank{thc_rank}.npz",
            **{k: v for k, v in thc_ham.items() if isinstance(v, np.ndarray)},
            nuclear_repulsion=np.array(thc_ham["nuclear_repulsion"]),
            nmo=np.array(thc_ham["nmo"]),
            thc_rank=np.array(thc_ham["thc_rank"]),
            thc_residual=np.array(thc_ham["thc_residual"]),
        )
        print(f"THC residual: {residual:.6e}")

    return thc_ham


def extract_qluts(
    thc_ham: dict[str, Any],
    num_bits_state_prep: int,
    eps: float,
    output_dir: Path,
) -> dict[str, Any]:
    """Stage 3: Build PrepareTHC and extract QROM truth tables.

    Follows the pattern from qlut-benchmarks/src/preparethc.py but
    uses real THC tensors instead of random coefficients.
    """
    import attrs
    from qualtran.bloqs.chemistry.thc import PrepareTHC
    from qualtran.bloqs.data_loading.qrom import QROM
    from qualtran.bloqs.data_loading.qrom_base import QROMBase
    from qualtran.bloqs.data_loading.qroam_clean import QROAMClean, QROAMCleanAdjoint
    from qualtran.resource_counting.generalizers import ignore_split_join
    from truthTable import TruthTable

    def _fixed_with_data(self, *data):
        _data = tuple([np.array(d, dtype=int) for d in data])
        new_target = tuple(
            max(t, max(1, int(np.max(d)).bit_length()) if d.size > 0 else t)
            for t, d in zip(self.target_bitsizes, _data)
        )
        obj = self
        if new_target != self.target_bitsizes:
            obj = attrs.evolve(obj, target_bitsizes=new_target)
        return attrs.evolve(obj, data_or_shape=_data)

    QROMBase.with_data = _fixed_with_data

    output_dir.mkdir(parents=True, exist_ok=True)

    chi = np.asarray(thc_ham["thc_chi"])
    zeta = np.asarray(thc_ham["thc_zeta"])
    h1e = np.asarray(thc_ham["h1e"])

    eri_thc = np.einsum(
        "Pp,Pr,Qq,Qs,PQ->prqs", chi, chi, chi, chi, zeta, optimize=True
    )
    tpq_prime = (
        h1e
        - 0.5 * np.einsum("illj->ij", eri_thc, optimize=True)
        + np.einsum("llij->ij", eri_thc, optimize=True)
    )
    t_l_all, t_l_eigvecs_all = np.linalg.eigh(tpq_prime)

    mask = np.abs(t_l_all) > eps
    t_l = t_l_all[mask]
    t_l_eigvecs = t_l_eigvecs_all[:, mask]

    num_mu = zeta.shape[0]
    num_spat = len(t_l)
    eta = chi @ t_l_eigvecs
    triu_rows, triu_cols = np.triu_indices(num_mu)
    flat_data_abs = np.concatenate([
        np.abs(zeta[triu_rows, triu_cols]), np.abs(t_l),
    ])
    flat_data_sum = float(np.sum(flat_data_abs))

    prep = PrepareTHC.from_hamiltonian_coeffs(
        t_l, eta, zeta, num_bits_state_prep=num_bits_state_prep
    )

    graph, _ = prep.call_graph(generalizer=ignore_split_join)
    qrom_types = (QROM, QROAMClean, QROAMCleanAdjoint)
    nodes = [n for n in getattr(graph, "nodes", []) if isinstance(n, qrom_types)]

    tt_files = []
    metadata = {
        "num_bits_state_prep": num_bits_state_prep,
        "eps": eps,
        "thc_rank": int(thc_ham["thc_rank"]),
        "nmo": int(thc_ham["nmo"]),
        "nodes": [],
    }

    for idx, node in enumerate(nodes):
        cls_name = type(node).__name__
        filename = f"qlut_{cls_name}_{idx}.tt"
        path = output_dir / filename

        tt = TruthTable.from_qrom_bloq(node, bitorder="msb")
        tt.to_file(str(path))
        tt_files.append(str(path))

        node_data = []
        for arr in getattr(node, "data", ()):
            node_data.append(np.asarray(arr).tolist())

        tt_bitsizes = compute_tt_bitsizes(node_data)

        metadata["nodes"].append({
            "filename": filename,
            "node_class": cls_name,
            "node_index": idx,
            "node_name": getattr(node, "name", cls_name),
            "num_inputs": tt.num_inputs,
            "num_outputs": tt.num_outputs,
            "selection_bitsizes": list(getattr(node, "selection_bitsizes", ())),
            "target_bitsizes": list(getattr(node, "target_bitsizes", ())),
            "tt_bitsizes": tt_bitsizes,
            "data_shapes": [
                list(np.asarray(a).shape) for a in getattr(node, "data", ())
            ],
            "original_data": node_data,
        })

    metadata["flat_data_sum"] = flat_data_sum
    metadata["num_spat"] = num_spat
    _save_json(output_dir / "extraction_metadata.json", metadata)

    np.savez_compressed(
        output_dir / "extraction_extra.npz",
        t_l_eigvecs=t_l_eigvecs,
    )

    return {
        "tt_files": tt_files,
        "metadata": metadata,
        "t_l": t_l,
        "t_l_eigvecs": t_l_eigvecs,
        "eta": eta,
    }


def approximate_qluts(
    tt_dir: Path,
    error_bound: float,
    approx_tt_binary: str | Path,
    time_limit: float = 60.0,
) -> dict[str, Any]:
    """Stage 4: Approximate each .tt file using the C++ approx-tt tool."""
    approx_dir = tt_dir / "approx"
    approx_dir.mkdir(parents=True, exist_ok=True)

    metadata_path = tt_dir / "extraction_metadata.json"
    node_bitsizes: dict[str, list[int]] = {}
    skip_nodes: set[str] = set()
    if metadata_path.exists():
        with open(metadata_path) as f:
            meta = json.load(f)
        for node in meta.get("nodes", []):
            target_bw = node.get("target_bitsizes", [])
            shapes = node.get("data_shapes", [])
            expanded: list[int] = []
            for r, bw in enumerate(target_bw):
                n_cols = shapes[r][1] if r < len(shapes) and len(shapes[r]) > 1 else 1
                expanded.extend([bw] * n_cols)
            node_bitsizes[node["filename"]] = expanded
            if node.get("node_class") == "QROM":
                skip_nodes.add(node["filename"])

    tt_files = sorted(tt_dir.glob("*.tt"))
    results = []

    for tt_file in tt_files:
        approx_file = approx_dir / tt_file.name

        if tt_file.name in skip_nodes:
            import shutil
            shutil.copy2(tt_file, approx_file)
            results.append({"solved": True, "bits_flipped": 0, "filename": tt_file.name})
            continue

        cmd = [
            str(approx_tt_binary),
            "--input", str(tt_file),
            "--output", str(approx_file),
            "--error-bound", str(error_bound),
            "--time-limit", str(time_limit),
        ]
        bitsizes = node_bitsizes.get(tt_file.name, [])
        if bitsizes:
            cmd.extend(["--registers", ",".join(str(b) for b in bitsizes)])

        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"Warning: approx-tt failed on {tt_file.name}: {proc.stderr}")
            continue

        stats = json.loads(proc.stdout.strip())
        stats["filename"] = tt_file.name
        results.append(stats)

    _save_json(approx_dir / "approx_stats.json", results)
    return {"approx_dir": str(approx_dir), "results": results}


def synthesize_qluts(
    tt_dir: Path,
    synth_tt_binary: str | Path,
    num_random_starts: int = 1,
    seed: int = 0,
) -> dict[str, Any]:
    """Synthesize each .tt file using the C++ synth-tt tool."""
    tt_files = sorted(tt_dir.glob("*.tt"))
    results = []

    for tt_file in tt_files:
        cmd = [
            str(synth_tt_binary),
            "--input", str(tt_file),
            "--num-random-starts", str(num_random_starts),
            "--seed", str(seed),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"Warning: synth-tt failed on {tt_file.name}: {proc.stderr}")
            continue

        stats = json.loads(proc.stdout.strip())
        stats["filename"] = tt_file.name
        results.append(stats)

    total_ands = sum(r.get("and_count", 0) for r in results)
    return {"results": results, "total_and_count": total_ands}


def compute_qrom_errors(
    exact_tt_dir: Path,
    approx_tt_dir: Path,
    extraction_metadata: dict[str, Any],
) -> list[dict[str, Any]]:
    """Compare exact and approximate truth tables per QROM register.

    Decodes both exact and approximate truth tables into per-register
    integer arrays (using target_bitsizes from metadata), then computes
    per-register error statistics.
    """
    register_errors = []

    for node_meta in extraction_metadata.get("nodes", []):
        filename = node_meta["filename"]
        exact_path = exact_tt_dir / filename
        approx_path = approx_tt_dir / filename
        if not exact_path.exists() or not approx_path.exists():
            continue

        exact_patterns = read_tt_file(exact_path)
        approx_patterns = read_tt_file(approx_path)

        tt_bitsizes = node_meta.get("tt_bitsizes")
        target_bitsizes = node_meta.get("target_bitsizes", [])
        data_shapes = node_meta.get("data_shapes", [])
        decode_bitsizes = tt_bitsizes if tt_bitsizes else target_bitsizes

        if not decode_bitsizes or not data_shapes:
            continue

        exact_regs = decode_per_register(
            exact_patterns, decode_bitsizes, data_shapes
        )
        approx_regs = decode_per_register(
            approx_patterns, decode_bitsizes, data_shapes
        )

        node_errors = {"node": filename, "registers": []}
        total_bits_changed = 0

        for reg_idx, (ex, ap) in enumerate(zip(exact_regs, approx_regs)):
            ex_flat = np.asarray(ex).ravel().astype(np.int64)
            ap_flat = np.asarray(ap).ravel().astype(np.int64)
            n = min(len(ex_flat), len(ap_flat))
            diff = ap_flat[:n] - ex_flat[:n]
            changed = int(np.sum(diff != 0))
            total_bits_changed += changed
            reg_bw = target_bitsizes[reg_idx] if reg_idx < len(target_bitsizes) else 1
            max_possible = (1 << reg_bw) - 1

            node_errors["registers"].append({
                "register_index": reg_idx,
                "bitwidth": reg_bw,
                "max_possible_value": max_possible,
                "num_entries": n,
                "num_changed": changed,
                "max_abs_error": int(np.max(np.abs(diff))) if n > 0 else 0,
                "mean_abs_error": float(np.mean(np.abs(diff))) if n > 0 else 0.0,
                "relative_error": (
                    float(np.mean(np.abs(diff)) / max_possible)
                    if max_possible > 0 and n > 0 else 0.0
                ),
            })

        node_errors["total_entries_changed"] = total_bits_changed
        register_errors.append(node_errors)

    return register_errors


def decode_and_reconstruct(
    tt_dir: Path,
    thc_ham: dict[str, Any],
    extraction: dict[str, Any],
) -> dict[str, Any]:
    """Stage 5: Decode approximate QROM truth tables and reconstruct Hamiltonian.

    Finds the forward QROAMClean node (1D data_shapes), decodes its
    approximate truth table, reverses alias sampling and eigendecomposition,
    and reconstructs h1e/h2e for CCSD(T) evaluation.
    """
    metadata = extraction["metadata"]
    t_l_eigvecs = extraction["t_l_eigvecs"]

    forward_node = None
    for node_meta in metadata["nodes"]:
        data_shapes = node_meta["data_shapes"]
        if data_shapes and all(len(s) == 1 for s in data_shapes):
            forward_node = node_meta
            break
    assert forward_node is not None, "No forward QROAMClean node found"

    tt_path = tt_dir / forward_node["filename"]
    patterns = read_tt_file(tt_path)
    tt_bitsizes = forward_node.get("tt_bitsizes", forward_node["target_bitsizes"])
    registers = decode_per_register(
        patterns,
        tt_bitsizes,
        forward_node["data_shapes"],
    )

    ham = reconstruct_hamiltonian_from_qrom(
        registers,
        num_mu=metadata["thc_rank"],
        num_spat=metadata["num_spat"],
        keep_bitsize=metadata["num_bits_state_prep"],
        flat_data_sum=metadata["flat_data_sum"],
        t_l_eigvecs=t_l_eigvecs,
        chi=np.asarray(thc_ham["thc_chi"]),
        nuclear_repulsion=thc_ham.get("nuclear_repulsion", 0.0),
    )
    return ham


def evaluate(ham: dict[str, Any]) -> dict[str, float]:
    """Stage 6: Run CCSD(T) on a Hamiltonian."""
    ccsd_t = _import_ccsd_t()
    result = ccsd_t.run_ccsd_t_from_hamiltonian(ham)
    return {k: float(v) for k, v in result.items()}


def compare_and_report(
    baseline: dict[str, float],
    approx: dict[str, float],
    output_path: Path,
    approx_stats: dict[str, Any] | None = None,
    integer_errors: list[dict] | None = None,
) -> dict[str, Any]:
    """Stage 7: Compare baseline and approximate energies."""
    deltas = {f"delta_{k}": approx[k] - baseline[k] for k in baseline}
    chemical_accuracy_ha = 1.6e-3

    report = {
        "baseline_energies": baseline,
        "approximate_energies": approx,
        "energy_differences": deltas,
        "within_chemical_accuracy": (
            abs(deltas.get("delta_ccsd_t_energy", float("inf")))
            < chemical_accuracy_ha
        ),
        "chemical_accuracy_threshold_ha": chemical_accuracy_ha,
    }
    if approx_stats is not None:
        report["approximation_stats"] = approx_stats
    if integer_errors is not None:
        report["integer_errors"] = integer_errors

    _save_json(output_path, report)
    return report


def run_pipeline(
    system: str,
    thc_rank: int,
    num_bits_state_prep: int,
    error_bound: float,
    output_dir: Path,
    eps: float = 1e-2,
    approx_tt_binary: str | Path | None = None,
    time_limit: float = 60.0,
    **system_kwargs: Any,
) -> dict[str, Any]:
    """Run the full approximate QLUT evaluation pipeline."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if approx_tt_binary is None:
        approx_tt_binary = _project_root / "build" / "approx-tt"

    print("Stage 1: Building molecule and Hamiltonian...")
    ham = create_problem(system, **system_kwargs)

    print("Stage 2: Computing THC decomposition...")
    thc_ham = compute_thc(ham, thc_rank, system, output_dir)
    thc_ham["nocc"] = ham["nocc"]

    print("Stage 3: Extracting QLUTs from PrepareTHC...")
    tt_dir = output_dir / "truth_tables"
    extraction = extract_qluts(thc_ham, num_bits_state_prep, eps, tt_dir)

    print("Stage 4: Approximating truth tables...")
    approx_result = approximate_qluts(
        tt_dir, error_bound, approx_tt_binary, time_limit
    )
    approx_dir = Path(approx_result["approx_dir"])

    print("Stage 5: Computing QROM per-register errors...")
    qrom_errors = compute_qrom_errors(
        tt_dir, approx_dir, extraction["metadata"]
    )
    for node_err in qrom_errors:
        for reg in node_err["registers"]:
            if reg["num_changed"] > 0:
                print(
                    f"  {node_err['node']} reg{reg['register_index']} "
                    f"({reg['bitwidth']}b): {reg['num_changed']}/{reg['num_entries']} "
                    f"changed, max_err={reg['max_abs_error']}/{reg['max_possible_value']}, "
                    f"rel_err={reg['relative_error']:.4f}"
                )

    print("Stage 6a: Evaluating baseline CCSD(T) from original THC...")
    baseline_ham = _thc_tensors_to_hamiltonian(
        h1e=np.asarray(thc_ham["h1e"]),
        thc_chi=np.asarray(thc_ham["thc_chi"]),
        thc_zeta=np.asarray(thc_ham["thc_zeta"]),
        nuclear_repulsion=thc_ham.get("nuclear_repulsion", 0.0),
    )
    baseline_ham["nocc"] = ham["nocc"]
    baseline_energy = evaluate(baseline_ham)

    print("Stage 6b: Decoding approximate QROM and evaluating CCSD(T)...")
    approx_ham = decode_and_reconstruct(approx_dir, thc_ham, extraction)
    approx_ham["nocc"] = ham["nocc"]
    approx_energy = evaluate(approx_ham)

    print("Stage 7: Comparing results...")
    report = compare_and_report(
        baseline_energy,
        approx_energy,
        output_dir / "comparison.json",
        approx_result,
        qrom_errors,
    )

    print(f"\nTHC baseline CCSD(T): {baseline_energy['ccsd_t_energy']:.10f} Ha")
    print(f"Approx CCSD(T):      {approx_energy['ccsd_t_energy']:.10f} Ha")
    delta = approx_energy['ccsd_t_energy'] - baseline_energy['ccsd_t_energy']
    print(f"Energy delta:        {delta:.10f} Ha")
    for node_err in qrom_errors:
        n_name = node_err["node"]
        total_changed = node_err["total_entries_changed"]
        print(f"  {n_name}: {total_changed} register entries changed by approximation")

    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Approximate QLUT evaluation pipeline"
    )
    parser.add_argument("--system", default="H4", help="Molecule name")
    parser.add_argument("--thc-rank", type=int, default=10, help="THC rank")
    parser.add_argument(
        "--num-bits", type=int, default=10, help="State preparation bits"
    )
    parser.add_argument(
        "--error-bound", type=float, default=1.0, help="Error bound"
    )
    parser.add_argument(
        "--eps", type=float, default=1e-2, help="Epsilon threshold"
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("results"), help="Output dir"
    )
    parser.add_argument(
        "--approx-tt", type=Path, help="Path to approx-tt binary"
    )
    parser.add_argument(
        "--time-limit", type=float, default=60.0, help="ILP time limit (sec)"
    )
    parser.add_argument(
        "--nh", type=int, help="Number of hydrogens (for H_chain)"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    system_kwargs: dict[str, Any] = {}
    if args.nh is not None:
        system_kwargs["nh"] = args.nh

    run_pipeline(
        system=args.system,
        thc_rank=args.thc_rank,
        num_bits_state_prep=args.num_bits,
        error_bound=args.error_bound,
        output_dir=args.output_dir,
        eps=args.eps,
        approx_tt_binary=args.approx_tt,
        time_limit=args.time_limit,
        **system_kwargs,
    )


if __name__ == "__main__":
    main()
