from __future__ import annotations
from enum import Enum, auto
import dataclasses
import itertools
from typing import Callable

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
        probabilities = np.array([t.probability for t in transitions])
        probabilities /= np.sum(probabilities)
        return rng.choice(transitions, p=probabilities)


TableCondition = Callable[[LeakageStatus, int | None, int | None], bool]


class ConditionalTable:
    def __init__(self, table: TransitionTable, condition: TableCondition | None) -> None:
        self.table = table
        self.condition = condition

    def check_condition(
        self,
        status: LeakageStatus,
        single_qubit_table_control: int | None,
        two_qubit_table_control: int | None,
    ) -> bool:
        if self.condition is None:
            return True
        return self.condition(status, single_qubit_table_control, two_qubit_table_control)


class TransitionCollection:
    def __init__(self) -> None:
        self._tables: dict[str, list[ConditionalTable]] = dict()

    def add_transition_table(
        self, instruction_name: str, transition_table: TransitionTable, condition: TableCondition | None = None
    ) -> None:
        self._tables.setdefault(instruction_name, []).append(ConditionalTable(transition_table, condition))

    def get_table(
        self,
        instruction_name: str,
        leakage_status: LeakageStatus,
        single_qubit_table_control: int | None,
        two_qubit_table_control: int | None,
    ) -> TransitionTable | None:
        tables = self._tables.get(instruction_name)
        if tables is None:
            return None
        for table in tables:
            if table.check_condition(leakage_status, single_qubit_table_control, two_qubit_table_control):
                return table.table
        return None
