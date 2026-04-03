#ifndef LEAKY_SIMULATOR_H
#define LEAKY_SIMULATOR_H

#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
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
    std::mt19937_64 leakage_rng;
    stim::TableauSimulator<stim::MAX_BITWORD_WIDTH> tableau_simulator;
    std::vector<LeakyPauliChannel> leaky_channels;

    explicit Simulator(
        uint32_t num_qubits,
        std::vector<LeakyPauliChannel> leaky_channels = {},
        std::optional<uint64_t> seed = std::nullopt);

    void apply_leaky_channel(stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel);
    void do_gate(const stim::CircuitInstruction& inst);
    void do_circuit(const stim::Circuit& circuit);
    void clear();
    std::vector<uint8_t> current_measurement_record(ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);
    void append_measurement_record_into(
        uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);
    void sample_into(
        const stim::Circuit& circuit,
        size_t shots,
        uint8_t* results_begin_ptr,
        ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);

   private:
    void append_result_masks(const stim::CircuitInstruction& inst);
    [[nodiscard]] uint8_t compute_group_leakage_mask(stim::SpanRef<const stim::GateTarget> group) const;
    void do_circuit_internal(const stim::Circuit& circuit, bool validate_capacity);
    void handle_transition(uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, char pauli);
};

}  // namespace leaky

#endif  // LEAKY_SIMULATOR_H
