#ifndef LEAKY_RAND_GEN_H
#define LEAKY_RAND_GEN_H

#include <random>

namespace leaky {
std::mt19937_64 &global_urng();

void randomize();

/**
 * @brief Set the seed for the mt19937_64 random number generator
 *
 * @param s
 */
void set_seed(unsigned seed);

/**
 * @brief A random double chosen uniformly at random between `from` and `to`
 *
 * @param from
 * @param to
 * @return double
 */
double rand_float(double from, double to);
} // namespace leaky

#endif // LEAKY_RAND_GEN_H