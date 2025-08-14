#include "leaky/core/status.pybind.h"

#include <pybind11/cast.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <vector>

#include "leaky/core/status.h"
#include "pybind11/pybind11.h"

py::class_<leaky::LeakageStatus> leaky_pybind::pybind_status(py::module &m) {
    return {m, "LeakageStatus"};
}

void leaky_pybind::pybind_status_methods(py::module &m, py::class_<leaky::LeakageStatus> &c) {
    c.def(
        py::init([](const py::object &num_qubits, const py::object &status) {
            if (!status.is_none()) {
                auto s = py::cast<std::vector<uint8_t>>(status);
                if (!num_qubits.is_none()) {
                    auto n = py::cast<size_t>(num_qubits);
                    if (s.size() != n) {
                        throw std::invalid_argument("Status vector length does not match num_qubits.");
                    }
                }
                auto leakage_status = leaky::LeakageStatus(s.size());
                for (size_t i = 0; i < s.size(); ++i) {
                    leakage_status.set(i, s[i]);
                }
                return leakage_status;
            } else if (!num_qubits.is_none()) {
                return leaky::LeakageStatus(py::cast<size_t>(num_qubits));
            }
            throw std::invalid_argument("Either num_qubits or status must be provided.");
        }),
        py::arg("num_qubits") = py::none(),
        py::arg("status") = py::none());
    c.def_readonly("num_qubits", &leaky::LeakageStatus::num_qubits);
    c.def("set", &leaky::LeakageStatus::set, py::arg("qubit"), py::arg("status"));
    c.def("reset", &leaky::LeakageStatus::reset, py::arg("qubit"));
    c.def("clear", &leaky::LeakageStatus::clear);
    c.def("get", &leaky::LeakageStatus::get, py::arg("qubit"));
    c.def("is_leaked", &leaky::LeakageStatus::is_leaked, py::arg("qubit"));
    c.def("any_leaked", &leaky::LeakageStatus::any_leaked);
    c.def("__str__", &leaky::LeakageStatus::str);
    c.def("__eq__", &leaky::LeakageStatus::operator==, py::arg("other"));
    c.def("__len__", [](leaky::LeakageStatus &self) {
        return self.num_qubits;
    });
    c.def(
        "__iter__",
        [](leaky::LeakageStatus &self) {
            return py::make_iterator(self.s.begin(), self.s.end());
        },
        py::keep_alive<0, 1>());
    c.def("to_list", [](leaky::LeakageStatus &self) {
        return std::vector<uint8_t>(self.s.begin(), self.s.end());
    });
}
