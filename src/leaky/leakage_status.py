"""Leakage status vector."""
import numpy as np

class LeakageStatus:
    """A vector of leakage status labels."""
    def __init__(self, num_qubits: int) -> None:
        self._vector = np.zeros(num_qubits, dtype=int)
