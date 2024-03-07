from __future__ import annotations
from enum import Enum, auto
import dataclasses
import itertools
from typing import Callable

import numpy as np

PAULI_STRINGS = ["I", "X", "Y", "Z"]

LeakageStatus = tuple[int, ...]
"""The leakage status of the qubits in the circuit.

Important:
    In `leaky`, the computational subspace is represented by 0, and the leakage subspace is represented by positive
    integers. |2> is represented by 1, |3> is represented by 2, and so on.
"""


class TransitionType(Enum):
    """The type of transition between two leakage states.

    R: the qubit stays within the computational subspace.
    U: the qubit transitions from the computational subspace to the leakage subspace.
    D: the qubit transitions from the leakage subspace to the computational subspace.
    L: the qubit stays within the leakage subspace.
    """

    R = auto()
    U = auto()
    D = auto()
    L = auto()


@dataclasses.dataclass(frozen=True)
class Transition:
    """The transition between two leakage status.

    Attributes:
        initial_status: The initial leakage status before transition.
        final_status: The final leakage status after transition.
        probability: The probability of the transition.
        pauli_channel_idx: The index of the Pauli channel attached to the transition. If None, the transition is
            not associated with a Pauli channel. For single-qubit channels, the index is in the range [0, 3]. For
            two-qubit channels, the index is in the range [0, 15].
    """

    initial_status: LeakageStatus
    final_status: LeakageStatus
    probability: float
    pauli_channel_idx: int | None = None

    def get_transition_types(self) -> list[TransitionType]:
        """Get the transition types for each qubit in the transition.

        Returns:
            A list of TransitionType instances, one for each qubit in the transition.
        """
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
        """Get the Pauli channel name associated with the transition.

        Args:
            is_single_qubit_channel: Whether the channel is a single-qubit channel.

        Returns:
            The Pauli channel name associated with the transition. If the transition is not associated with a Pauli
            channel, None is returned.
        """
        if self.pauli_channel_idx is None:
            return None
        if is_single_qubit_channel:
            return (PAULI_STRINGS[self.pauli_channel_idx],)
        return list(itertools.product(PAULI_STRINGS, repeat=2))[self.pauli_channel_idx]


@dataclasses.dataclass(frozen=True)
class TransitionTable:
    """All the possible transitions of an operation.

    Attributes:
        transitions: A dictionary with the initial leakage status as keys and a list of transitions as values.
    """

    transitions: dict[LeakageStatus, list[Transition]]

    def get_transition_prob(
        self, initial_status: LeakageStatus, final_status: LeakageStatus, pauli_channel_idx: int | None
    ) -> float:
        """Get the probability of a transition.

        Args:
            initial_status: The initial leakage status before transition.
            final_status: The final leakage status after transition.
            pauli_channel_idx: The index of the Pauli channel attached to the transition. If None, the transition is
                not associated with a Pauli channel. For single-qubit channels, the index is in the range [0, 3]. For
                two-qubit channels, the index is in the range [0, 15].

        Returns:
            The probability of the transition. If the transition is not in the table, 0.0 is returned.
        """
        for t in self.transitions[initial_status]:
            if t.final_status == final_status and t.pauli_channel_idx == pauli_channel_idx:
                return t.probability
        return 0.0

    def sample(self, initial_status: LeakageStatus, rng: np.random.Generator) -> Transition:
        """Sample a transition from the table.

        Args:
            initial_status: The initial leakage status before transition.
            rng: The random number generator to use for sampling.

        Returns:
            A transition sampled from the table.
        """
        transitions = self.transitions[initial_status]
        probabilities = np.array([t.probability for t in transitions])
        probabilities /= np.sum(probabilities)
        return rng.choice(transitions, p=probabilities)


TableCondition = Callable[[LeakageStatus, int | None, int | None], bool]


class ConditionalTable:
    """A transition table with an associated condition."""

    def __init__(self, table: TransitionTable, condition: TableCondition | None) -> None:
        self.table = table
        self.condition = condition

    def check_condition(
        self,
        status: LeakageStatus,
        single_qubit_table_control: int | None,
        two_qubit_table_control: int | None,
    ) -> bool:
        """Check if the condition is satisfied.

        Args:
            status: The leakage status of the qubits.
            single_qubit_table_control: The value of the single qubit controller.
            two_qubit_table_control: The value of the two qubit controller.

        Returns:
            True if the condition is satisfied, False otherwise.
        """
        if self.condition is None:
            return True
        return self.condition(status, single_qubit_table_control, two_qubit_table_control)


class TransitionCollection:
    """The collection of conditional transition tables for different instructions."""

    def __init__(self) -> None:
        self._tables: dict[str, list[ConditionalTable]] = dict()

    def add_transition_table(
        self, instruction_name: str, transition_table: TransitionTable, condition: TableCondition | None = None
    ) -> None:
        """Add a transition table to the collection.

        Args:
            instruction_name: The name of the instruction.
            transition_table: The transition table to add.
            condition: The condition associated with the table. If None, the table is always used.
        """
        self._tables.setdefault(instruction_name, []).append(ConditionalTable(transition_table, condition))

    def get_table(
        self,
        instruction_name: str,
        leakage_status: LeakageStatus,
        single_qubit_table_control: int | None,
        two_qubit_table_control: int | None,
    ) -> TransitionTable | None:
        """Get the transition table for an instruction.

        Args:
            instruction_name: The name of the instruction.
            leakage_status: The leakage status of the instruction targets.
            single_qubit_table_control: The value of the single qubit controller.
            two_qubit_table_control: The value of the two qubit controller.

        Returns:
            The selected transition table for the instruction. If no table is found or satisfying the related
            condition, None is returned.
        """
        tables = self._tables.get(instruction_name)
        if tables is None:
            return None
        for table in tables:
            if table.check_condition(leakage_status, single_qubit_table_control, two_qubit_table_control):
                return table.table
        return None
