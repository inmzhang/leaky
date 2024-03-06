import numpy as np
import stim
import pytest

from leaky.transition import Transition, TransitionTable
from leaky.simulator import ReadoutStrategy, Simulator


def test_do_noiseless_instructions():
    simulator = Simulator(4)
    simulator.do("X", [0, 2])
    simulator.measure([0, 1, 2, 3])
    assert simulator.current_measurement_record() == [1, 0, 1, 0]
    simulator.do("CNOT", [0, 1, 2, 3])
    simulator.measure([0, 1, 2, 3])
    assert simulator.current_measurement_record() == [1, 0, 1, 0, 1, 1, 1, 1]
    simulator.reset([0, 1, 2, 3])
    simulator.measure([0, 1, 2, 3])
    assert simulator.current_measurement_record() == [1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0]


def test_do_noiseless_circuit():
    circuit = stim.Circuit.from_file("src/tests/test_data/surface_code.stim")
    simulator = Simulator(circuit.num_qubits)
    simulator.do_circuit(circuit)
    measurement_record = np.array([simulator.current_measurement_record()], dtype=np.bool_)
    m2d_converter = circuit.compile_m2d_converter()
    detectors, obs_flips = m2d_converter.convert(measurements=measurement_record, separate_observables=True)
    assert np.all(detectors == 0)
    assert np.all(obs_flips == 0)


@pytest.mark.parametrize(
    "gate",
    ["MX", "MY", "RX", "RY", "MR", "MRX", "MRZ", "MRY", "MPP"],
)
def test_raise_not_supported_error(gate):
    simulator = Simulator(1)
    with pytest.raises(ValueError, match="Only Z basis"):
        simulator.do(gate, [0])
        
        
def test_single_qubit_pauli_transition():
    tables = {
        "H": TransitionTable(
            {
                (0,): [Transition((0,), (0,), 1.0, 3)],
            }
        )
    }
    simulator = Simulator(1, tables)
    simulator.do("H", [0])
    simulator.do("H", [0], add_potential_noise=False)
    simulator.measure([0])
    assert simulator.current_measurement_record() == [1]


def test_single_qubit_leakage_transition():
    tables = {
        "H": TransitionTable(
            {
                (0,): [Transition((0,), (1,), 1.0)],
                (1,): [Transition((1,), (2,), 1.0)],
                (2,): [Transition((2,), (0,), 1.0)],
            }
        )
    }
    simulator = Simulator(1, tables)
    simulator.do("H", [0])
    simulator.measure([0])
    assert simulator.current_measurement_record() == [2]
    assert simulator.current_measurement_record(ReadoutStrategy.DETERMINISTIC_LEAKAGE_PROJECTION) == [1]

    simulator.do("X", [0])
    simulator.measure([0])
    assert simulator.current_measurement_record()[1] == 2

    simulator.do("H", [0])
    simulator.measure([0])
    assert simulator.current_measurement_record()[2] == 3

    simulator.do("H", [0])
    simulator.measure([0])
    assert simulator.current_measurement_record()[3] in [0, 1]

    simulator.reset([0])
    simulator.measure([0])
    assert simulator.current_measurement_record()[4] == 0


def test_two_qubit_pauli_transition():
    tables = {
        "CZ": TransitionTable(
            {
                (0, 0): [Transition((0, 0), (0, 0), 1.0, 5)],
            }
        )
    }
    simulator = Simulator(2, tables)
    simulator.do("CZ", [0, 1])
    simulator.measure([0, 1])
    assert simulator.current_measurement_record() == [1, 1]


def test_two_qubit_leakage_transition():
    tables = {
        "CZ": TransitionTable(
            {
                (0, 0): [Transition((0, 0), (0, 1), 1.0, 1)],
                (0, 1): [Transition((0, 1), (0, 2), 1.0, 1)],
                (0, 2): [Transition((0, 2), (1, 2), 1.0)],
                (1, 2): [Transition((1, 2), (0, 0), 1.0)],
            }
        )
    }
    simulator = Simulator(2, tables)
    simulator.do("CZ", [0, 1])
    simulator.measure([0, 1])
    assert simulator.current_measurement_record() == [1, 2]
    
    simulator.do("CZ", [0, 1])
    simulator.measure([0, 1])
    assert simulator.current_measurement_record()[2:] == [0, 3]
    
    simulator.do("CZ", [0, 1])
    simulator.measure([0, 1])
    assert simulator.current_measurement_record()[4:] == [2, 3]
    
    simulator.do("CZ", [0, 1])
    simulator.measure([0, 1])
    assert all(m in [0, 1] for m in simulator.current_measurement_record()[6:])
    