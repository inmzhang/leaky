#ifndef LEAKY_READOUT_STRATEGY_H
#define LEAKY_READOUT_STRATEGY_H

#include <cstdint>

namespace leaky {

enum ReadoutStrategy : uint8_t {
    RawLabel,
    RandomLeakageProjection,
    DeterministicLeakageProjection,
};

}  // namespace leaky

#endif  // LEAKY_READOUT_STRATEGY_H