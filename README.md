# leaky

[![ci](https://github.com/inmzhang/leaky/actions/workflows/ci.yml/badge.svg)](https://github.com/inmzhang/leaky/actions/workflows/ci.yml)

> WARNING: This is a work in progress and is not yet ready for use, there will be breaking changes.

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
import leaky

channel = leaky.LeakyPauliChannel()
channel.add_transition(
    initial_status=0, # Computational space
    final_status=1, # |2> leakage status
    pauli_channel_idx=0,
    probability=1.0,
)
simulator = leaky.Simulator(num_qubits=1)
simulator.do_1q_leaky_pauli_channel(leaky.Instruction('X', [0]), channel)
simulator.do(leaky.Instruction('M', [0]))
assert simulator.current_measurement_record() == [2]
```