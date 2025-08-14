"""Leaky: An implementation of Google's Pauli+ simulator based on `stim`."""

# (This is a stubs file describing the classes and methods in leaky.)
from __future__ import annotations

import enum
from typing import Iterable, Iterator, Sequence, Optional, TYPE_CHECKING, Union

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

class LeakageStatus:
    """A vector container that holds the leakage status of a list of qubits.

    The integer 0 represents the computational space, and the integers greater than 0
    represent the leakage space.
    """
    def __init__(
        self, num_qubits: int | None = None, status: list[int] | None = None
    ) -> None:
        """Initialize a `leaky.LeakageStatus`.

        Args:
            num_qubits: The number of qubits this status holds. Either this or
                `status` must be provided. If this is provided, the status will
                be initialized to all 0s (computational space). If both are provided,
                the length of `status` must match `num_qubits`.
            status: A list of integers representing the leakage status of each qubit.
                Either this or `num_qubits` must be provided. If both are provided,
                the length of `status` must match `num_qubits`.

        Examples:
            >>> import leaky
            >>> status1 = leaky.LeakageStatus(num_qubits=2)
            >>> status2 = leaky.LeakageStatus(status=[0, 2])
        """
        ...

    @property
    def num_qubits(self) -> int:
        """The number of qubits this status holds."""
        ...

    def set(self, qubit: int, status: int) -> None:
        """Set the leakage status of a qubit.

        Args:
            qubit: The index of the qubit to set.
            status: The leakage status to set for the qubit.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
        """
        ...

    def reset(self, qubit: int) -> None:
        """Reset the leakage status of a qubit to the computational space.

        Args:
            qubit: The index of the qubit to reset.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> status.reset(0)
        """
        ...

    def get(self, qubit: int) -> int:
        """Get the leakage status of a qubit.

        Args:
            qubit: The index of the qubit to get the status of.

        Returns:
            The leakage status of the qubit.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> status.get(0)
            1
        """
        ...

    def clear(self) -> None:
        """Clear the leakage status of all qubits, resetting them to the computational space.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> status.clear()
            >>> status.get(0)
            0
        """
        ...

    def is_leaked(self, qubit: int) -> bool:
        """Check if a qubit is in the leakage space.

        Args:
            qubit: The index of the qubit to check.

        Returns:
            True if the qubit is in the leakage space, False otherwise.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> status.is_leaked(0)
            True
            >>> status.is_leaked(1)
            False
        """
        ...

    def any_leaked(self) -> bool:
        """Check if any qubit is in the leakage space.

        Returns:
            True if any qubit is in the leakage space, False otherwise.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> status.any_leaked()
            True
            >>> status.clear()
            >>> status.any_leaked()
            False
        """
        ...

    def __str__(self) -> str:
        """Get a string representation of the leakage status.

        Returns:
            A string representation of the leakage status.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> str(status)
            '|C⟩|2⟩'
        """
        ...

    def __eq__(self, other: object) -> bool:
        """Check if two leakage statuses are equal.

        Args:
            other: The other leakage status to compare with.

        Returns:
            True if the two statuses are equal, False otherwise.

        Examples:
            >>> import leaky
            >>> status1 = leaky.LeakageStatus(num_qubits=2)
            >>> status2 = leaky.LeakageStatus(num_qubits=2)
            >>> status1.set(0, 1)
            >>> status1 == status2
            False
            >>> status2.set(0, 1)
            >>> status1 == status2
            True
        """
        ...

    def __len__(self) -> int:
        """Get the number of qubits in the leakage status.

        Returns:
            The number of qubits in the leakage status.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> len(status)
            2
        """
        ...

    def __iter__(self) -> Iterator[int]:
        """Iterate over the leakage status of each qubit.

        Returns:
            An iterable of the leakage status of each qubit.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(num_qubits=2)
            >>> status.set(0, 1)
            >>> list(status)
            [1, 0]
        """
        ...

    @property
    def data(self) -> list[int]:
        """Get the raw data of the leakage status.

        Returns:
            A list representing the leakage status of each qubit.

        Examples:
            >>> import leaky
            >>> status = leaky.LeakageStatus(status=[0, 1])
            >>> status.data
            [0, 1]
        """
        ...

class Transition:
    """Transition into a leakage status with a associated pauli operator."""
    @property
    def to_status(self) -> LeakageStatus:
        """The leakage status this transition ends to."""
        ...

    @property
    def pauli_operator(self) -> str:
        """The Pauli operator associated with this transition."""
        ...

