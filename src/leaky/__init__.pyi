"""Leaky: An implementation of Google's Pauli+ simulator based on `stim`."""

# (This is a stubs file describing the classes and methods in leaky.)
from __future__ import annotations

import enum
from typing import Iterable, Sequence, Tuple, Optional, TYPE_CHECKING, Union

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

    def get_prob_from_to(
        self, initial_status: int, final_status: int, pauli_idx: int
    ) -> float:
        """Get the transition probability from an initial status to a final status with the
        specified pauli channel index.

        Args:
            initial_status: The initial status of the qubit(s).
            final_status: The final status of the qubit(s).
            pauli_idx: The index of the Pauli channel.

        Returns:
            The probability of the transition.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 0.5)
            >>> channel.get_transitions_from_to(0, 1, 0)
            0.5
            >>> channel.get_transitions_from_to(0, 1, 1)
            0.0
        """
        ...

    def sample(self, initial_status: int) -> Optional[Tuple[int, int]]:
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

class Instruction:
    """An instruction almost the same as `leaky.Instruction`.

    This is a simplified re-implemented since `stim.PyCircuitInstruction` is not exposed
    by `libstim` C++ API.
    """
    def __init__(
        self,
        gate_name: str,
        targets: Iterable[Union[int, stim.GateTarget]],
        arg: Iterable[float] = (),
    ) -> None:
        """Initialize an `leaky.Instruction`.

        Args:
            name: The name of the operation's gate (e.g. "H" or "M" or "CNOT").
            targets: The objects operated on by the gate. This isan iterable of multiple
                targets to broadcast the gate over. Each target can be an integer (a qubit),
                a stim.GateTarget, or a special target from one of the `stim.target_*`
                methods (such as a measurement record target like `rec[-1]` from
                `stim.target_rec(-1)`).
            arg: The "parens arguments" for the gate, such as the probability for a
                noise operation. A list of doubles parameterizing the gate. Different
                gates take different parens arguments. For example, X_ERROR takes a
                probability, OBSERVABLE_INCLUDE takes an observable index, and
                PAULI_CHANNEL_1 takes three disjoint probabilities.

        Examples:
            >>> import leaky
            >>> instruction1 = leaky.Instruction("X", [0])
            >>> instruction2 = leaky.Instruction("CNOT", [0, 1])
            >>> instruction3 = leaky.Instruction("DEPOLARIZE1", [0], [0.1])
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
            circuit: The `stim.Circuit` to apply. Note that this method should
                only be used for small-shots simulations. Since the simulator
                look up the bound channels for each instruction, it is not
                efficient for large repeated simulations. For large repeated
                simulations, it is recommended to use the `sample_batch` method.

        Examples:
            >>> import stim
            >>> import leaky
            >>> circuit = stim.Circuit()
            >>> circuit.append_operation(leaky.Instruction("X", [0]))
            >>> circuit.append_operation(leaky.Instruction("M", [0]))
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do_circuit(circuit)
        """
        ...

    def do(
        self,
        instruction: "leaky.Instruction",
        loop_up_bound_channels: bool = True,
    ) -> None:
        """Apply an instruction to the simulator.

        Args:
            instruction: The `leaky.Instruction` to apply.
            loop_up_bound_channels: Whether to look up and apply the bound channels
                when applying the instruction. Default is True.

        Examples:
            >>> import leaky
            >>> instruction = leaky.Instruction("X", [0])
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do(instruction)
        """
        ...

    def apply_1q_leaky_pauli_channel(
        self,
        targets: Iterable[Union[int, stim.GateTarget]],
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Apply a single qubit leaky Pauli channel to a circuit instruction.

        Args:
            targets: The objects operated on by the gate. This isan iterable of multiple
                targets to broadcast the gate over. Each target can be an integer (a qubit),
                a stim.GateTarget, or a special target from one of the `stim.target_*`
                methods (such as a measurement record target like `rec[-1]` from
                `stim.target_rec(-1)`).
            channel: The leaky channel to apply.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel()
            >>> channel.add_transition(0, 1, 0, 1.0)
            >>> simulator = leaky.Simulator(1)
            >>> simulator.apply_1q_leaky_pauli_channel([0, 1], channel)
        """
        ...

    def apply_2q_leaky_pauli_channel(
        self,
        targets: "leaky.Instruction",
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Apply a two qubit leaky Pauli channel to a circuit instruction.

        Args:
            targets: The objects operated on by the gate. This isan iterable of multiple
                targets to broadcast the gate over. Each target can be an integer (a qubit),
                a stim.GateTarget, or a special target from one of the `stim.target_*`
                methods (such as a measurement record target like `rec[-1]` from
                `stim.target_rec(-1)`).
            channel: The leaky channel to apply.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel(is_single_qubit_channel=False)
            >>> channel.add_transition(0, 1, 0, 1.0)
            >>> simulator = leaky.Simulator(2)
            >>> simulator.do_2q_leaky_pauli_channel([0, 1, 2, 3], channel)
        """
        ...

    def bind_leaky_channel(
        self,
        ideal_inst: "leaky.Instruction",
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Bind a leaky channel to a circuit instruction.

        A bound channel will be applied to the simulator whenever the bound
        instruction is applied.

        Args:
            ideal_inst: The ideal circuit instruction to bind the channel to.
            channel: The leaky channel to bind.

        Examples:
            >>> import leaky
            >>> instruction = leaky.Instruction("X", [0])
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
            >>> simulator.do(leaky.Instruction("X", [0]))
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
            >>> simulator.do(leaky.Instruction("M", [0]))
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

def decompose_kraus_operators_to_leaky_pauli_channel(
    kraus_operators: Sequence[np.ndarray],
    num_qubits: int,
    num_level: int,
    safety_check: bool = True,
) -> LeakyPauliChannel:
    """Decompose the Kraus operators into a leaky pauli channel representation with
    Generalized Pauli Twirling(GPT).

    Args:
        kraus_operators: A sequence of Kraus operators corresponding to an operation's error channel.
        num_qubits: The number of qubits in the operation.
        num_level: The number of levels of the quantum system to be considered.
        safety_check: If True, perform a safety check to ensure the channel is valid.
            A channel is valid if the sum of the probabilities of all transitions
            from a given initial status is 1. And the pauli channel related to the
            qubits with transition type that not in R(stay in the computational space)
            should always be I. Default is True.

    Returns:
        A LeakyPauliChannel object representing the error channel.
    """
    ...

def leakage_status_tuple_to_int(status: Tuple[int, ...]) -> int:
    """Convert a leakage status tuple to an integer representation.

    Args:
        status: A tuple of leakage status. Currently, only support up to two
            qubits.

    Returns:
        An integer representation of the leakage status.
    """
    ...
