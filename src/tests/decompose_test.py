import itertools

import numpy as np

from leaky.decompose import get_projector_slice, project_kraus_operators
from leaky.transition import TransitionTable


def assert_float_equal(a: float, b: float):
    assert abs(a - b) < 1e-6


def test_get_projector_slice():
    assert get_projector_slice(2, [0]) == [0, 1]
    assert get_projector_slice(3, [1]) == [2]
    assert get_projector_slice(3, [0, 1]) == [2, 5]
    assert get_projector_slice(3, [1, 1]) == [8]
    assert get_projector_slice(4, [0, 0]) == [0, 1, 4, 5]


def test_single_qubit_unitary_decompose():
    theta = np.pi / 6
    U = np.array(
        [
            [1, 0, 0, 0],
            [0, np.cos(theta), np.sin(theta), 0],
            [0, -np.sin(theta), np.cos(theta), 0],
            [0, 0, 0, 1],
        ],
        dtype=complex,
    )
    transitions = project_kraus_operators([U], 1, 4)
    table = TransitionTable(transitions)
    equality_check = {
        ((0,), (0,), 0): np.cos(theta / 2) ** 4,
        ((0,), (0,), 3): np.sin(theta / 2) ** 4,
        ((0,), (1,), None): np.sin(theta) ** 2 / 2,
        ((1,), (0,), None): np.sin(theta) ** 2,
        ((1,), (1,), None): np.cos(theta) ** 2,
        ((2,), (2,), None): 1,
    }
    for initial, final in itertools.product(range(3), repeat=2):
        for pauli_idx in [0, 1, 2, 3, None]:
            expected_prob = equality_check.get(((initial,), (final,), pauli_idx), 0)
            computed_prob = table.get_transition_prob((initial,), (final,), pauli_idx)
            assert_float_equal(expected_prob, computed_prob)
         