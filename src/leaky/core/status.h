#ifndef LEAKY_STATUS_H
#define LEAKY_STATUS_H

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace leaky {

struct LeakageStatus {
    /// The number of qubits in the system.
    size_t num_qubits;
    /// The current leakage status of each qubit.
    std::vector<uint8_t> s;

    explicit LeakageStatus(size_t num_qubits = 0);
    void set(size_t qubit, uint8_t status);
    void reset(size_t qubit);
    void clear();
    [[nodiscard]] uint8_t get(size_t qubit) const;
    [[nodiscard]] bool is_leaked(size_t qubit) const;
    [[nodiscard]] bool any_leaked() const;

    [[nodiscard]] size_t size() const {
        return num_qubits;
    }
    [[nodiscard]] size_t count() const {
        return num_qubits;
    }
    [[nodiscard]] std::string str() const;
    bool operator==(const LeakageStatus &other) const;
    bool operator!=(const LeakageStatus &other) const;
};

std::ostream &operator<<(std::ostream &out, const LeakageStatus &status);
}  // namespace leaky

#endif  // LEAKY_STATUS_H
