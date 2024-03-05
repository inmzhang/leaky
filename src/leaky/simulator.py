from __future__ import annotations

from typing import Iterable
from enum import Enum, auto

import numpy as np
import stim

from leaky.transition import Transition, LeakageStatus, TransitionTable, TransitionType

STIM_ANNOTATIONS = ["DETECTOR", "MPAD", "OBSERVABLE_INCLUDE", "QUBIT_COORDS", "SHIFT_COORDS", "TICK"]


class StatusVec:
    def __init__(self, num_qubits) -> None:
        self.status_vec = np.zeros(num_qubits, dtype=int)

    def get_status(self, qubits: list[int]) -> LeakageStatus:
        return tuple(self.status_vec[qubits])

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
    def __init__(self, num_qubits: int, tables: dict[str, TransitionTable] | None, seed: int | None) -> None:
        self._tables = tables or dict()
        self._status_vec = StatusVec(num_qubits)
        self._rng = np.random.default_rng(seed)
        self._tableau_simulator = stim.TableauSimulator(seed)
        self._tableau_simulator.set_num_qubits(num_qubits)
        self._measurement_status: list[int] = []

    def do(
        self,
        name: str,
        targets: int | stim.GateTarget | Iterable[int | stim.GateTarge],
        args: float | Iterable[float],
    ) -> None:
        """Do instruction."""
        instruction = stim.CircuitInstruction(name, targets, args)
        self.do_instruction(instruction)

    def do_circuit(self, circuit: stim.Circuit) -> None:
        for instruction in circuit:
            if isinstance(instruction, stim.CircuitRepeatBlock):
                body = instruction.body_copy()
                repeatitions = instruction.repeat_count
                for _ in range(repeatitions):
                    self.do_circuit(body)
                continue
            elif instruction.name in STIM_ANNOTATIONS:
                continue
            self.do_instruction(instruction)

    def do_instruction(self, instruction: stim.CircuitInstruction) -> None:
        instruction_name = instruction.name
        instruction_targets = [t.qubit_value for t in instruction.targets_copy()]
        if instruction_name in ["M", "MZ"]:
            self.measure(instruction_targets)
            return
        if instruction_name in ["R", "RZ"]:
            self.reset(instruction_targets)
            return
        if instruction_name in ["MX", "MY", "RX", "RY"]:
            raise ValueError(f"Only Z basis measurements and resets are supported, not {instruction_name}.")

        self._tableau_simulator.do(instruction)
        table = self._tables.get(instruction_name)
        if table is None:
            return
        current_status = self._status_vec.get_status(instruction_targets)
        sampled_transition = table.sample(current_status, self._rng)
        self._apply_transition(instruction_targets, sampled_transition)

    def measure(self, targets: list[int]) -> None:
        """Z basis measurement."""
        self._measurement_status.extend(self._status_vec.get_status(targets))
        self._tableau_simulator.measure_many(*targets)

    def reset(self, targets: list[int]) -> None:
        """Z basis reset."""
        self._status_vec[targets] = 0
        self._tableau_simulator.reset(*targets)

    def current_measurement_record(
        self, readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION
    ) -> list[int]:
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
