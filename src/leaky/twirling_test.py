import itertools

import numpy as np
import pytest

from leaky import LeakageStatus
from leaky.twirling import (
    _get_projector_slice,
    generalized_pauli_twirling,
    PAULIS,
)

TWO_QUBITS_PAULIS = [
    (p1[0] + p2[0], np.kron(p1[1], p2[1]))
    for p1, p2 in itertools.product(PAULIS, PAULIS)
]


def test_get_projector_slice():
    assert _get_projector_slice(2, ((0,),)) == [0]
    assert _get_projector_slice(2, ((0, 1),)) == [0, 1]
    assert _get_projector_slice(3, ((2,),)) == [2]
    assert _get_projector_slice(3, ((0, 1), (2,))) == [2, 5]
    assert _get_projector_slice(3, ((2,), (2,))) == [8]
    assert _get_projector_slice(4, ((0, 1), (0, 1))) == [0, 1, 4, 5]
    assert _get_projector_slice(4, ((0, 1), (2,))) == [2, 6]


P = 1e-2

RNG = np.random.default_rng()


def test_single_qubit_depolarize_channel():
    channel_probs_list = []
    for _ in range(10):
        probs = RNG.random(4)
        probs /= sum(probs)
        channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        depolarize_channel = [
            np.sqrt(p) * pauli[1] for p, pauli in zip(channel_probs, PAULIS)
        ]
        channel = generalized_pauli_twirling(depolarize_channel, 1, 2)
        for i, p in enumerate(channel_probs):
            assert channel.get_prob_from_to(
                LeakageStatus(1), LeakageStatus(1), "IXYZ"[i]
            ) == pytest.approx(p)


def test_phase_damping_channel():
    k = 0.02
    kraus_operators = [
        np.array(
            [
                [1, 0],
                [0, np.sqrt(1 - k)],
            ],
            dtype=complex,
        ),
        np.array(
            [
                [0, np.sqrt(k)],
                [0, 0],
            ],
            dtype=complex,
        ),
    ]
    channel = generalized_pauli_twirling(kraus_operators, 1, 2)
    for pauli, sqrt_p in zip(
        "IXYZ",
        [
            (1 + np.sqrt(1 - k)) / 2,
            np.sqrt(k) / 2,
            1j * np.sqrt(k) / 2,
            (1 - np.sqrt(1 - k)) / 2,
        ],
    ):
        assert channel.get_prob_from_to(
            LeakageStatus(1), LeakageStatus(1), pauli
        ) == pytest.approx(np.abs(sqrt_p) ** 2)


def test_two_qubit_depolarize_channel():
    channel_probs_list = []
    for _ in range(10):
        probs = RNG.random(16)
        probs /= sum(probs)
        channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        pauli_ops = [pauli[0] for pauli in TWO_QUBITS_PAULIS]
        depolarize_channel = [
            np.sqrt(p) * pauli[1] for p, pauli in zip(channel_probs, TWO_QUBITS_PAULIS)
        ]
        channel = generalized_pauli_twirling(depolarize_channel, 2, 2)
        for i, p in enumerate(channel_probs):
            assert channel.get_prob_from_to(
                LeakageStatus(2), LeakageStatus(2), pauli_ops[i]
            ) == pytest.approx(p)


THETA = np.pi / 6
U = np.array(
    [
        [1, 0, 0, 0],
        [0, np.cos(THETA), np.sin(THETA), 0],
        [0, -np.sin(THETA), np.cos(THETA), 0],
        [0, 0, 0, 1],
    ],
    dtype=complex,
)


def test_single_qubit_4level_unitary():
    channel = generalized_pauli_twirling([U], 1, 4)
    equality_check = {
        ((0,), (0,), "I"): np.cos(THETA / 2) ** 4,
        ((0,), (0,), "Z"): np.sin(THETA / 2) ** 4,
        ((0,), (1,), "I"): np.sin(THETA) ** 2 / 2,
        ((1,), (0,), "I"): np.sin(THETA) ** 2,
        ((1,), (1,), "I"): np.cos(THETA) ** 2,
        ((2,), (2,), "I"): 1,
    }
    for initial, final in itertools.product(range(3), repeat=2):
        for pauli in "IXYZ":
            expected_prob = equality_check.get(((initial,), (final,), pauli), 0)
            computed_prob = channel.get_prob_from_to(
                LeakageStatus(status=[initial]), LeakageStatus(status=[final]), pauli
            )
            assert computed_prob == pytest.approx(expected_prob)


def test_two_qubit_4level_unitary_decompose():
    channel = generalized_pauli_twirling([np.kron(U, U)], 2, 4)
    equality_check = {
        ((0, 0), (0, 0), 0): np.cos(THETA / 2) ** 8,
        ((0, 0), (0, 0), 3): np.cos(THETA / 2) ** 4 * np.sin(THETA / 2) ** 4,
        ((0, 0), (0, 0), 12): np.cos(THETA / 2) ** 4 * np.sin(THETA / 2) ** 4,
        ((0, 0), (0, 0), 15): np.sin(THETA / 2) ** 8,
        ((0, 0), (0, 0), 10): 0,
        ((0, 0), (1, 1), 0): np.sin(THETA) ** 4 / 4,
        ((0, 0), (1, 0), 0): np.cos(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
        ((0, 0), (1, 0), 3): np.sin(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
        ((1, 0), (1, 0), 0): np.cos(THETA / 2) ** 4 * np.cos(THETA) ** 2,
        ((0, 0), (0, 2), 0): 0,
        ((2, 0), (2, 1), 0): np.sin(THETA) ** 2 / 2,
        ((1, 1), (1, 1), 0): np.cos(THETA) ** 4,
        ((2, 1), (2, 1), 0): np.cos(THETA) ** 2,
        ((0, 2), (0, 2), 0): np.cos(THETA / 2) ** 4,
        ((1, 2), (1, 2), 0): np.cos(THETA) ** 2,
        ((2, 2), (2, 2), 0): 1,
    }
    for check, p in equality_check.items():
        prob = channel.get_prob_from_to(
            LeakageStatus(status=list(check[0])),
            LeakageStatus(status=list(check[1])),
            TWO_QUBITS_PAULIS[check[2]][0],
        )
        assert prob == pytest.approx(p)
