#include "leaky/core/simulator.h"

#include "gtest/gtest.h"

#include "leaky/core/readout_strategy.h"
#include "stim/circuit/circuit.h"

using namespace leaky;

static std::vector<stim::GateTarget> qubit_targets(const std::vector<uint32_t>& targets) {
    std::vector<stim::GateTarget> result;
    for (uint32_t t : targets) {
        result.push_back(stim::GateTarget::qubit(t & ~stim::TARGET_INVERTED_BIT, t & stim::TARGET_INVERTED_BIT));
    }
    return result;
}
struct OpDat {
    stim::GateType gate;
    std::vector<stim::GateTarget> targets;
    OpDat(const char* gate, uint32_t u) : gate(stim::GATE_DATA.at(gate).id), targets(qubit_targets({u})) {
    }
    OpDat(const char* gate, std::vector<uint32_t> u) : gate(stim::GATE_DATA.at(gate).id), targets(qubit_targets(u)) {
    }
    operator stim::CircuitInstruction() const {
        return {gate, {}, targets};
    }
};

TEST(simulator, construct) {
    Simulator sim(1);
    ASSERT_EQ(sim.num_qubits, 1);
    ASSERT_EQ(sim.leakage_status.size(), 1);
    ASSERT_EQ(sim.leakage_masks_record.size(), 0);
}

TEST(simulator, do_simple_circuit_without_leak1) {
    Simulator sim(1);
    sim.do_gate(OpDat("X", 0));
    sim.do_gate(OpDat("M", 0));
    ASSERT_EQ(sim.current_measurement_record().back(), 1);
    auto circuit = stim::Circuit();
    circuit.append_from_text("X 0\nM 0");
    sim.do_circuit(circuit);
    ASSERT_EQ(sim.current_measurement_record().back(), 0);
}

TEST(simulator, do_simple_circuit_without_leak2) {
    Simulator sim(2);
    sim.do_reset(OpDat("R", {0, 1}));
    sim.do_measurement(OpDat("M", {0, 1}));
    sim.do_gate(OpDat("H", 0));
    sim.do_gate(OpDat("CNOT", {0, 1}));
    sim.do_measurement(OpDat("M", {0, 1}));
    auto result = sim.current_measurement_record();
    ASSERT_EQ(result.size(), 4);
    ASSERT_EQ(result[0], 0);
    ASSERT_EQ(result[1], 0);
    ASSERT_EQ(result[2] ^ result[3], 0);
}

TEST(simulator, do_surface_circuit_without_leak) {
    auto params = stim::CircuitGenParameters(11, 11, "rotated_memory_z");
    auto circuit = stim::generate_surface_code_circuit(params).circuit;
    Simulator sim(circuit.count_qubits());
    sim.do_circuit(circuit);
    auto result = sim.current_measurement_record();
    ASSERT_EQ(result.size(), circuit.count_measurements());
}

TEST(simulator, circuit_target_exceed_num_qubits) {
    Simulator sim(2);
    auto circuit = stim::Circuit();
    circuit.append_from_text("H 2");
    ASSERT_THROW(sim.do_circuit(circuit), std::invalid_argument);
}

TEST(simulator, do_1q_leaky_channel) {
    Simulator sim(1);
    LeakyPauliChannel channel(true);
    channel.add_transition(0, 0, 1, 0.5);
    channel.add_transition(0, 1, 0, 0.5);
    for (auto i = 0; i < 1000; i++) {
        sim.do_1q_leaky_pauli_channel(OpDat("X", 0), channel);
        sim.do_measurement(OpDat("M", 0));
        ASSERT_TRUE(sim.leakage_masks_record[0] == 0 || sim.leakage_masks_record[0] == 1);
        auto result = sim.current_measurement_record();
        ASSERT_TRUE(result[0] == 0 || result[0] == 2);
        sim.clear();
    }
}

TEST(simulator, readout_strategy) {
    Simulator sim(1);
    LeakyPauliChannel channel(true);
    channel.add_transition(0, 2, 0, 1);
    sim.do_1q_leaky_pauli_channel(OpDat("X", 0), channel);
    sim.do_measurement(OpDat("M", 0));
    ASSERT_EQ(sim.current_measurement_record()[0], 3);
    ASSERT_EQ(sim.current_measurement_record(ReadoutStrategy::DeterministicLeakageProjection)[0], 1);
    auto random_result = sim.current_measurement_record(ReadoutStrategy::RandomLeakageProjection)[0];
    ASSERT_TRUE(random_result == 0 || random_result == 1);
}

TEST(simulator, do_2q_leaky_channel) {
    Simulator sim(2);
    LeakyPauliChannel channel(true);
    channel.add_transition(0x00, 0x01, 4, 1);
    sim.do_2q_leaky_pauli_channel(OpDat("CZ", {0, 1}), channel);
    sim.do_measurement(OpDat("M", {0, 1}));
    ASSERT_TRUE(sim.leakage_masks_record[0] == 0);
    ASSERT_TRUE(sim.leakage_masks_record[1] == 1);
    auto result = sim.current_measurement_record();
    ASSERT_TRUE(result[0] == 1);
    ASSERT_TRUE(result[1] == 2);
}

TEST(simulator, leaked_qubit_trans_down) {
    auto counts = std::map<uint8_t, uint64_t>();
    Simulator sim(1);
    LeakyPauliChannel channel(true);
    channel.add_transition(0, 1, 0, 1);
    channel.add_transition(1, 0, 0, 1);
    for (auto i = 0; i < 1000; i++) {
        sim.do_1q_leaky_pauli_channel(OpDat("X", 0), channel);
        sim.do_1q_leaky_pauli_channel(OpDat("X", 0), channel);
        sim.do_measurement(OpDat("M", 0));
        counts[sim.current_measurement_record()[0]]++;
        sim.clear();
    }
    ASSERT_TRUE(400 < counts[0] && counts[0] < 600);
    ASSERT_TRUE(400 < counts[1] && counts[1] < 600);
}

TEST(simulator, qubit_leaked_up) {
    auto counts = std::map<uint8_t, uint64_t>();
    Simulator sim(2);
    LeakyPauliChannel channel(true);
    channel.add_transition(0, 1, 0, 1);
    for (auto i = 0; i < 1000; i++) {
        sim.do_gate(OpDat("H", 0));
        sim.do_gate(OpDat("CNOT", {0, 1}));
        sim.do_1q_leaky_pauli_channel(OpDat("I", 1), channel);
        sim.do_measurement(OpDat("M", 0));
        counts[sim.current_measurement_record()[0]]++;
        sim.clear();
    }
    ASSERT_TRUE(400 < counts[0] && counts[0] < 600);
    ASSERT_TRUE(400 < counts[1] && counts[1] < 600);
}

TEST(simulator, bind_leaky_channel) {
    Simulator sim(1);
    LeakyPauliChannel channel(true);
    channel.add_transition(0, 1, 0, 1);
    sim.bind_leaky_channel(OpDat("X", 0), channel);
    sim.do_gate(OpDat("X", 0));
    sim.do_measurement(OpDat("M", 0));
    ASSERT_TRUE(sim.leakage_masks_record[0] == 1);
    ASSERT_TRUE(sim.current_measurement_record()[0] == 2);
}