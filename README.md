# leaky

[![ci](https://github.com/inmzhang/leaky/actions/workflows/ci.yml/badge.svg)](https://github.com/inmzhang/leaky/actions/workflows/ci.yml)

> WARNING: This is a work in progress and there will be no gaurentee of backward compatibility until the first stable release.

An implementation of Google's Pauli+ simulator. It uses `stim.TableauSimulator` internally for stabilizer
simulation and adds support for leakage errors.

## Python API At A Glance

`leaky` exposes a small Python API centered around four public concepts:

- `LeakageStatus`: an indexed container describing whether each qubit is in the computational space (`0`) or in a leakage space (`1`, `2`, ...).
- `LeakyPauliChannel`: a stochastic channel over leakage-status transitions plus Pauli corrections on qubits that stay in the computational space.
- `Simulator`: a stabilizer simulator that executes `stim.Circuit` objects while tracking leakage.
- `generalized_pauli_twirling`: a utility that converts Kraus operators into a `LeakyPauliChannel`.

Top-level helpers:

- `set_seed(seed)` and `randomize()`: control module-level randomness used by `rand_float(...)` and `LeakyPauliChannel.sample(...)`.
- `rand_float(begin, end)`: draw a uniform floating-point value from `[begin, end)`.
- `ReadoutStrategy`: choose how leaked measurement results are represented.

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
import stim

# Quantum channel that can includes leakage transitions
channel_2q = leaky.LeakyPauliChannel(2)
# Qubits order is in least-significant bit order, the first qubit is the leftmost
# state in the Dirac notation
# leakage state 0 represents computational space, nth state represent n+1 leakage space
leak_from = leaky.LeakageStatus(2)
# the second qubit will leak into the first leakage space (|2⟩)
leak_to = leaky.LeakageStatus(status=[0, 1])

# |C⟩|C⟩ -> |C⟩|2⟩ with associated Pauli operator X on the first qubit
channel_2q.add_transition(leak_from, leak_to, "XI", 1.0)
s = leaky.Simulator(4, [channel_2q], seed=1234)

# The leakage channel can be annotated in the circuit with the exact tag
# `leaky<n>` attached to the `I` instruction, where `n` represents the nth
# channel bound to the simulator during initialization. The channel will be
# broadcast to the targets of the `I` instruction based on the channel
# dimensions.
circuit = stim.Circuit("""
R 0 1 2 3
X 0 2
CNOT 0 1 2 3
I[leaky<0>] 0 1 2 3
M 0 1 2 3
""")
s.do_circuit(circuit)
assert len(s.leaky_channels) == 1
assert s.current_measurement_record().tolist() == [0, 2, 0, 2]

# Another way to apply the leakage channel is by calling `apply_leaky_channel` method
# with raw qubit targets.
s.apply_leaky_channel([1, 2], channel_2q)
```

## Core Concepts

- Leakage label `0` means the qubit is in the computational space.
- Leakage label `1` corresponds to the first leakage level and is displayed as `|2⟩`.
- Leakage label `2` corresponds to the second leakage level and is displayed as `|3⟩`.
- Pauli operators attached to a transition may only act on qubits that remain in the computational space during that transition.

## Working With `LeakageStatus`

`LeakageStatus` is the basic container used throughout the API.

```python
import leaky

status = leaky.LeakageStatus(status=[0, 1, 0])
assert status.num_qubits == 3
assert status.get(1) == 1
assert status.is_leaked(1) is True
assert list(status) == [0, 1, 0]

status.reset(1)
assert status.data == [0, 0, 0]
```

Notes:

- `get`, `set`, `reset`, and `is_leaked` validate indices and raise `IndexError` if the qubit index is out of range.
- `str(status)` renders the current leakage configuration using Dirac-style labels, for example `|C⟩|2⟩`.

## Building `LeakyPauliChannel`

Channels are defined by adding transitions from one leakage configuration to another.

```python
import leaky

channel = leaky.LeakyPauliChannel(num_qubits=1)
ground = leaky.LeakageStatus(status=[0])
leaked = leaky.LeakageStatus(status=[1])

channel.add_transition(ground, ground, "I", 0.97)
channel.add_transition(ground, leaked, "I", 0.03)
channel.add_transition(leaked, ground, "I", 0.20)
channel.add_transition(leaked, leaked, "I", 0.80)
channel.safety_check()
```

Channel invariants:

- `from_status`, `to_status`, and `pauli_operator` must all match the channel width.
- Transition probabilities for a fixed `from_status` must sum to `1.0`.
- A non-identity Pauli may only appear on qubits that remain in the computational space during the transition.

Useful methods:

- `channel.num_transitions`
- `channel.get_prob_from_to(from_status, to_status, pauli_operator)`
- `channel.sample(initial_status)`
- `channel.safety_check()`

## Running Simulations

You can execute individual gates or whole Stim circuits.

```python
import leaky
import stim

channel = leaky.LeakyPauliChannel(1)
channel.add_transition(
    leaky.LeakageStatus(status=[0]),
    leaky.LeakageStatus(status=[1]),
    "I",
    1.0,
)

sim = leaky.Simulator(2, [channel], seed=1234)
sim.do_gate("R", [0, 1])
sim.do_gate("H", [0])
sim.do_gate("I", [0], tag="leaky<0>")
sim.do_gate("M", [0, 1])
print(sim.current_measurement_record().tolist())
```

`Simulator` supports two ways to apply leaky channels:

- Annotate `stim.Circuit` instructions as `I[leaky<n>] ...`, where `n` is the index in `Simulator(..., leaky_channels=[...])`.
- Call `sim.apply_leaky_channel(targets, channel)` directly.

Target rules for `apply_leaky_channel(...)`:

- Targets must be raw qubit targets only.
- The target list is broadcast in groups of `channel.num_qubits`.

`Simulator(seed=...)` seeds only that simulator instance. It does not mutate the
module-level RNG used by helpers such as `leaky.rand_float(...)` and
`LeakyPauliChannel.sample(...)`.

## Readout Strategies

Leaked measurements can be surfaced in three different ways:

- `ReadoutStrategy.RawLabel`: leaked results are returned as leakage labels `2`, `3`, ...
- `ReadoutStrategy.RandomLeakageProjection`: leaked results are randomly projected to `0` or `1`.
- `ReadoutStrategy.DeterministicLeakageProjection`: leaked results are deterministically reported as `1`.

For multi-qubit measurements such as `MPP`, `RawLabel` uses the maximum leakage
level among the measured qubits.

## Batch Sampling

Use `Simulator.sample(...)` to repeatedly sample the same circuit:

```python
import leaky
import stim

sim = leaky.Simulator(2, seed=5)
circuit = stim.Circuit("""
R 0 1
H 0
CNOT 0 1
M 0 1
""")

shots = sim.sample(circuit, 4)
assert shots.shape == (4, 2)
```

## Twirling Utility

`generalized_pauli_twirling(...)` converts Kraus operators into a `LeakyPauliChannel`.

```python
import numpy as np
import leaky

kraus = [
    np.array([[1, 0], [0, np.sqrt(0.9)]], dtype=complex),
    np.array([[0, np.sqrt(0.1)], [0, 0]], dtype=complex),
]

channel = leaky.generalized_pauli_twirling(
    kraus_operators=kraus,
    num_qubits=1,
    num_level=2,
)
channel.safety_check()
```

Guidelines:

- Each Kraus operator should have shape `(num_level**num_qubits, num_level**num_qubits)`.
- The returned channel is suitable for `Simulator(..., leaky_channels=[...])` or `apply_leaky_channel(...)`.
- For larger systems, decomposition cost grows quickly with `num_qubits` and `num_level`.

## API References

The shipped type stub at `src/leaky/__init__.pyi` is the canonical API reference in this repository:

- https://github.com/inmzhang/leaky/blob/master/src/leaky/__init__.pyi

It documents the public classes, methods, and helper functions that are intended
to be imported from Python.
