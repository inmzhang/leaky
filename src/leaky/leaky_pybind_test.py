import leaky
import stim
import pytest


def ls(s: str) -> leaky.LeakageStatus:
    status = leaky.LeakageStatus(len(s))
    for i, c in enumerate(s):
        status.set(i, int(c))
    return status


def test_leaky_pauli_channel():
    channel = leaky.LeakyPauliChannel(num_qubits=1)
    channel.add_transition(ls("0"), ls("1"), "I", 1.0)
    channel.safety_check()
    assert channel.num_transitions == 1
    with pytest.raises(
        RuntimeError,
        match="The sum of probabilities for each initial status should be 1, but get ",
    ):
        channel.add_transition(ls("1"), ls("2"), "I", 0.5)
        channel.safety_check()
    with pytest.raises(
        RuntimeError,
        match="The attached pauli of transitions for the qubits in D/U/L should be I",
    ):
        channel.add_transition(ls("1"), ls("1"), "X", 0.5)
        channel.safety_check()


def test_simulator_noiseless_bell_states():
    s = leaky.Simulator(4)
    s.do_gate("R", [0, 1, 2, 3])
    s.do_gate("H", [0, 2])
    s.do_gate("CNOT", [0, 1, 2, 3])
    s.do_gate("M", [0, 1, 2, 3])
    record = s.current_measurement_record()
    assert record[0] ^ record[1] == 0
    assert record[2] ^ record[3] == 0


def test_simulator_do_noiseless_bell_circuit():
    circuit = stim.Circuit("""R 0 1 2 3
H 0 2
CNOT 0 1 2 3
M 0 1 2 3
""")
    s = leaky.Simulator(4)
    s.do_circuit(circuit)
    record = s.current_measurement_record()
    assert record[0] ^ record[1] == 0
    assert record[2] ^ record[3] == 0


def test_simulator_do_leaky_channel():
    s = leaky.Simulator(4)
    channel_2q = leaky.LeakyPauliChannel(2)
    channel_2q.add_transition(ls("00"), ls("01"), "XI", 1.0)
    # s.do_gate("CZ", [0, 1, 2, 3])
    s.apply_leaky_channel([0, 1, 2, 3], channel_2q)
    s.do_gate("M", [0, 1, 2, 3])
    assert s.current_measurement_record().tolist() == [1, 2, 1, 2]

    channel_1q = leaky.LeakyPauliChannel(1)
    channel_1q.add_transition(ls("0"), ls("0"), "I", 1.0)
    s.do_gate("H", [0, 2])
    s.apply_leaky_channel([0, 2], channel_1q)
    s.do_gate("M", [0, 1, 2, 3])
    assert s.current_measurement_record().tolist()[-4:][0] in [0, 1]
    assert s.current_measurement_record().tolist()[-4:][2] in [0, 1]

    s.clear()
    assert s.current_measurement_record().size == 0


def test_simulator_bind_leaky_channel():
    channel_1q_1 = leaky.LeakyPauliChannel(1)
    channel_1q_1.add_transition(ls("1"), ls("0"), "I", 1.0)
    channel_1q_2 = leaky.LeakyPauliChannel(1)
    channel_1q_2.add_transition(ls("1"), ls("2"), "I", 1.0)
    channel_2q = leaky.LeakyPauliChannel(2)
    channel_2q.add_transition(ls("00"), ls("01"), "XI", 1.0)
    s = leaky.Simulator(4, [channel_1q_1, channel_1q_2, channel_2q])
    circuit = stim.Circuit("""
R 0 1 2 3
X 0 2
CNOT 0 1 2 3
I[leaky<2>] 0 1 2 3
M 0 1 2 3
""")
    s.do_circuit(circuit)
    assert len(s.leaky_channels) == 3
    assert s.current_measurement_record().tolist() == [0, 2, 0, 2]
    s.do_circuit(
        stim.Circuit("""
I[leaky<0>] 1
I[leaky<1>] 3
M 0 1 2 3
""")
    )
    assert s.current_measurement_record().tolist()[-3] in [0, 1]
    assert s.current_measurement_record().tolist()[-1] == 3
    assert s.current_measurement_record().tolist()[-4] == 0
    assert s.current_measurement_record().tolist()[-2] == 0

    s.clear()
    s.do_circuit(circuit.without_tags())
    assert s.current_measurement_record().tolist() == [1, 1, 1, 1]
