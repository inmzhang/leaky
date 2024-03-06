from __future__ import annotations

from typing import Iterable
from enum import Enum, auto

import numpy as np
import stim

from leaky.transition import Transition, LeakageStatus, TransitionTable, TransitionType

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
    def __init__(self, num_qubits) -> None:
        self.status_vec = np.zeros(num_qubits, dtype=int)

    def get_status(self, qubits: list[int]) -> LeakageStatus:
        return tuple(self.status_vec[qubits])

    def set_status(self, qubits: list[int], status: int) -> None:
        self.status_vec[qubits] = status

    def apply_transition(self, on_qubits: list[int], transition: Transition) -> None:
        self.status_vec[on_qubits] = transition.final_status

    def clear(self) -> None:
        self.status_vec = np.zeros_like(self.status_vec, dtype=int)


class ReadoutStrategy(Enum):
    # read the raw labels
    RAW_LABEL = auto()
    # randomly project the leakage to the ground state(50% chance for 0/1)
    RANDOM_LEAKAGE_PROJECTION = auto()
    # deterministicly project the leakage state to state 1
    DETERMINISTIC_LEAKAGE_PROJECTION = auto()


class Simulator:
    def __init__(
        self, num_qubits: int, tables: dict[str, TransitionTable] | None = None, seed: int | None = None
    ) -> None:
        self._num_qubits = num_qubits
        self._tables = tables or dict()
        self._status_vec = StatusVec(num_qubits)
        self._rng = np.random.default_rng(seed)
        self._tableau_simulator = stim.TableauSimulator(seed=seed)
        self._tableau_simulator.set_num_qubits(num_qubits)
        self._measurement_status: list[int] = []

    def do(
        self,
        name: str,
        targets: Iterable[int | stim.GateTarge],
        args: float | Iterable[float] = (),
        add_potential_noise: bool = True,
    ) -> None:
        """Do instruction."""
        instruction = stim.CircuitInstruction(name, list(targets), list(args))
        self.do_instruction(instruction, add_potential_noise)

    def do_circuit(self, circuit: stim.Circuit, qubits_map: dict[int, int] | None = None) -> None:
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
        instruction_name = instruction.name
        instruction_targets = [t.qubit_value for t in instruction.targets_copy()]
        if instruction_name in ["M", "MZ"]:
            self.measure(instruction_targets)
            return
        if instruction_name in ["R", "RZ"]:
            self.reset(instruction_targets)
            return
        if instruction_name in ["MX", "MY", "RX", "RY", "MR", "MRX", "MRZ", "MRY", "MPP"]:
            raise ValueError(f"Only Z basis measurements and resets are supported, not {instruction_name}.")

        table = self._tables.get(instruction_name)
        for targets in _split_targets(instruction_name, instruction_targets):
            current_status = self._status_vec.get_status(targets)
            if all(s == 0 for s in current_status):
                self._tableau_simulator.do(stim.CircuitInstruction(instruction_name, targets))
            if table is None or not add_potential_noise:
                continue
            sampled_transition = table.sample(current_status, self._rng)
            self._apply_transition(targets, sampled_transition)

    def measure(self, targets: list[int]) -> None:
        """Z basis measurement."""
        self._measurement_status.extend(self._status_vec.get_status(targets))
        self._tableau_simulator.measure_many(*targets)

    def reset(self, targets: list[int]) -> None:
        """Z basis reset."""
        self._status_vec.set_status(targets, 0)
        self._tableau_simulator.reset(*targets)

    def current_measurement_record(self, readout_strategy: ReadoutStrategy = ReadoutStrategy.RAW_LABEL) -> list[int]:
        """Get the measurement record."""
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
        return self._status_vec.get_status(targets)

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
