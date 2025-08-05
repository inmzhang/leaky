import itertools

import leaky
import numpy as np
import pytest

from leaky.utils import (
    _get_projector_slice,
    decompose_kraus_operators_to_leaky_channel,
    PAULIS,
)


def ls(s: str) -> leaky.LeakageStatus:
    status = leaky.LeakageStatus(len(s))
    for i, c in enumerate(s):
        status.set(i, int(c))
    return status


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


def test_single_qubit_depolarize_decompose():
    channel_probs_list = []
    for _ in range(10):
        probs = RNG.random(4)
        probs /= sum(probs)
        channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        depolarize_channel = [
            np.sqrt(p) * pauli for p, (_, pauli) in zip(channel_probs, PAULIS)
        ]
        channel = decompose_kraus_operators_to_leaky_channel(depolarize_channel, 1, 2)
        for i, p in enumerate(channel_probs):
            assert channel.get_prob_from_to(
                ls("0"), ls("0"), "IXYZ"[i]
            ) == pytest.approx(p)


def test_two_qubit_depolarize_decompose():
    channel_probs_list = []
    probs = np.ones(16)
    probs /= sum(probs)
    channel_probs_list.append(probs)
    # for _ in range(1):
    #     probs = RNG.random(16)
    #     probs /= sum(probs)
    #     channel_probs_list.append(probs)
    for channel_probs in channel_probs_list:
        depolarize_channel = []
        pauli_operators = []

        for p, ((ps1, pauli1), (ps2, pauli2)) in zip(
            channel_probs, itertools.product(PAULIS, PAULIS)
        ):
            depolarize_channel.append(np.sqrt(p) * np.kron(pauli1, pauli2))
            pauli_operators.append(ps1 + ps2)
        channel = decompose_kraus_operators_to_leaky_channel(depolarize_channel, 2, 2)
        for i, p in enumerate(channel_probs):
            assert channel.get_prob_from_to(ls("00"), ls("00"), "IX") == pytest.approx(p)


# THETA = np.pi / 6
# U = np.array(
#     [
#         [1, 0, 0, 0],
#         [0, np.cos(THETA), np.sin(THETA), 0],
#         [0, -np.sin(THETA), np.cos(THETA), 0],
#         [0, 0, 0, 1],
#     ],
#     dtype=complex,
# )
#
#
# def test_single_qubit_4level_unitary_decompose():
#     channel = decompose_kraus_operators_to_leaky_pauli_channel([U], 1, 4)
#     equality_check = {
#         ((0,), (0,), 0): np.cos(THETA / 2) ** 4,
#         ((0,), (0,), 3): np.sin(THETA / 2) ** 4,
#         ((0,), (1,), 0): np.sin(THETA) ** 2 / 2,
#         ((1,), (0,), 0): np.sin(THETA) ** 2,
#         ((1,), (1,), 0): np.cos(THETA) ** 2,
#         ((2,), (2,), 0): 1,
#     }
#     for initial, final in itertools.product(range(3), repeat=2):
#         for pauli_idx in [0, 1, 2, 3]:
#             expected_prob = equality_check.get(((initial,), (final,), pauli_idx), 0)
#             computed_prob = channel.get_prob_from_to(initial, final, pauli_idx)
#             assert computed_prob == pytest.approx(expected_prob)
#
#
# def test_two_qubit_4level_unitary_decompose():
#     channel = decompose_kraus_operators_to_leaky_pauli_channel([np.kron(U, U)], 2, 4)
#     equality_check = {
#         (0x00, 0x00, 0): np.cos(THETA / 2) ** 8,
#         (0x00, 0x00, 15): np.sin(THETA / 2) ** 8,
#         (0x00, 0x00, 10): 0,
#         (0x00, 0x11, 0): np.sin(THETA) ** 4 / 4,
#         (0x00, 0x01, 0): np.cos(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
#         (0x00, 0x01, 12): np.sin(THETA / 2) ** 4 * np.sin(THETA) ** 2 / 2,
#         (0x01, 0x01, 0): np.cos(THETA / 2) ** 4 * np.cos(THETA) ** 2,
#         (0x00, 0x20, 0): 0,
#         (0x02, 0x12, 0): np.sin(THETA) ** 2 / 2,
#         (0x11, 0x11, 0): np.cos(THETA) ** 4,
#         (0x12, 0x12, 0): np.cos(THETA) ** 2,
#         (0x20, 0x20, 0): np.cos(THETA / 2) ** 4,
#         (0x21, 0x21, 0): np.cos(THETA) ** 2,
#         (0x22, 0x22, 0): 1,
#     }
#     for check, p in equality_check.items():
#         assert channel.get_prob_from_to(*check) == pytest.approx(p)
