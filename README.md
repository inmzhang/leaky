# leaky

[![ci](https://github.com/inmzhang/leaky/actions/workflows/ci.yml/badge.svg)](https://github.com/inmzhang/leaky/actions/workflows/ci.yml)

Leakage-aware stabilizer simulation built on top of `stim.TableauSimulator`.

## Install

From PyPI:

```bash
pip install leakysim
```

From source:

```bash
git clone https://github.com/inmzhang/leaky.git
cd leaky
pip install .
```

## Quick Example

```python
import leaky
import stim

channel = leaky.LeakyPauliChannel(2)
channel.add_transition(
    leaky.LeakageStatus(status=[0, 0]),
    leaky.LeakageStatus(status=[0, 1]),
    "XI",
    1.0,
)

sim = leaky.Simulator(4, [channel], seed=1234)
circuit = stim.Circuit("""
R 0 1 2 3
X 0 2
CNOT 0 1 2 3
I[leaky<0>] 0 1 2 3
M 0 1 2 3
""")

sim.do_circuit(circuit)
print(sim.current_measurement_record().tolist())  # [0, 2, 0, 2]
```

## Main Python API

- `LeakageStatus`: per-qubit leakage labels. `0` means computational space.
- `LeakyPauliChannel`: leakage transitions plus Pauli corrections on qubits that stay computational.
- `Simulator`: executes `stim.Circuit` objects while tracking leakage.
- `ReadoutStrategy`: controls how leaked measurement results are reported.
- `generalized_pauli_twirling(...)`: converts Kraus operators into a `LeakyPauliChannel`.

## Notes

- Use `I[leaky<n>] ...` in a Stim circuit to apply the `n`th channel bound to a simulator.
- `Simulator(seed=...)` only seeds that simulator instance.
- `apply_leaky_channel(...)` expects raw qubit targets and broadcasts in groups of `channel.num_qubits`.
- For `ReadoutStrategy.RawLabel`, leaked measurements are reported as `2`, `3`, ... instead of projected bits.

## API Reference

The shipped stub file is the canonical Python API reference:

- [`src/leaky/__init__.pyi`](src/leaky/__init__.pyi)
