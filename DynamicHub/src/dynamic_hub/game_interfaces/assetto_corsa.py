from __future__ import annotations

import socket
from typing import Iterator

from .base import GameInterface
from ..config import TelemetryFrame


class AssettoCorsaUDP(GameInterface):
    """Assetto Corsa UDP listener.

    This class models the default shared memory -> UDP bridge configuration.
    Replace `parse_payload` with your preferred packet schema.
    """

    name = "assetto_corsa"

    def __init__(self, port: int = 9996) -> None:
        self.port = port

    def frames(self) -> Iterator[TelemetryFrame]:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        while True:
            data, _ = sock.recvfrom(1024)
            yield self.parse_payload(data)

    def parse_payload(self, data: bytes) -> TelemetryFrame:
        # Placeholder implementation; replace with proper parsing of AC UDP payloads.
        heave = float(int.from_bytes(data[:2], "little", signed=True) / 32767)
        surge = float(int.from_bytes(data[2:4], "little", signed=True) / 32767)
        sway = float(int.from_bytes(data[4:6], "little", signed=True) / 32767)
        return TelemetryFrame(heave=heave, surge=surge, sway=sway)

    def describe_connection(self) -> str:
        return f"Assetto Corsa UDP on port {self.port}"
