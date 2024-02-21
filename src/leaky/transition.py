import dataclasses

import numpy as np

LeakageStatus = tuple[int, ...]


@dataclasses.dataclass(frozen=True)
class Transition:
    initial_status: LeakageStatus
    final_status: LeakageStatus
    probability: float
    pauli_channel_idx: int | None = None


@dataclasses.dataclass(frozen=True)
class TransitionTable:
    transitions: dict[LeakageStatus, list[Transition]]
    rng: np.random.Generator = dataclasses.field(default_factory=np.random.default_rng)

    def get_transition_prob(
        self, initial_status: LeakageStatus, final_status: LeakageStatus, pauli_channel_idx: int | None
    ) -> float:
        for t in self.transitions[initial_status]:
            if t.final_status == final_status and t.pauli_channel_idx == pauli_channel_idx:
                return t.probability
        return 0.0

    def sample(self, initial_status: LeakageStatus) -> Transition:
        transitions = self.transitions[initial_status]
        probabilities = [t.probability for t in transitions]
        return self.rng.choice(transitions, p=probabilities)
