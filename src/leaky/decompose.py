"""Decompose Kraus operators with generalized pauli twirling(GPC)."""
import dataclasses
import itertools
import functools

import numpy as np


PAULIS = [
    np.array([[1, 0], [0, 1]], dtype=complex),
    np.array([[0, 1], [1, 0]], dtype=complex),
    np.array([[0, -1j], [1j, 0]], dtype=complex),
    np.array([[1, 0], [0, -1]], dtype=complex),
]


LeakageStatus = tuple[int, ...]


@dataclasses.dataclass(frozen=True)
class Transition:
    initial_status: LeakageStatus
    final_status: LeakageStatus
    probability: float
    pauli_channel: list[float] = dataclasses.field(default_factory=list)


@dataclasses.dataclass(frozen=True)
class TransitionTable:
    num_qubits: int
    num_level: int
    transitions: dict[tuple[LeakageStatus, LeakageStatus], Transition]

    def get(
        self,
        initial_status: LeakageStatus,
        final_status: LeakageStatus,
        pauli_index: int = None,
    ) -> float:
        transition = self.transitions[initial_status, final_status]
        return transition.probability if pauli_index is None else transition.pauli_channel[pauli_index]


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


def project_kraus_with_initial_final(
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


def get_qubits_stayed_in_computational_space(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> list[int]:
    return [i for i, (s0, s1) in enumerate(zip(initial_status, final_status)) if s0 == s1 == 0]


def get_num_newly_leaked_qubits(
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
) -> int:
    return sum(s0 == 0 and s1 > 0 for s0, s1 in zip(initial_status, final_status))


def project_kraus(kraus: np.ndarray, num_qubits: int, num_level: int) -> TransitionTable:
    all_status = list(itertools.product(range(num_level - 1), repeat=num_qubits))
    transitions = dict()
    for initial_status, final_status in itertools.product(all_status, repeat=2):
        num_newly_leaked = get_num_newly_leaked_qubits(initial_status, final_status)
        prefactor: float = 1.0 / 2**num_newly_leaked
        projected_kraus = project_kraus_with_initial_final(kraus, num_qubits, num_level, initial_status, final_status)
        qubits_stay = get_qubits_stayed_in_computational_space(initial_status, final_status)
        num_qubits_stay = len(qubits_stay)
        probability: float
        pauli_channel: list[float] = []
        if not qubits_stay:
            probability = prefactor * np.sum(np.abs(projected_kraus) ** 2)
        else:
            dim = 2 ** num_qubits_stay
            pauli_channel = [
                np.abs(np.trace(projected_kraus @ functools.reduce(np.kron, paulis)) / dim) ** 2 
                for paulis in itertools.product(PAULIS, repeat=len(qubits_stay))
            ]
            probability = prefactor * sum(pauli_channel)
        transitions[(initial_status, final_status)] = Transition(
            initial_status, final_status, probability, pauli_channel
        )
    return TransitionTable(num_qubits, num_level, transitions)
