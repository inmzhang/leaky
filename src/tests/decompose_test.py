import itertools

import numpy as np
import pytest

from leaky.decompose import get_projector_slice, decompose_kraus_operators, PAULIS, TWO_QUBITS_PAULIS
from leaky.transition import TransitionTable


def assert_float_equal(a: float, b: float):
    assert abs(a - b) < 1e-6


def test_get_projector_slice():
    assert get_projector_slice(2, ((0,),)) == [0]
    assert get_projector_slice(2, ((0, 1),)) == [0, 1]
    assert get_projector_slice(3, ((2,),)) == [2]
    assert get_projector_slice(3, ((0, 1), (2,))) == [2, 5]
    assert get_projector_slice(3, ((2,), (2,))) == [8]
    assert get_projector_slice(4, ((0, 1), (0, 1))) == [0, 1, 4, 5]
    assert get_projector_slice(4, ((0, 1), (2,))) == [2, 6]


P = 1e-2

RNG = np.random.default_rng()


def test_single_qubit_depolarize_decompose():
    channel_probs_list = []
    for _ in range(10):
        probs = RNG.random(4)
        probs /= sum(probs)
        channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        depolarize_channel = [np.sqrt(p) * pauli for p, pauli in zip(channel_probs, PAULIS)]
        transitions = decompose_kraus_operators(depolarize_channel, 1, 2)
        table = TransitionTable(transitions)
        for i, p in enumerate(channel_probs):
            assert table.get_transition_prob((0,), (0,), i) == pytest.approx(p)


def test_phase_damping_channel_decompose():
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
                [0, 0],
                [0, np.sqrt(k)],
            ],
            dtype=complex,
        ),
    ]
    transitions = decompose_kraus_operators(kraus_operators, 1, 2)
    table = TransitionTable(transitions)
    table.get_transition_prob((0,), (0,), 0) == pytest.approx((1 + np.sqrt(1 - k)) / 2)
    table.get_transition_prob((0,), (0,), 3) == pytest.approx((1 - np.sqrt(1 - k)) / 2)


def test_two_qubit_depolarize_decompose():
    channel_probs_list = []
    for _ in range(10):
        probs = RNG.random(16)
        probs /= sum(probs)
        channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        depolarize_channel = [np.sqrt(p) * pauli for p, pauli in zip(channel_probs, TWO_QUBITS_PAULIS)]
        transitions = decompose_kraus_operators(depolarize_channel, 2, 2)
        table = TransitionTable(transitions)
        for i, p in enumerate(channel_probs):
            assert table.get_transition_prob((0, 0), (0, 0), i) == pytest.approx(p)


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


def test_single_qubit_4level_unitary_decompose():
    transitions = decompose_kraus_operators([U], 1, 4)
    table = TransitionTable(transitions)
    equality_check = {
        ((0,), (0,), 0): np.cos(THETA / 2) ** 4,
        ((0,), (0,), 3): np.sin(THETA / 2) ** 4,
        ((0,), (1,), None): np.sin(THETA) ** 2 / 2,
        ((1,), (0,), None): np.sin(THETA) ** 2,
        ((1,), (1,), None): np.cos(THETA) ** 2,
        ((2,), (2,), None): 1,
    }
    for initial, final in itertools.product(range(3), repeat=2):
        for pauli_idx in [0, 1, 2, 3, None]:
            expected_prob = equality_check.get(((initial,), (final,), pauli_idx), 0)
            computed_prob = table.get_transition_prob((initial,), (final,), pauli_idx)
            assert computed_prob == pytest.approx(expected_prob)


def test_two_qubit_4level_unitary_decompose():
    transitions = decompose_kraus_operators([np.kron(U, U)], 2, 4)
    table = TransitionTable(transitions)
    equality_check = {
        ((0, 0), (0, 0), 0): np.cos(THETA / 2) ** 8,
        ((0, 0), (0, 0), 15): np.sin(THETA / 2) ** 8,
        ((0, 0), (0, 0), 10): 0,
        ((0, 0), (1, 1), None): np.sin(THETA) ** 4 / 4,
        ((0, 0), (0, 1), 0): np.cos(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
        ((0, 0), (0, 1), 3): np.sin(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
        ((0, 1), (0, 1), 0): np.cos(THETA / 2) ** 4 * np.cos(THETA) ** 2,
        ((0, 0), (2, 0), None): 0,
        ((0, 2), (1, 2), None): np.sin(THETA) ** 2 / 2,
        ((1, 1), (1, 1), None): np.cos(THETA) ** 4,
        ((1, 2), (1, 2), None): np.cos(THETA) ** 2,
        ((2, 0), (2, 0), 0): np.cos(THETA / 2) ** 4,
        ((2, 1), (2, 1), None): np.cos(THETA) ** 2,
        ((2, 2), (2, 2), None): 1,
    }
    for check, p in equality_check.items():
        assert table.get_transition_prob(*check) == pytest.approx(p)
