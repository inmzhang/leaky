#include "leaky/core/status.h"

#include <sstream>

leaky::LeakageStatus::LeakageStatus(size_t num_qubits) : num_qubits(num_qubits), s(num_qubits, 0) {
}

void leaky::LeakageStatus::set(size_t qubit, uint8_t status) {
    s[qubit] = status;
}

void leaky::LeakageStatus::reset(size_t qubit) {
    s[qubit] = 0;
}

void leaky::LeakageStatus::clear() {
    std::fill(s.begin(), s.end(), 0);
}

uint8_t leaky::LeakageStatus::get(size_t qubit) const {
    return s[qubit];
}

bool leaky::LeakageStatus::is_leaked(size_t qubit) const {
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
