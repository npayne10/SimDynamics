from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Iterator

from ..config import TelemetryFrame


class GameInterface(ABC):
    """Abstract source of telemetry frames."""

    name: str

    @abstractmethod
    def frames(self) -> Iterator[TelemetryFrame]:
        """Yield normalized telemetry frames for the active session."""

    @abstractmethod
    def describe_connection(self) -> str:
        """Human-readable information on how the game feed is connected."""
