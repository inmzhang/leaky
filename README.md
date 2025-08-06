# leaky

[![ci](https://github.com/inmzhang/leaky/actions/workflows/ci.yml/badge.svg)](https://github.com/inmzhang/leaky/actions/workflows/ci.yml)

> WARNING: This is a work in progress and there will be no gaurentee of backward compatibility until the first stable release.

An implementation of Google's Pauli+ simulator. It uses `stim.TableauSimulator` internally for stabilizer
simulation and adds support for leakage errors.

## Installation

### From PyPI

```bash
pip install leakysim
```

### Built from source

```bash
git clone https://github.com/inmzhang/leaky.git
cd leaky
pip install .
```

## Basic usage

```python
import numpy as np
import leaky
import stim

# Quantum channel that can includes leakage transitions
channel_2q = leaky.LeakyPauliChannel(2)
# Qubits order is in least-significant bit order, the first qubit is the leftmost
# state in the Dirac notation
# leakage state 0 represents computational space, nth state represent n+1 leakage space
s1, s2 = leaky.LeakageStatus(2), leaky.LeakageStatus(2)
s2.set(1, 1) # the second qubit will leak into the first leakage space (|2⟩)
channel_2q.add_transition(s1, s2, "XI", 1.0) # |C⟩|C⟩ -> |C⟩|2⟩ with associated Pauli operator X on the first qubit
s = leaky.Simulator(4, [channel_2q])

# The leakage channel can be annotated in the circuit with a special tag `leaky<n>`
# attached to the `I` instruction, where `n` represents the nth channel bound to the
# simulator during initialization. The channel will be broadcast to the targets
# of the `I` instruction based on the channel dimensions.
circuit = stim.Circuit("""
R 0 1 2 3
X 0 2
CNOT 0 1 2 3
I[leaky<0>] 0 1 2 3
M 0 1 2 3
""")
s.do_circuit(circuit)
assert len(s.leaky_channels) == 3
assert s.current_measurement_record().tolist() == [0, 2, 0, 2]

# Another way to apply the leakage channel is by calling `apply_leaky_channel` method
s.apply_leaky_channel([1, 2], channel_2q)

# A higher dimension leakage channel (`four_qubit_channel = LeakyPauliChannel(4)`) can be applied by either:
# 1. include `I[leaky<n>] q0 q1 q2 q3` in the circuit and call `do_circuit`
# 2. simulator.apply_leaky_channel([q0, q1, q2, q3], four_qubit_channel)
```
