import stim
import numpy as np

from leaky.sampler import Sampler


def test_sampler():
    circuit = stim.Circuit.from_file("src/tests/test_data/surface_code.stim")
    sampler = Sampler(circuit)
    measurements = sampler.sample(10)
    assert measurements.shape == (10, circuit.num_measurements)
    
    detectors, obs_flips = sampler.sample_detectors(10)
    assert detectors.shape == (10, circuit.num_detectors)
    assert obs_flips.shape == (10, circuit.num_observables)
    assert np.all(detectors == 0)
    assert np.all(obs_flips == 0)