class LeakyPauliChannel:
    """A generalized Pauli channel incorporating incoherent leakage transitions."""
    def __init__(self, num_qubits: int) -> None:
        """Initialize a `leaky.LeakyPauliChannel`.

        Args:
            num_qubits: The number of qubits this channel acts on.

        Examples:
            >>> import leaky
            >>> channel = leaky.LeakyPauliChannel(num_qubits=2)
        """
        ...

    @property
    def num_transitions(self) -> int:
        """The number of different types of transitions in the channel."""
        ...

    def add_transition(
        self,
        from_status: LeakageStatus,
        to_status: LeakageStatus,
        pauli_operator: str,
        probability: float,
    ) -> None:
        """Add a transition to the channel.

        Args:
            from_status: The leakage status of the transition starting from.
            to_status: The leakage status of the transition ending to.
            pauli_operator: A string representing the Pauli operator associated
                with the transition.
            probability: The probability of the transition.

        Examples:
            >>> from leaky import LeakyPauliChannel, LeakageStatus
            >>> channel = LeakyPauliChannel(2)
            >>> channel.add_transition(LeakageStatus(2), LeakageStatus(2), "XI", 1.0)
        """
        ...

    def get_prob_from_to(
        self, from_status: LeakageStatus, to_status: LeakageStatus, pauli_operator: str
    ) -> float:
        """Get the transition probability from an initial status to a final status with the
        specified pauli operator.

        Args:
            from_status: The leakage status the transition starts from.
            to_status: The leakage status the transition ends to.
            pauli_operator: The Pauli operator associated with the transition.

        Returns:
            The probability of the transition.

        Examples:
            >>> from leaky import LeakyPauliChannel, LeakageStatus
            >>> channel = LeakyPauliChannel(2)
            >>> channel.add_transition(LeakageStatus(2), LeakageStatus(2), "XI", 1.0)
            >>> channel.get_prob_from_to(LeakageStatus(2), LeakageStatus(2), "XI")
            1.0
        """
        ...

    def sample(self, initial_status: LeakageStatus) -> Optional[Transition]:
        """Sample a transition from an initial status.

        Args:
            initial_status: The initial status of the qubit(s).

        Returns:
            A `Transition` object representing the sampled transition or None if
            no transition is available from the initial status.

        Examples:
            >>> from leaky import LeakyPauliChannel, LeakageStatus
            >>> channel = LeakyPauliChannel(2)
            >>> channel.add_transition(LeakageStatus(2), LeakageStatus(2), "XI", 1.0)
            >>> transition = channel.sample(LeakageStatus(2))
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
            >>> channel = leaky.LeakyPauliChannel(2)
            >>> channel.add_transition(LeakageStatus(2), LeakageStatus(2), "XI", 1.0)
            >>> channel.safety_check()
        """
        ...

    def __str__(self) -> str:
        """The readable string representation of the channel."""
        ...

class ReadoutStrategy(enum.Enum):
    """The strategy for readout simulating results."""

    RawLabel = "RawLabel"
    RandomLeakageProjection = "RandomLeakageProjection"
    DeterministicLeakageProjection = "DeterministicLeakageProjection"

class Simulator:
    """A simulator for stabilizer quantum circuits with incoherent leakage transitions."""
    def __init__(
        self,
        num_qubits: int,
        leaky_channels: list[LeakyPauliChannel] | None = None,
        *,
        seed: Optional[int] = None,
    ) -> None:
        """Initialize a simulator with the given number of qubits.

        Args:
            num_qubits: The number of qubits in the simulator.
            leaky_channels: A list of `leaky.LeakyPauliChannel` objects to bind
                to the simulator. These channels will be applied to the simulator
                when calling `do_gate` or `do_circuit` and there is a leaky
                instruction which is in the form of `I[leaky<n>] q0 q1 ...` where
                `leaky<n>` is the special leaky tag, `n` is the index of the
                channel in the list.
            seed: The random seed to use for the simulator.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(2)
        """
        ...

    def do_circuit(self, circuit: "stim.Circuit") -> None:
        """Apply a circuit to the simulator.

        Args:
            circuit: The `stim.Circuit` to simulate. Incoherent leakage transitions
                should be added to the circuit in the form of `I[leaky<n>] q0 q1 ...`
                where `leaky<n>` is the special leaky tag, `n` is the index of
                the channel in the list `leaky_channels` passed to the simulator
                constructor.
        """
        ...

    def do_gate(
        self,
        name: str,
        targets: Iterable[Union[int, stim.GateTarget]],
        args: Iterable[float] = (),
        tag: str = "",
    ) -> None:
        """Apply an instruction to the simulator.

        Args:
            instruction: The stim circuit instruction to simulate.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do_gate("H", [0])
        """
        ...

    def apply_leaky_channel(
        self,
        targets: Iterable[Union[int, stim.GateTarget]],
        channel: "leaky.LeakyPauliChannel",
    ) -> None:
        """Simulate a quantum channel with incoherent leakage transitions.

        Args:
            targets: The objects operated on by the gate. This is an iterable of multiple
                targets to broadcast the gate over. Each target can be an integer (a qubit),
                a stim.GateTarget, or a special target from one of the `stim.target_*`
                methods (such as a measurement record target like `rec[-1]` from
                `stim.target_rec(-1)`). The targets will be grouped into groups
                that have the same number of qubits as the channel expects.
            channel: The leaky channel to apply.
        """
        ...

    def clear(self) -> None:
        """Clear the simulator's state.

        Examples:
            >>> import leaky
            >>> simulator = leaky.Simulator(1)
            >>> simulator.do("X", [0])
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
            >>> simulator.do_gate("M", [0])
            >>> simulator.current_measurement_record()
            array([0], dtype=uint8)
        """
        ...

    def sample(
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

    @property
    def leaky_channels(self) -> list[LeakyPauliChannel]:
        """Get the list of leaky channels bound to the simulator.

        Returns:
            A list of `leaky.LeakyPauliChannel` objects.
        """
        ...

    @property
    def leakage_status(self) -> "leaky.LeakageStatus":
        """Get the current leakage status of the simulator.

        Returns:
            A `leaky.LeakageStatus` object representing the current leakage status.
        """
        ...

    @property
    def leakage_masks_record(self) -> list[int]:
        """Get the leakage masks record of the simulator."""
        ...

def generalized_pauli_twirling(
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
