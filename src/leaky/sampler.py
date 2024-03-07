from __future__ import annotations
from concurrent.futures import ProcessPoolExecutor

import stim
import numpy as np

from leaky.simulator import ReadoutStrategy, Simulator
from leaky.transition import TransitionCollection


class Sampler:
    """Core class for sampling from a reference circuit with leaky noise model."""

    def __init__(
        self,
        reference_circuit: stim.Circuit,
        transition_collection: TransitionCollection | None = None,
        seed: int | None = None,
    ) -> None:
        """Initialize the sampler.

        Args:
            reference_circuit: The reference circuit to sample from.
            transition_collection: The transition collection to use for sampling. If None, a default collection will be used.
            seed: The seed for the random number generator. If None, the default seed will be used.
        """
        self._reference_circuit = reference_circuit
        self._transition_collection = transition_collection
        self._seed = seed

    def sample(
        self,
        shots: int,
        readout_strategy: ReadoutStrategy = ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION,
        num_workers: int = 1,
    ) -> np.ndarray[np.uint8]:
        """Sample from the reference circuit with the leaky noise model.

        Args:
            shots: The number of shots to sample.
            readout_strategy: The readout strategy to use for sampling.
            num_workers: The number of workers to use for parallel sampling.

        Returns:
            An array of shape (shots, num_measurements) with dtype np.uint8.
        """
        num_qubits = self._reference_circuit.num_qubits
        args = [
            (
                i,
                num_qubits,
                self._transition_collection,
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
        """Sample the detectors from the reference circuit with the leaky noise model.

        Args:
            shots: The number of shots to sample.
            readout_strategy: The readout strategy to use for sampling.
            num_workers: The number of workers to use for parallel sampling.

        Returns:
            A tuple of two arrays. The first array is of shape (shots, num_detectors) with dtype np.bool_. The second
            array is of shape (shots, num_observables) with dtype np.bool_.
        """
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
        seed,
        reference_circuit,
        readout_strategy,
    ) = args
    seed = seed * (i + 1) if seed is not None else None
    simulator = Simulator(
        num_qubits,
        transition_collection,
        seed,
    )
    simulator.do_circuit(reference_circuit)
    return simulator.current_measurement_record(readout_strategy)
