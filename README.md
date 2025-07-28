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

# Assume you have a unitary repr of CNOT noise from dynamical simulation
# which is a 2**4 * 2**4 matrix, incoorporating leakage errors up to 4-th level
cnot_kraus = np.load('cnot_kraus.npy')

# Decompose the Kraus operator into pauli channels and incoherent stochastic transitions
# with Generalize Pauli decomposition
cnot_channel: leaky.LeakyPauliChannel = leaky.decompose_kraus_operators_to_leaky_pauli_channel(
    kraus_operators = cnot_kraus,
    num_qubits = 2,
    num_level = 4,
)

# Simulate a bell state preparation circuit
circuit = stim.Circuit("""R 0 1 2 3
H 0 2
CNOT 0 1 2 3
M 0 1 2 3""")

# Initialize a leaky simulator
simulator = leaky.Simulator(num_qubits=circuit.num_qubits)

# Bind the channel to the corresponding cx gates
# We only bind the channel to a single cx gate for demonstration
simulator.bind_leaky_channel(leaky.Instruction('CX', [0, 1]), cnot_channel)

# Sample the circuit
results = simulator.sample_batch(circuit, shots=50000)
```
