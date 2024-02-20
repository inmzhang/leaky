import numpy as np

from leaky.decompose import get_projector_slice, project_kraus


def assert_float_equal(a, b):
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
    table = project_kraus(U, 1, 4)
    # C -> C; I
    assert_float_equal(table.get((0,), (0,), 0), np.cos(theta / 2) ** 4)
    # C -> C; X
    assert_float_equal(table.get((0,), (0,), 1), 0)
    # C -> C; Y
    assert_float_equal(table.get((0,), (0,), 2), 0)
    # C -> C; Z
    assert_float_equal(table.get((0,), (0,), 3), np.sin(theta / 2) ** 4)
    # C -> 2
    assert_float_equal(table.get((0,), (1,)), np.sin(theta) ** 2 / 2)
    # 2 -> C
    assert_float_equal(table.get((1,), (0,)), np.sin(theta) ** 2)
    # 2 -> 2
    assert_float_equal(table.get((1,), (1,)), np.cos(theta) ** 2)
    # 3 -> 3
    assert_float_equal(table.get((2,), (2,)), 1)
