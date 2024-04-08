import leaky
import stim
import pytest


def test_leaky_pauli_channel():
    channel = leaky.LeakyPauliChannel(is_single_qubit_channel=True)
    channel.add_transition(0, 1, 0, 1.0)
    channel.safety_check()
    assert channel.num_transitions == 1
    with pytest.raises(
        RuntimeError,
        match="The sum of probabilities for each initial status should be 1, but get ",
    ):
        channel.add_transition(1, 2, 0, 0.5)
        channel.safety_check()
    with pytest.raises(
        RuntimeError,
        match="The attached pauli of transitions for the qubits in D/U/L should be I",
    ):
        channel.add_transition(1, 1, 1, 0.5)
        channel.safety_check()


def test_simulator_noiseless_bell_states():
    s = leaky.Simulator(4)
    s.do(leaky.Instruction("R", [0, 1, 2, 3]))
    s.do(leaky.Instruction("H", [0, 2]))
    s.do(leaky.Instruction("CNOT", [0, 1, 2, 3]))
    s.do(leaky.Instruction("M", [0, 1, 2, 3]))
    record = s.current_measurement_record()
    assert record[0] ^ record[1] == 0
    assert record[2] ^ record[3] == 0


def test_simulator_do_noiseless_bell_circuit():
    circuit = stim.Circuit(
        """R 0 1 2 3
H 0 2
CNOT 0 1 2 3
M 0 1 2 3"""
    )
    s = leaky.Simulator(4)
    s.do_circuit(circuit)
    record = s.current_measurement_record()
    assert record[0] ^ record[1] == 0
    assert record[2] ^ record[3] == 0


def test_simulator_do_leaky_channel():
    s = leaky.Simulator(4)
    channel_2q = leaky.LeakyPauliChannel(is_single_qubit_channel=False)
    channel_2q.add_transition(0x00, 0x10, 1, 1.0)
    s.do(leaky.Instruction("CZ", [0, 1, 2, 3]))
    s.apply_2q_leaky_pauli_channel([0, 1, 2, 3], channel_2q)
    s.do(leaky.Instruction("M", [0, 1, 2, 3]))
    assert s.current_measurement_record().tolist() == [2, 1, 2, 1]

    channel_1q = leaky.LeakyPauliChannel(is_single_qubit_channel=True)
    channel_1q.add_transition(1, 0, 0, 1.0)
    s.do(leaky.Instruction("H", [0, 2]))
    s.apply_1q_leaky_pauli_channel([0, 2], channel_1q)
    s.do(leaky.Instruction("M", [0, 1, 2, 3]))
    assert s.current_measurement_record().tolist()[-4:][0] in [0, 1]
    assert s.current_measurement_record().tolist()[-4:][2] in [0, 1]

    s.clear()
    assert s.current_measurement_record().size == 0


def test_simulator_bind_leaky_channel():
    s = leaky.Simulator(4)
    channel_1q_1 = leaky.LeakyPauliChannel(is_single_qubit_channel=True)
    channel_1q_1.add_transition(1, 0, 0, 1.0)
    channel_1q_2 = leaky.LeakyPauliChannel(is_single_qubit_channel=True)
    channel_1q_2.add_transition(1, 2, 0, 1.0)
    channel_2q = leaky.LeakyPauliChannel(is_single_qubit_channel=False)
    channel_2q.add_transition(0x00, 0x10, 1, 1.0)
    s.bind_leaky_channel(leaky.Instruction("H", [0]), channel_1q_1)
    s.bind_leaky_channel(leaky.Instruction("H", [2]), channel_1q_2)
    s.bind_leaky_channel(leaky.Instruction("CNOT", [0, 1]), channel_2q)
    s.bind_leaky_channel(leaky.Instruction("CNOT", [2, 3]), channel_2q)
    s.do_circuit(stim.Circuit("X 0 2\nCNOT 0 1 2 3\nM 0 1 2 3"))
    assert len(s.bound_leaky_channels) == 4
    assert s.current_measurement_record().tolist() == [2, 0, 2, 0]
    s.do(leaky.Instruction("H", [0, 2]))
    s.do(leaky.Instruction("M", [0, 1, 2, 3]))
    assert s.current_measurement_record().tolist()[-4] in [0, 1]
    assert s.current_measurement_record().tolist()[-3:] == [0, 3, 0]

    s.clear(clear_bound_channels=True)
    assert len(s.bound_leaky_channels) == 0
    s.do_circuit(stim.Circuit("X 0 2\nCNOT 0 1 2 3\nM 0 1 2 3"))
    assert s.current_measurement_record().tolist() == [1, 1, 1, 1]
