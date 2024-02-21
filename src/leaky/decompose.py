"""Decompose Kraus operators with generalized pauli twirling(GPC)."""
from typing import Sequence
import itertools
import functools

import numpy as np

from leaky.transition import Transition, LeakageStatus


PAULIS = [
    np.array([[1, 0], [0, 1]], dtype=complex),
    np.array([[0, 1], [1, 0]], dtype=complex),
    np.array([[0, -1j], [1j, 0]], dtype=complex),
    np.array([[1, 0], [0, -1]], dtype=complex),
]


def get_projector_slice(
    num_level: int,
    projector_status: LeakageStatus,
) -> list[int]:
    assert all(s < num_level - 1 for s in projector_status)
    num_qubits = len(projector_status)
    status = projector_status[0]
    if num_qubits == 1:
        return [0, 1] if status == 0 else [status + 1]
    tail_slice = get_projector_slice(num_level, projector_status[1:])
    offset = (status + 1) * num_level ** (num_qubits - 1)
    if status == 0:
        return [*tail_slice, *map(lambda x: x + offset, tail_slice)]
    return [x + offset for x in tail_slice]


def _project_kraus_with_initial_final(
    kraus: np.ndarray,
    num_qubits: int,
    num_level: int,
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> np.ndarray:
    assert kraus.shape[0] == num_level**num_qubits
    initial_slice = get_projector_slice(num_level, initial_status)
    final_slice = get_projector_slice(num_level, final_status)
    return kraus[final_slice, :][:, initial_slice]


def _get_qubits_stayed_in_computational_space(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> list[int]:
    return [i for i, (s0, s1) in enumerate(zip(initial_status, final_status)) if s0 == s1 == 0]


def _get_num_newly_leaked_qubits(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> int:
    return sum(s0 == 0 and s1 > 0 for s0, s1 in zip(initial_status, final_status))


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


def project_kraus_operators(
    kraus_operators: Sequence[np.ndarray], num_qubits: int, num_level: int
) -> dict[LeakageStatus, list[Transition]]:
    all_status = list(itertools.product(range(num_level - 1), repeat=num_qubits))
    record: dict[LeakageStatus, dict[tuple[LeakageStatus, int | None], float]] = dict()
    for kraus in kraus_operators:
        for initial_status, final_status in itertools.product(all_status, repeat=2):
            num_newly_leaked = _get_num_newly_leaked_qubits(initial_status, final_status)
            prefactor: float = 1.0 / 2**num_newly_leaked
            projected_kraus = _project_kraus_with_initial_final(
                kraus, num_qubits, num_level, initial_status, final_status
            )
            qubits_stay = _get_qubits_stayed_in_computational_space(initial_status, final_status)
            num_qubits_stay = len(qubits_stay)
            probability: float
            pauli_channel: list[float] | None = None
            if not qubits_stay:
                probability = prefactor * np.sum(np.abs(projected_kraus) ** 2)
            else:
                dim = 2**num_qubits_stay
                pauli_channel = [
                    np.abs(np.trace(projected_kraus @ functools.reduce(np.kron, paulis)) / dim) ** 2
                    for paulis in itertools.product(PAULIS, repeat=len(qubits_stay))
                ]
                probability = prefactor * sum(pauli_channel)
            if probability < 1e-9:
                continue
            _add_transition(record, initial_status, final_status, probability, pauli_channel)

    transitions = {
        initial_status: [Transition(p, final_status, i) for (final_status, i), p in trans_dict.items()]
        for initial_status, trans_dict in record.items()
    }
    return transitions
