from __future__ import annotations
from enum import Enum, auto
import dataclasses
import itertools

import numpy as np

PAULI_STRINGS = ["I", "X", "Y", "Z"]

LeakageStatus = tuple[int, ...]


class TransitionType(Enum):
    R = auto()
    U = auto()
    D = auto()
    L = auto()


@dataclasses.dataclass(frozen=True)
class Transition:
    initial_status: LeakageStatus
    final_status: LeakageStatus
    probability: float
    pauli_channel_idx: int | None = None

    def get_transition_types(self) -> list[TransitionType]:
        transition_types = []
        for init, final in zip(self.initial_status, self.final_status):
            if init == 0 and final == 0:
                transition_types.append(TransitionType.R)
            elif init == 0 and final > 0:
                transition_types.append(TransitionType.U)
            elif init > 0 and final == 0:
                transition_types.append(TransitionType.D)
            else:
                transition_types.append(TransitionType.L)
        return transition_types

    def get_pauli_channel_name(self, is_single_qubit_channel: bool) -> tuple[str] | None:
        if self.pauli_channel_idx is None:
            return None
        if is_single_qubit_channel:
            return (PAULI_STRINGS[self.pauli_channel_idx],)
        return list(itertools.product(PAULI_STRINGS, repeat=2))[self.pauli_channel_idx]



@dataclasses.dataclass
class TransitionTable:
    transitions: dict[LeakageStatus, list[Transition]]

    def get_transition_prob(
        self, initial_status: LeakageStatus, final_status: LeakageStatus, pauli_channel_idx: int | None
    ) -> float:
        for t in self.transitions[initial_status]:
            if t.final_status == final_status and t.pauli_channel_idx == pauli_channel_idx:
                return t.probability
        return 0.0

    def sample(self, initial_status: LeakageStatus, rng: np.random.Generator) -> Transition:
        transitions = self.transitions[initial_status]
        probabilities = [t.probability for t in transitions]
        return rng.choice(transitions, p=probabilities)
