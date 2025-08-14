#ifndef LEAKY_SIMULATOR_H
#define LEAKY_SIMULATOR_H

#include <cstdint>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/readout_strategy.h"
#include "leaky/core/status.h"
#include "stim.h"

namespace leaky {

struct Simulator {
    uint32_t num_qubits;
    LeakageStatus leakage_status;
    std::vector<uint8_t> leakage_masks_record;
    stim::TableauSimulator<stim::MAX_BITWORD_WIDTH> tableau_simulator;
    std::vector<LeakyPauliChannel> leaky_channels;

    explicit Simulator(uint32_t num_qubits, std::vector<LeakyPauliChannel> leaky_channels = {});

    void apply_leaky_channel(stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel);
    void do_gate(const stim::CircuitInstruction& inst);
    void do_circuit(const stim::Circuit& circuit);
    void clear();
    std::vector<uint8_t> current_measurement_record(ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);
    void append_measurement_record_into(
        uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);

   private:
    void handle_transition(
        uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, std::string_view pauli);
};

}  // namespace leaky

#endif  // LEAKY_SIMULATOR_H
