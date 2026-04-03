#include "leaky/core/status.h"

#include <stdexcept>
#include <sstream>

namespace {

void check_qubit_index(const leaky::LeakageStatus& status, size_t qubit) {
    if (qubit >= status.num_qubits) {
        throw std::out_of_range(
            "Qubit index " + std::to_string(qubit) + " is out of range for a leakage status with " +
            std::to_string(status.num_qubits) + " qubits.");
    }
}

}  // namespace

leaky::LeakageStatus::LeakageStatus(size_t num_qubits) : num_qubits(num_qubits), s(num_qubits, 0) {
}

void leaky::LeakageStatus::set(size_t qubit, uint8_t status) {
    check_qubit_index(*this, qubit);
    s[qubit] = status;
}

void leaky::LeakageStatus::reset(size_t qubit) {
    check_qubit_index(*this, qubit);
    s[qubit] = 0;
}

void leaky::LeakageStatus::clear() {
    std::fill(s.begin(), s.end(), 0);
}

uint8_t leaky::LeakageStatus::get(size_t qubit) const {
    check_qubit_index(*this, qubit);
    return s[qubit];
}

bool leaky::LeakageStatus::is_leaked(size_t qubit) const {
    check_qubit_index(*this, qubit);
    return s[qubit] > 0;
}

bool leaky::LeakageStatus::any_leaked() const {
    for (size_t i = 0; i < num_qubits; ++i) {
        if (s[i] > 0) {
            return true;
        }
    }
    return false;
}

bool leaky::LeakageStatus::operator==(const leaky::LeakageStatus &other) const {
    return num_qubits == other.num_qubits && s == other.s;
}

bool leaky::LeakageStatus::operator!=(const leaky::LeakageStatus &other) const {
    return !(*this == other);
}

std::string leaky::LeakageStatus::str() const {
    std::stringstream ss;
    for (size_t i = 0; i < num_qubits; ++i) {
        ss << "|" << (s[i] == 0 ? "C" : std::to_string(s[i] + 1)) << "⟩";
    }
    return ss.str();
}

std::ostream &leaky::operator<<(std::ostream &out, const leaky::LeakageStatus &status) {
    out << status.str();
    return out;
}
