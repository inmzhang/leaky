"""Utilities for the leaky module."""

from typing import List, Sequence, Tuple
import itertools
import functools

import numpy as np

from leaky import LeakyPauliChannel, LeakageStatus


LeakageSpaces = Tuple[int, ...]
ProjectSpaces = Tuple[Tuple[int, ...], ...]


PAULIS = [
    ("I", np.array([[1, 0], [0, 1]], dtype=complex)),
    ("X", np.array([[0, 1], [1, 0]], dtype=complex)),
    ("Y", np.array([[0, -1j], [1j, 0]], dtype=complex)),
    ("Z", np.array([[1, 0], [0, -1]], dtype=complex)),
]


def generalized_pauli_twirling(
    kraus_operators: Sequence[np.ndarray],
    num_qubits: int,
    num_level: int,
    safety_check: bool = True,
) -> LeakyPauliChannel:
    """Decompose the Kraus operators into a leaky pauli channel representation with
    Generalized Pauli Twirling(GPT).

    Args:
        kraus_operators: A sequence of Kraus operators corresponding to an operation's error channel.
        num_qubits: The number of qubits in the operation.
        num_level: The number of levels of the quantum system to be considered.
        safety_check: If True, perform a safety check to ensure the channel is valid.
            A channel is valid if the sum of the probabilities of all transitions
            from a given initial status is 1. And the pauli channel related to the
            qubits with transition type that not in R(stay in the computational space)
            should always be I. Default is True.

    Returns:
        A LeakyPauliChannel object representing the error channel.
    """
    channel = LeakyPauliChannel(num_qubits)

    all_status = list(itertools.product(range(num_level - 1), repeat=num_qubits))
    for kraus in kraus_operators:
        for initial_status, final_status in itertools.product(all_status, repeat=2):
            num_u = sum(
                s0 == 0 and s1 > 0 for s0, s1 in zip(initial_status, final_status)
            )
            q_in_r = [
                i
                for i, (s0, s1) in enumerate(zip(initial_status, final_status))
                if s0 == 0 and s1 == 0
            ]
            num_r = len(q_in_r)

            prefactor: float = 1.0 / 2**num_u
            projectors = _scatter_status(initial_status, final_status)
            for initial_projector, final_projector in projectors:
                projected_kraus = _project_kraus_with_initial_final(
                    kraus, num_qubits, num_level, initial_projector, final_projector
                )
                probability: float
                pauli_channel: List[Tuple[str, float]] = []
                if num_r == 0:
                    assert projected_kraus.shape == (1, 1)
                    probability = (
                        prefactor * np.abs(projected_kraus).astype(float) ** 2
                    ).item()
                    pauli_channel.append(("I" * num_qubits, probability))
                else:
                    dim = 2**num_r
                    assert projected_kraus.shape == (dim, dim)
                    for paulis in itertools.product(PAULIS, repeat=num_r):
                        pauli_array = [p[1] for p in paulis]
                        probability = (
                            prefactor
                            * np.abs(
                                np.trace(
                                    projected_kraus
                                    @ functools.reduce(np.kron, pauli_array)
                                )
                                / dim
                            )
                            ** 2
                        )
                        pauli_string: list[str] = []
                        i = 0
                        for q in range(num_qubits):
                            if q not in q_in_r:
                                pauli_string.append("I")
                            else:
                                pauli_string.append(paulis[i][0])
                                i += 1
                        pauli_channel.append(("".join(pauli_string), probability))

                    probability = sum([p for _, p in pauli_channel])
                if probability < 1e-9:
                    continue
                for pauli_op, p in pauli_channel:
                    if p < 1e-9:
                        continue
                    channel.add_transition(
                        LeakageStatus(status=list(initial_status)),
                        LeakageStatus(status=list(final_status)),
                        pauli_op,
                        p,
                    )
    if safety_check:
        channel.safety_check()
    return channel


def _l2p(leakage_status: LeakageSpaces) -> ProjectSpaces:
    return tuple((0, 1) if s == 0 else (s + 1,) for s in leakage_status)


def _get_projector_slice(
    num_level: int,
    project_status: ProjectSpaces,
) -> List[int]:
    """Get slice into the matrix for the subspace projection defined by project_status."""
    num_qubits = len(project_status)
    status = project_status[0]
    if num_qubits == 1:
        return list(status)
    tail_slice = _get_projector_slice(num_level, project_status[1:])
    return [x + s * num_level ** (num_qubits - 1) for s in status for x in tail_slice]


def _project_kraus_with_initial_final(
    kraus: np.ndarray,
    num_qubits: int,
    num_level: int,
    initial_project_status: ProjectSpaces,
    final_project_status: ProjectSpaces,
) -> np.ndarray:
    assert kraus.shape[0] == num_level**num_qubits
    initial_slice = _get_projector_slice(num_level, initial_project_status)
    final_slice = _get_projector_slice(num_level, final_project_status)
    return kraus[final_slice, :][:, initial_slice]


def _scatter_status(
    initial: LeakageSpaces, final: LeakageSpaces
) -> List[Tuple[ProjectSpaces, ProjectSpaces]]:
    initial_project_status = _l2p(initial)
    final_project_status = _l2p(final)

    up_indices = [
        i
        for i, (start, end) in enumerate(zip(initial, final))
        if start == 0 and end > 0
    ]
    down_indices = [
        i
        for i, (start, end) in enumerate(zip(initial, final))
        if start > 0 and end == 0
    ]

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
