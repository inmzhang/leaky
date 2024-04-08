#ifndef LEAKY_SIMULATOR_H
#define LEAKY_SIMULATOR_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/readout_strategy.h"
#include "stim.h"
#include "stim/circuit/circuit_instruction.h"
#include "stim/circuit/gate_target.h"

namespace leaky {

struct Simulator {
    uint32_t num_qubits;
    std::vector<uint8_t> leakage_status;
    std::vector<uint8_t> leakage_masks_record;
    stim::TableauSimulator<stim::MAX_BITWORD_WIDTH> tableau_simulator;
    std::unordered_map<size_t, LeakyPauliChannel> bound_leaky_channels;

    explicit Simulator(uint32_t num_qubits);

    void bind_leaky_channel(const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel);
    void apply_1q_leaky_pauli_channel(stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel);
    void apply_2q_leaky_pauli_channel(stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel);
    void do_gate(const stim::CircuitInstruction& inst);
    void do_measurement(const stim::CircuitInstruction& inst);
    void do_reset(const stim::CircuitInstruction& inst);
    void do_circuit(const stim::Circuit& circuit);
    void clear(bool clear_bound_channels = false);
    std::vector<uint8_t> current_measurement_record(ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);
    void append_measurement_record_into(
        uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);

   private:
    bool all_target_is_in_r(stim::SpanRef<const stim::GateTarget> targets, bool is_single_target);
    void handle_u_or_d(uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target);
};

}  // namespace leaky

#endif  // LEAKY_SIMULATOR_H