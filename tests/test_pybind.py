import leaky
import pytest
import stim


def ls(s: str) -> leaky.LeakageStatus:
    return leaky.LeakageStatus(status=[int(c) for c in s])


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


def test_leaky_pauli_channel_rejects_width_mismatch():
    channel = leaky.LeakyPauliChannel(num_qubits=2)
    with pytest.raises(ValueError, match="Transition width must match"):
        channel.add_transition(leaky.LeakageStatus(1), leaky.LeakageStatus(1), "I", 1.0)


def test_leakage_status_out_of_range_raises_index_error():
    status = leaky.LeakageStatus(num_qubits=1)
    with pytest.raises(IndexError):
        status.get(1)
    with pytest.raises(IndexError):
        status.set(1, 1)
    with pytest.raises(IndexError):
        status.reset(1)
    with pytest.raises(IndexError):
        status.is_leaked(1)


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


def test_apply_leaky_channel_skips_unmapped_groups_instead_of_stopping():
    channel = leaky.LeakyPauliChannel(1)
    channel.add_transition(ls("1"), ls("0"), "I", 1.0)

    s = leaky.Simulator(3)
    s.leakage_status.set(1, 1)
    s.leakage_status.set(2, 1)
    s.apply_leaky_channel([0, 1, 2], channel)

    assert s.leakage_status.data == [0, 0, 0]


def test_apply_leaky_channel_rejects_non_qubit_targets():
    channel = leaky.LeakyPauliChannel(1)
    channel.add_transition(ls("0"), ls("0"), "I", 1.0)
    s = leaky.Simulator(1)

    with pytest.raises(ValueError, match="raw qubit target"):
        s.apply_leaky_channel([stim.target_x(0)], channel)


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


def test_invalid_leaky_tag_raises():
    channel = leaky.LeakyPauliChannel(1)
    channel.add_transition(ls("0"), ls("0"), "I", 1.0)
    s = leaky.Simulator(1, [channel])

    with pytest.raises(ValueError, match="Invalid leaky channel tag"):
        s.do_gate("I", [0], tag="leaky<0")


def test_mpp_measurements_follow_produced_result_count():
    s = leaky.Simulator(4)
    s.do_gate("R", [0, 1, 2, 3])
    s.leakage_status.set(1, 1)
    s.leakage_status.set(3, 2)

    s.do_circuit(stim.Circuit("MPP X0*X1 Y2*Z3"))

    raw_record = s.current_measurement_record()
    projected_record = s.current_measurement_record(
        leaky.ReadoutStrategy.DeterministicLeakageProjection
    )

    assert raw_record.shape == (2,)
    assert raw_record.tolist() == [2, 3]
    assert projected_record.tolist() == [1, 1]


def test_simulator_seed_is_isolated_per_instance():
    def projection_sequence(make_other: bool) -> list[int]:
        s = leaky.Simulator(8, seed=123)
        for q in range(8):
            s.leakage_status.set(q, 1)
        s.do_gate("M", list(range(8)))
        if make_other:
            _ = leaky.Simulator(1, seed=999)
        return [
            s.current_measurement_record(
                leaky.ReadoutStrategy.RandomLeakageProjection
            ).tolist()[i]
            for i in range(8)
        ]

    assert projection_sequence(False) == projection_sequence(True)


def test_simulator_sample_has_expected_shape():
    circuit = stim.Circuit("""
R 0 1
MPP X0*X1
M 0 1
""")
    s = leaky.Simulator(2, seed=5)
    shots = s.sample(circuit, 4)
    assert shots.shape == (4, 3)

