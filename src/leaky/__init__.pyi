"""Leaky: An implementation of Google's Pauli+ simulator based on `stim`."""
# (This is a stubs file describing the classes and methods in leaky.)
from __future__ import annotations

import enum
from typing import Tuple, Optional, TYPE_CHECKING

import numpy as np
import numpy.typing as npt

if TYPE_CHECKING:
    import leaky
    import stim

def randomize() -> None:
    """
    Choose a random seed using `std::random_device`.

    Examples:
        >>> import leaky
        >>> leaky.randomize()
    """
    ...

def set_seed(seed: int) -> None:
    """
    Set the random seed used by the simulator.

    Args:
        seed: The seed to use.

    Examples:
        >>> import leaky
        >>> leaky.set_seed(12345)
    """
    ...

def rand_float(begin: float, end: float) -> float:
    """
    Generate a floating point number chosen uniformly at random
    over the interval between `begin` and `end`

    Args:
        begin:
            Smallest float that can be drawn from the distribution
        end:
            Largest float that can be drawn from the distribution

    Returns:
        The random float

    Examples:
        >>> import leaky
        >>> leaky.rand_float(0.0, 1.0)
    """
    ...

class LeakyPauliChannel:
    """A generalized Pauli channel incorporating incoherent leakage transitions."""
    def __init__(self, is_single_qubit_channel: bool = True) -> None:
        """Initialize a `leaky.LeakyPauliChannel`.

        Args:
            is_single_qubit_channel: Whether the channel is single-qubit or two-qubit.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
        """
        ...

    @property
    def num_transitions(self) -> int:
        """The number of transitions in the channel."""
        ...

    def add_transition(
        self,
        initial_status: int,
        final_status: int,
        pauli_channel_idx: int,
        probability: float,
    ) -> None:
        """Add a transition to the channel.

        Args:
            initial_status: The initial status of the qubit(s). If the channel is
            single-qubit,
                this is a single status represented by a uint8. If the channel is two-qubit,
                this is a pair of status, which is a uint8 concatenated by two 4-bit status.
            final_status: The final status of the qubit(s). If the channel is single-qubit,
                this is a single status represented by a uint8. If the channel is two-qubit,
                this is a pair of status, which is a uint8 concatenated by two 4-bit status.
            pauli_channel_idx: The index of the Pauli channel. For single qubit channels, this
                is the index of the Pauli in the order [I, X, Y, Z]. For two-qubit channels, this
                is the index of the Pauli in the order [II, IX, IY, IZ, XI, XX, XY, XZ, YI, YX, YY,
                YZ, ZI, ZX, ZY, ZZ].
            probability: The probability of the transition.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> print(channel)
            Transitions:
                |C> --I--> |2>: 0.5,
        """
        ...

    def get_transitions_from_to(
        self,
        initial_status: int,
        final_status: int,
    ) -> Optional[Tuple[Tuple[int, int], float]]:
        """Get the transitions from an initial status to a final status.

        Args:
            initial_status: The initial status of the qubit(s).
            final_status: The final status of the qubit(s).

        Returns:
            A pair of transition and probability if the transition exists, otherwise None.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.get_transitions_from_to(0, 1)
            ((1, 0), 0.5)
        """
        ...

    def sample(self, initial_status: int) -> Tuple[int, int]:
        """Sample a transition from an initial status.

        Args:
            initial_status: The initial status of the qubit(s).

        Returns:
            A transition which is a tuple of (final_status, pauli_channel_idx).

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 0, 1, 0.5)
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.sample(0)
            (1, 0)
        """
        ...

    def safety_check(self) -> None:
        """Check if the channel is well-defined.

        1. the sum of the transition probabilities from the same initial status
        to different final status should be 1.

        2. the pauli channel related to the qubits with transition type that not
        in R(stay in the computational space) should always be I.

        Raises:
            RuntimeError: If the channel is not well-defined.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.safety_check()
            RuntimeError: The sum of probabilities for each initial status should be 1
        """
        ...

    def __str__(self) -> str:
        """The readable string representation of the channel.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.add_transition(0, 0, 1, 0.5)
            >>> print(channel)
            Transitions:
                |C> --I--> |2>: 0.5,
                |C> --X--> |C>: 0.5,
        """
        ...

    def __repr__(self) -> str:
        """The compact string representation of the channel.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.add_transition(0, 0, 1, 0.5)
            >>> channel
            LeakyPauliChannel(is_single_qubit_channel=true, with 2 transitions attached)
        """
        ...

