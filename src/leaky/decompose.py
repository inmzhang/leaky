"""Decompose Kraus operators with generalized pauli twirling(GPC)."""
from typing import Sequence
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
    final_status: LeakageStatus
    pauli_channel_idx: int | None = None


@dataclasses.dataclass(frozen=True)
class TransitionTable:
    transitions: dict[LeakageStatus, dict[Transition, float]]
    probabilities: dict[LeakageStatus, list[float]]
    rng: np.random.RandomState

    @classmethod
    def from_record(
        cls,
        record: dict[LeakageStatus, dict[Transition, float]],
        random_state: np.random.RandomState | None,
    ) -> "TransitionTable":
        return cls(
            transitions={status: list(m.keys()) for status, m in record.items()},
            probabilities={status: list(m.values()) for status, m in record.items()},
            rng=random_state or np.random.RandomState(),
        )

    def sample(self, initial_status: LeakageStatus) -> Transition:
        transitions = self.transitions[initial_status]
        probabilities = self.probabilities[initial_status]
        return self.rng.choice(transitions, p=probabilities)


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


def add_transition(
    record: dict[LeakageStatus, dict[Transition, float]],
    initial_status: LeakageStatus,
    final_status: LeakageStatus,
    probability: float,
    pauli_channel: list[float] | None = None,
) -> None:
    transitions = []
    probs = []
    if pauli_channel is None:
        transitions.append(Transition(final_status))
        probs.append(probability)
    else:
        for i, p in enumerate(pauli_channel):
            transitions.append(Transition(final_status, i))
            probs.append(p)
    for transition, prob in zip(transitions, probs):
        prev_prob = record.setdefault(initial_status, dict()).get(transition, 0.0)
        record[initial_status][transition] = prev_prob + prob


def project_kraus_operators(
    kraus_operators: Sequence[np.ndarray], num_qubits: int, num_level: int
) -> dict[LeakageStatus, dict[Transition, float]]:
    all_status = list(itertools.product(range(num_level - 1), repeat=num_qubits))
    record: dict[LeakageStatus, dict[Transition, float]] = dict()
    for kraus in kraus_operators:
        for initial_status, final_status in itertools.product(all_status, repeat=2):
            num_newly_leaked = get_num_newly_leaked_qubits(initial_status, final_status)
            prefactor: float = 1.0 / 2**num_newly_leaked
            projected_kraus = project_kraus_with_initial_final(
                kraus, num_qubits, num_level, initial_status, final_status
            )
            qubits_stay = get_qubits_stayed_in_computational_space(initial_status, final_status)
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
            add_transition(record, initial_status, final_status, probability, pauli_channel)
    return record
