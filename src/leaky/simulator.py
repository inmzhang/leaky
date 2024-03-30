from __future__ import annotations

from typing import Iterable
from enum import Enum, auto

import numpy as np
import stim

from leaky.transition import Transition, LeakageStatus, TransitionTable, TransitionType, TransitionCollection

SINGLE_CLIFFORD_GATES = [
    "I",
    "X",
    "Y",
    "Z",
    "C_XYZ",
    "C_ZYX",
    "H",
    "H_XY",
    "H_XZ",
    "H_YZ",
    "S",
    "SQRT_X",
    "SQRT_X_DAG",
    "SQRT_Y",
    "SQRT_Y_DAG",
    "SQRT_Z",
    "SQRT_Z_DAG",
    "S_DAG",
]

STIM_ANNOTATIONS = ["DETECTOR", "MPAD", "OBSERVABLE_INCLUDE", "QUBIT_COORDS", "SHIFT_COORDS", "TICK"]


class StatusVec:
    """Vector of leakage status for the qubits."""

    def __init__(self, num_qubits) -> None:
        self.status_vec = np.zeros(num_qubits, dtype=int)

    def get_status(self, qubits: list[int]) -> LeakageStatus:
        """Get the leakage status of the qubits.

        Args:
            qubits: The qubits to get the status.

        Returns:
            The leakage status of the qubits.
        """
        return tuple(self.status_vec[qubits])

    def set_status(self, qubits: list[int], status: int) -> None:
        """Set the leakage status of the qubits.

        Args:
            qubits: The qubits to set the status.
            status: The leakage status to set.
        """
        self.status_vec[qubits] = status

    def apply_transition(self, on_qubits: list[int], transition: Transition) -> None:
        """Apply the transition to the status vector.

        Args:
            on_qubits: The qubits to apply the transition.
            transition: The transition to apply.
        """
        self.status_vec[on_qubits] = transition.final_status

    def clear(self) -> None:
        """Clear the status vector."""
        self.status_vec = np.zeros_like(self.status_vec, dtype=int)


class ReadoutStrategy(Enum):
    """The readout strategy for the measurement record.

    RAW_LABEL: The raw measurement record, including the leakage state.
    RANDOM_LEAKAGE_PROJECTION: Randomly project the leakage to the ground state(50% chance for 0/1).
    DETERMINISTIC_LEAKAGE_PROJECTION: Deterministicly project the leakage state to state 1.
    """

    RAW_LABEL = auto()
    RANDOM_LEAKAGE_PROJECTION = auto()
    DETERMINISTIC_LEAKAGE_PROJECTION = auto()


