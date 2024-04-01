#ifndef LEAKY_SIMULATOR_H
#define LEAKY_SIMULATOR_H

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/readout_strategy.h"
#include "stim.h"

namespace leaky {

typedef std::tuple<stim::GateType, stim::SpanRef<const double>, stim::SpanRef<const stim::GateTarget>> inst_id;

struct Simulator {
    uint32_t num_qubits;
    std::vector<uint8_t> leakage_status;
    std::vector<uint8_t> leakage_masks_record;
    std::map<inst_id, const LeakyPauliChannel&> binded_leaky_channels;
    stim::TableauSimulator<stim::MAX_BITWORD_WIDTH> tableau_simulator;

    Simulator(uint32_t num_qubits, std::map<inst_id, const LeakyPauliChannel&> binded_leaky_channels = {});

    void bind_leaky_channel(const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel);
    void do_1q_leaky_pauli_channel(const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel);
    void do_2q_leaky_pauli_channel(const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel);
    void do_gate(const stim::CircuitInstruction& inst);
    void do_circuit(const stim::Circuit& circuit);
    void do_measurement(const stim::CircuitInstruction& inst);
    void do_reset(const stim::CircuitInstruction& inst);
    void clear(bool clear_binded_channels = false);
    std::vector<uint8_t> current_measurement_record(ReadoutStrategy readout_strategy = ReadoutStrategy::RawLabel);

   private:
    void handle_u_or_d(uint8_t cur_status, uint8_t next_status, stim::GateTarget target);
};

}  // namespace leaky

#endif  // LEAKY_SIMULATOR_H