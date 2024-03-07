"""Decompose Kraus operators with generalized pauli twirling(GPC)."""
from typing import Sequence
import itertools
import functools

import numpy as np

from leaky.transition import Transition, LeakageStatus, TransitionTable


ProjectStatus = tuple[tuple[int], ...]


PAULIS = np.array(
    [
        np.array([[1, 0], [0, 1]], dtype=complex),
        np.array([[0, 1], [1, 0]], dtype=complex),
        np.array([[0, -1j], [1j, 0]], dtype=complex),
        np.array([[1, 0], [0, -1]], dtype=complex),
    ]
)

TWO_QUBITS_PAULIS = np.array([np.kron(p1, p2) for p1, p2 in itertools.product(PAULIS, repeat=2)])


def l2p(leakage_status: LeakageStatus) -> ProjectStatus:
    return tuple((0, 1) if s == 0 else (s + 1,) for s in leakage_status)


def get_projector_slice(
    num_level: int,
    project_status: ProjectStatus,
) -> list[int]:
    """Get slice into the matrix for the subspace projection defined by project_status."""
    num_qubits = len(project_status)
    status = project_status[0]
    if num_qubits == 1:
        return list(status)
    tail_slice = get_projector_slice(num_level, project_status[1:])
    return [x + s * num_level ** (num_qubits - 1) for s in status for x in tail_slice]


def _project_kraus_with_initial_final(
    kraus: np.ndarray,
    num_qubits: int,
    num_level: int,
    initial_project_status: ProjectStatus,
    final_project_status: ProjectStatus,
) -> np.ndarray:
    assert kraus.shape[0] == num_level**num_qubits
    initial_slice = get_projector_slice(num_level, initial_project_status)
    final_slice = get_projector_slice(num_level, final_project_status)
    return kraus[final_slice, :][:, initial_slice]


def _get_num_qubits_in_space_r(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> int:
    return sum(s0 == 0 and s1 == 0 for s0, s1 in zip(initial_status, final_status))


def _get_num_qubits_in_space_u(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> int:
    return sum(s0 == 0 and s1 > 0 for s0, s1 in zip(initial_status, final_status))


def _scatter_status(initial: LeakageStatus, final: LeakageStatus) -> list[tuple[ProjectStatus, ProjectStatus]]:
    initial_project_status = l2p(initial)
    final_project_status = l2p(final)

    up_indices = [i for i, (start, end) in enumerate(zip(initial, final)) if start == 0 and end > 0]
    down_indices = [i for i, (start, end) in enumerate(zip(initial, final)) if start > 0 and end == 0]

    initial_combinations = list(
        itertools.product(
            *[
                [status] if index not in up_indices else [(0,), (1,)]
                for index, status in enumerate(initial_project_status)
            ]
        )
    )
    final_combinations = list(
        itertools.product(
            *[
                [status] if index not in down_indices else [(0,), (1,)]
                for index, status in enumerate(final_project_status)
            ]
        )
    )
    return list(itertools.product(initial_combinations, final_combinations))


def _add_transition(
    record: dict[LeakageStatus, dict[tuple[LeakageStatus, int | None], float]],
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
    probability: float,
    pauli_channel: list[float] | None = None,
) -> None:
    transitions = []
    probs = []
    if pauli_channel is None:
        transitions.append((final_status, None))
        probs.append(probability)
    else:
        for i, p in enumerate(pauli_channel):
            if p < 1e-9:
                continue
            transitions.append((final_status, i))
            probs.append(p)
    for transition, prob in zip(transitions, probs):
        prev_prob = record.setdefault(initial_status, dict()).get(transition, 0.0)
        record[initial_status][transition] = prev_prob + prob


def decompose_kraus_operators(
    kraus_operators: Sequence[np.ndarray], num_qubits: int, num_level: int
) -> TransitionTable:
    """Decompose the Kraus operators into a transition table.

    Args:
        kraus_operators: A sequence of Kraus operators corresponding to an operation's error channel.
        num_qubits: The number of qubits in the operation.
        num_level: The number of levels of the quantum system to be considered.
    """
    all_status = list(itertools.product(range(num_level - 1), repeat=num_qubits))
    record: dict[LeakageStatus, dict[tuple[LeakageStatus, int | None], float]] = dict()
    for kraus in kraus_operators:
        for initial_status, final_status in itertools.product(all_status, repeat=2):
            num_u = _get_num_qubits_in_space_u(initial_status, final_status)
            num_r = _get_num_qubits_in_space_r(initial_status, final_status)

            prefactor: float = 1.0 / 2**num_u
            projectors = _scatter_status(initial_status, final_status)
            for initial_projector, final_projector in projectors:
                projected_kraus = _project_kraus_with_initial_final(
                    kraus, num_qubits, num_level, initial_projector, final_projector
                )
                probability: float
                pauli_channel: list[float] | None = None
                if num_r == 0:
                    assert projected_kraus.shape == (1, 1)
                    probability = (prefactor * np.abs(projected_kraus).astype(float) ** 2).item()
                else:
                    dim = 2**num_r
                    assert projected_kraus.shape == (dim, dim)
                    pauli_channel = [
                        prefactor * np.abs(np.trace(projected_kraus @ functools.reduce(np.kron, paulis)) / dim) ** 2
                        for paulis in itertools.product(PAULIS, repeat=num_r)
                    ]
                    probability = sum(pauli_channel)
                if probability < 1e-9:
                    continue
                _add_transition(record, initial_status, final_status, probability, pauli_channel)

    transitions = {
        initial_status: [Transition(initial_status, final_status, p, i) for (final_status, i), p in trans_dict.items()]
        for initial_status, trans_dict in record.items()
    }
    return TransitionTable(transitions)
