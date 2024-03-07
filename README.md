# leaky

> WIP: The project is in the early stages of development and is not well tested. The documentation is also incomplete.

An unoptimized(slow) implementation of Google's Pauli+ simulator

## Installation

```bash
git clone https://github.com/inmzhang/leaky.git
cd leaky
pip install -e .
```

If you want to run the tests, you can install the test dependencies with

```bash
pip install pytest pytest-cov
```

and then run the tests with

```bash
pytest --cov=src/leaky src/tests
```

## Basic usage

```python
import leaky

trans_collect = TransitionCollection()
trans_collect.add_transition_table(
    "H",
    TransitionTable(
        {
            (0,): [Transition((0,), (1,), 1.0)],
            (1,): [Transition((1,), (2,), 1.0)],
            (2,): [Transition((2,), (0,), 1.0)],
        }
    ),
)
simulator = Simulator(1, trans_collect)
simulator.do("H", [0])
simulator.measure([0])
assert simulator.current_measurement_record() == [2]
```