#include "leaky/core/rand_gen.h"

#include <random>

std::mt19937_64& leaky::global_urng() {
    static std::mt19937_64 u{};
    return u;
}

void leaky::randomize() {
    static std::random_device rd{};
    leaky::global_urng().seed(rd());
}

void leaky::set_seed(unsigned seed) {
    leaky::global_urng().seed(seed);
}

double leaky::rand_float(double from, double to) {
    static std::uniform_real_distribution<> d{};
    using parm_t = decltype(d)::param_type;
    return d(global_urng(), parm_t{from, to});
}