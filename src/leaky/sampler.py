from __future__ import annotations
from concurrent.futures import ProcessPoolExecutor

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
        self,
        shots: int,
        readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION,
        num_workers: int = 1,
    ) -> np.ndarray[np.uint8]:
        num_qubits = self._reference_circuit.num_qubits
        args = [
            (
                i,
                num_qubits,
                self._transition_collection,
                self._single_qubit_transition_controls,
                self._two_qubit_transition_controls,
                self._seed,
                self._reference_circuit,
                readout_strategy,
            )
            for i in range(shots)
        ]
        with ProcessPoolExecutor(max_workers=num_workers) as executor:
            results = executor.map(_sample_single_shot, args, chunksize=1000)
        return np.asarray(list(results), dtype=np.uint8)

    def sample_detectors(
        self,
        shots: int,
        readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION,
        num_workers: int = 1,
    ) -> tuple[np.ndarray[np.bool_], np.ndarray[np.bool_]]:
        if readout_strategy not in [
            ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION,
            ReadoutStrategy.RANDOM_LEAKAGE_PROJECTION,
        ]:
            raise ValueError("Invalid readout strategy for detector sampling.")
        measurements = self.sample(shots, readout_strategy, num_workers=num_workers)
        m2d_converter = self._reference_circuit.compile_m2d_converter()
        return m2d_converter.convert(measurements=measurements.astype(np.bool_), separate_observables=True)


def _sample_single_shot(args):
    (
        i,
        num_qubits,
        transition_collection,
        single_qubit_transition_controls,
        two_qubit_transition_controls,
        seed,
        reference_circuit,
        readout_strategy,
    ) = args
    seed = seed * (i + 1) if seed is not None else None
    simulator = Simulator(
        num_qubits,
        transition_collection,
        single_qubit_transition_controls,
        two_qubit_transition_controls,
        seed,
    )
    simulator.do_circuit(reference_circuit)
    return simulator.current_measurement_record(readout_strategy)