class Simulator:
    """Core simulator for the leaky noise model."""

    def __init__(
        self,
        num_qubits: int,
        transition_collection: TransitionCollection | None = None,
        single_qubit_transition_controls: dict[int, int] | None = None,
        two_qubit_transition_controls: dict[tuple[int, int], int] | None = None,
        seed: int | None = None,
    ) -> None:
        """Initialize the simulator.

        Args:
            num_qubits: The number of qubits in the simulator.
            transition_collection: The transition collection to use for the simulator.
            single_qubit_transition_controls: The classical control register for selecting single qubit transition tables.
            two_qubit_transition_controls: The classical control register for selecting two qubit transition tables.
            seed: The random seed for the simulator.
        """
        self._num_qubits = num_qubits
        # classical control registers for selecting transition tables
        self._single_qubit_transition_controls = single_qubit_transition_controls or dict()
        self._two_qubit_transition_controls = two_qubit_transition_controls or dict()

        self._transition_collection = transition_collection or TransitionCollection()
        self._status_vec = StatusVec(num_qubits)
        self._rng = np.random.default_rng(seed)
        self._tableau_simulator = stim.TableauSimulator(seed=seed)
        self._tableau_simulator.set_num_qubits(num_qubits)
        self._measurement_status: list[int] = []

    @property
    def num_qubits(self) -> int:
        """Return the number of qubits in the simulator."""
        return self._num_qubits

    @property
    def internal_tableau_simulator(self) -> stim.TableauSimulator:
        """Return the internal stim.TableauSimulator instance."""
        return self._tableau_simulator

    @property
    def transition_collection(self) -> TransitionCollection:
        """Return the transition collection."""
        return self._transition_collection

    @property
    def single_qubit_transition_controls(self) -> dict[int, int]:
        """Return the current single qubit transition controls."""
        return self._single_qubit_transition_controls

    @property
    def two_qubit_transition_controls(self) -> dict[tuple[int, int], int]:
        """Return the current two qubit transition controls."""
        return self._two_qubit_transition_controls

    def set_single_qubit_transition_controls(self, controls: dict[int, int]) -> None:
        """Set the single qubit transition controls."""
        self._single_qubit_transition_controls.update(controls)

    def set_two_qubit_transition_controls(self, controls: dict[tuple[int, int], int]) -> None:
        """Set the two qubit transition controls."""
        self._two_qubit_transition_controls.update(controls)

    def do(
        self,
        name: str,
        targets: Iterable[int | stim.GateTarge],
        args: float | Iterable[float] = (),
        add_potential_noise: bool = True,
    ) -> None:
        """Do a single circuit instruction.

        Args:
            name: The name of the instruction.
            targets: The qubits to apply the instruction.
            args: The arguments for the instruction.
            add_potential_noise: Whether to add potential noise to the instruction. When True,
                the simulator will sample a transition from the transition table(if exists) and
                apply it to the qubits. Default is True.
        """
        instruction = stim.CircuitInstruction(name, list(targets), list(args))
        self.do_instruction(instruction, add_potential_noise)

    def do_circuit(self, circuit: stim.Circuit, qubits_map: dict[int, int] | None = None) -> None:
        """Do a circuit.

        Args:
            circuit: The circuit to apply.
            qubits_map: The mapping from the circuit qubits to the simulator qubits. The simulator
                qubits are indexed from 0 to num_qubits-1. If None, the qubits in the circuit are
                automatically mapped to the simulator qubits.
        """
        if circuit.num_qubits != self._num_qubits:
            raise ValueError(f"Expected {self._num_qubits} qubits, but got {circuit.num_qubits} in the circuit.")

        # map circuit qubits to simulator qubits
        qubits_in_circuit = list(circuit.get_final_qubit_coordinates().keys())
        qubits_map = qubits_map or dict(zip(qubits_in_circuit, range(self._num_qubits)))

        for instruction in circuit:
            if isinstance(instruction, stim.CircuitRepeatBlock):
                body = instruction.body_copy()
                repeatitions = instruction.repeat_count
                for _ in range(repeatitions):
                    self.do_circuit(body, qubits_map)
                continue
            elif instruction.name in STIM_ANNOTATIONS:
                continue
            instruction_targets = [qubits_map[t.qubit_value] for t in instruction.targets_copy()]
            self.do_instruction(
                stim.CircuitInstruction(instruction.name, instruction_targets, instruction.gate_args_copy())
            )

    def do_instruction(
        self,
        instruction: stim.CircuitInstruction,
        add_potential_noise: bool = True,
    ) -> None:
        """Do a single circuit instruction.

        Args:
            instruction: The instruction to apply.
            add_potential_noise: Whether to add potential noise to the instruction. When True,
                the simulator will sample a transition from the transition table(if exists) and
                apply it to the qubits. Default is True.
        """
        instruction_name = instruction.name
        instruction_targets = [t.qubit_value for t in instruction.targets_copy()]
        instruction_args = instruction.gate_args_copy()
        if instruction_name in ["M", "MZ"]:
            assert len(instruction_args) <= 1
            self.measure(instruction_targets, instruction_args[0] if instruction_args else None)
            return
        if instruction_name in ["R", "RZ"]:
            assert len(instruction_args) <= 1
            self.reset(instruction_targets, instruction_args[0] if instruction_args else None)
            return
        if instruction_name in ["MR", "MRZ"]:
            self.measure(instruction_targets)
            self.reset(instruction_targets)
        if instruction_name in ["MX", "MY", "RX", "RY", "MRX", "MRY", "MPP"]:
            raise ValueError(f"Only Z basis measurements and resets are supported, not {instruction_name}.")

        if not self.transition_collection.has_table_for(instruction_name):
            self._tableau_simulator.do(instruction)
            return

        for targets in _split_targets(instruction_name, instruction_targets):
            table = self._get_satifying_table(instruction_name, targets)
            current_status = self._status_vec.get_status(targets)
            if all(s == 0 for s in current_status):
                self._tableau_simulator.do(stim.CircuitInstruction(instruction_name, targets, instruction_args))
            if table is None or not add_potential_noise:
                continue
            sampled_transition = table.sample(current_status, self._rng)
            self._apply_transition(targets, sampled_transition)

    def measure(self, targets: list[int], flip_probability: float | None = None) -> None:
        """Z basis measurement.

        Args:
            targets: The qubits to measure.
        """
        self._measurement_status.extend(self._status_vec.get_status(targets))
        measurement_args = [flip_probability] if flip_probability is not None else []
        self._tableau_simulator.do(stim.CircuitInstruction("M", targets, measurement_args))

    def reset(self, targets: list[int], flip_probability: float | None = None) -> None:
        """Z basis reset.

        Args:
            targets: The qubits to reset.
        """
        self._status_vec.set_status(targets, 0)
        reset_args = [flip_probability] if flip_probability is not None else []
        self._tableau_simulator.do(stim.CircuitInstruction("R", targets, reset_args))

    def current_measurement_record(self, readout_strategy: ReadoutStrategy = ReadoutStrategy.RAW_LABEL) -> list[int]:
        """Get the measurement record.

        Args:
            readout_strategy: The readout strategy for the measurement record. Default is ReadoutStrategy.RAW_LABEL.

        Returns:
            The measurement record as a list of integers.
        """
        if readout_strategy == ReadoutStrategy.RAW_LABEL:
            return [
                int(m) if status == 0 else status + 1
                for m, status in zip(self._tableau_simulator.current_measurement_record(), self._measurement_status)
            ]
        if readout_strategy == ReadoutStrategy.RANDOM_LEAKAGE_PROJECTION:
            return [
                int(m) if status == 0 else self._rng.choice([0, 1])
                for m, status in zip(self._tableau_simulator.current_measurement_record(), self._measurement_status)
            ]
        if readout_strategy == ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION:
            return [
                int(m) if status == 0 else 1
                for m, status in zip(self._tableau_simulator.current_measurement_record(), self._measurement_status)
            ]

    def current_status(self, targets: list[int]) -> LeakageStatus:
        """Get the current leakage status of the qubits.

        Args:
            targets: The qubits to get the status.

        Returns:
            The leakage status of the qubits.
        """
        return self._status_vec.get_status(targets)

    def _get_satifying_table(self, instruction_name: str, targets: list[int]) -> TransitionTable | None:
        leakage_status = self._status_vec.get_status(targets)
        if len(targets) == 1:
            single_qubit_table_control = self._single_qubit_transition_controls.get(targets[0])
            two_qubit_table_controls = None
        else:
            single_qubit_table_control = None
            two_qubit_table_controls = self._two_qubit_transition_controls.get(tuple(targets))
        return self._transition_collection.get_table(
            instruction_name, leakage_status, single_qubit_table_control, two_qubit_table_controls
        )

    def _apply_transition(self, targets: list[int], transition: Transition) -> None:
        self._status_vec.apply_transition(targets, transition)
        transition_types = transition.get_transition_types()
        qubits_in_r = []
        for target, transition_type in zip(targets, transition_types):
            if transition_type == TransitionType.U:
                self._tableau_simulator.x_error(target, p=0.5)
                self._tableau_simulator.reset(target)
            elif transition_type == TransitionType.D:
                self._tableau_simulator.reset(target)
                self._tableau_simulator.x_error(target, p=0.5)
            elif transition_type == TransitionType.R:
                qubits_in_r.append(target)
        if qubits_in_r:
            pauli_channel = transition.get_pauli_channel_name(is_single_qubit_channel=len(qubits_in_r) == 1)
            assert pauli_channel is not None, "TransitionType.R should have a pauli_channel."
            for qubit, pauli in zip(qubits_in_r, pauli_channel):
                self._tableau_simulator.do(stim.CircuitInstruction(pauli, [qubit]))


def _split_targets(instruction_name: str, instruction_targets: list[int]) -> list[list[int]]:
    if instruction_name in SINGLE_CLIFFORD_GATES:
        return [[t] for t in instruction_targets]
    return [[t1, t2] for t1, t2 in zip(instruction_targets[::2], instruction_targets[1::2])]
