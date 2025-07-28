#ifndef LEAKY_SIMULATOR_H
#define LEAKY_SIMULATOR_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/readout_strategy.h"
#include "stim.h"

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
    void do_gate(const stim::CircuitInstruction& inst, bool look_up_bound_channels = true);
    void do_circuit(const stim::Circuit& circuit);
    void clear(bool clear_bound_channels = false);
    std::vector<uint8_t> current_measurement_record(ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);
    void append_measurement_record_into(
        uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);

   private:
    void handle_transition(
        uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, std::string_view pauli);
};

}  // namespace leaky

#endif  // LEAKY_SIMULATOR_H