class ReadoutStrategy(enum.Enum):
    """The strategy for readout simulating results."""

    RawLabel: int
    RandomLeakageProjection: int
    DeterministicLeakageProjection: int

class Simulator:
    """A simulator for stabilizer quantum circuits with incoherent leakage transitions."""
    def __init__(
        self,
        num_qubits: int,
        *,
        seed: Optional[int] = None,
    ) -> None:
        """Initialize a simulator with the given number of qubits.

        Args:
            num_qubits: The number of qubits in the simulator.
            seed: The random seed to use for the simulator.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(2)
        """
        ...

    def do_circuit(self, circuit: "stim.Circuit") -> None:
        """Apply a circuit to the simulator.

        Args:
            circuit: The `stim.Circuit` to apply.

        Examples:
            >>> import stim
            >>> import leaky
            >>> circuit = stim.Circuit()
            >>> circuit.append_operation(stim.CircuitInstruction("X", [0]))
            >>> circuit.append_operation(stim.CircuitInstruction("M", [0]))
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do_circuit(circuit)
        """
        ...

    def do(self, instruction: "stim.CircuitInstruction") -> None:
        """Apply an instruction to the simulator.

        Args:
            instruction: The `stim.CircuitInstruction` to apply.

        Examples:
            >>> import stim
            >>> import leaky
            >>> instruction = stim.CircuitInstruction("X", [0])
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do(instruction)
        """
        ...

    def do_1q_leaky_pauli_channel(
        self,
        ideal_inst: "stim.CircuitInstruction",
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Apply a single qubit leaky Pauli channel to a circuit instruction.

        Args:
            ideal_inst: The ideal circuit instruction to apply the channel to.
            channel: The leaky channel to apply.

        Examples:
            >>> import stim
            >>> import leaky
            >>> instruction = stim.CircuitInstruction("X", [0])
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 1.0)
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do_1q_leaky_pauli_channel(instruction, channel)
        """
        ...

    def do_2q_leaky_pauli_channel(
        self,
        ideal_inst: "stim.CircuitInstruction",
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Apply a two qubit leaky Pauli channel to a circuit instruction.

        Args:
            ideal_inst: The ideal circuit instruction to apply the channel to.
            channel: The leaky channel to apply.

        Examples:
            >>> import stim
            >>> import leaky
            >>> instruction = stim.CircuitInstruction("CNOT", [0, 1])
            >>> channel = leaky.LeakyPauliChannel(is_single_qubit_channel=False)
            >>> channel.add_transition(0, 1, 0, 1.0)
            >>> simulator = leaky.Simulator(2)
            >>> simulator.do_2q_leaky_pauli_channel(instruction, channel)
        """
        ...

    def bind_leaky_channel(
        self,
        ideal_inst: "stim.CircuitInstruction",
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Bind a leaky channel to a circuit instruction.

        A bound channel will be applied to the simulator whenever the bound
        instruction is applied.

        Args:
            ideal_inst: The ideal circuit instruction to bind the channel to.
            channel: The leaky channel to bind.

        Examples:
            >>> import stim
            >>> import leaky
            >>> instruction = stim.CircuitInstruction("X", [0])
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 1.0)
            >>> simulator = leaky.Simulator(1)
            >>> simulator.bind_leaky_channel(instruction, channel)
        """
        ...

    def clear(self, clear_bound_channels: bool = False) -> None:
        """Clear the simulator's state.

        Args:
            clear_bound_channels: Whether to also clear the bound leaky channels.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do(stim.CircuitInstruction("X", [0]))
            >>> simulator.clear()
        """
        ...

    def current_measurement_record(
        self,
        readout_strategy: "leaky.ReadoutStrategy" = ReadoutStrategy.RawLabel,
    ) -> npt.NDArray[np.uint8]:
        """Get the current measurement record.

        Args:
            readout_strategy: The strategy for readout simulating results.

        Returns:
            The measurement record.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do(stim.CircuitInstruction("M", [0]))
            >>> simulator.current_measurement_record()
            array([0], dtype=uint8)
        """
        ...

    def sample_batch(
        self,
        circuit: "stim.Circuit",
        shots: int,
        readout_strategy: "leaky.ReadoutStrategy" = ReadoutStrategy.RawLabel,
    ) -> npt.NDArray[np.uint8]:
        """Batch sample the measurement results of a circuit.

        Args:
            circuit: The circuit to sample.
            shots: The number of shots.
            readout_strategy: The readout strategy to use.

        Returns:
            A numpy array of measurement results with `dtype=uint8`. The shape of the array
            is `(shots, circuit.num_measurements)`.
        """
        ...