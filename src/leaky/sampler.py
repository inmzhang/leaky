from __future__ import annotations

import stim
import numpy as np

from leaky.simulator import ReadoutStrategy, Simulator
from leaky.transition import TransitionCollection


class Sampler:
    def __init__(
        self,
        reference_circuit: stim.Circuit,
        transition_collection: TransitionCollection | None = None,
        single_qubit_transition_controls: dict[int, int] | None = None,
        two_qubit_transition_controls: dict[tuple[int, int], int] | None = None,
        seed: int | None = None,
    ) -> None:
        self._reference_circuit = reference_circuit
        self._transition_collection = transition_collection
        self._single_qubit_transition_controls = single_qubit_transition_controls
        self._two_qubit_transition_controls = two_qubit_transition_controls
        self._seed = seed

    def sample(
        self, shots: int, readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION
    ) -> np.ndarray[np.uint8]:
        num_qubits = self._reference_circuit.num_qubits
        results = []
        for i in range(shots):
            seed = self._seed * (i + 1) if self._seed is not None else None
            simulator = Simulator(
                num_qubits,
                self._transition_collection,
                self._single_qubit_transition_controls,
                self._two_qubit_transition_controls,
                seed,
            )
            simulator.do_circuit(self._reference_circuit)
            results.append(simulator.current_measurement_record(readout_strategy))
        return np.asarray(results, dtype=np.uint8)

    def sample_detectors(
        self, shots: int, readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION
    ) -> tuple[np.ndarray[np.bool_], np.ndarray[np.bool_]]:
        if readout_strategy not in [
            ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION,
            ReadoutStrategy.RANDOM_LEAKAGE_PROJECTION,
        ]:
            raise ValueError("Invalid readout strategy for detector sampling.")
        measurements = self.sample(shots, readout_strategy)
        m2d_converter = self._reference_circuit.compile_m2d_converter()
        return m2d_converter.convert(measurements=measurements.astype(np.bool_), separate_observables=True)
