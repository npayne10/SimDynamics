from __future__ import annotations

import socket
from typing import Iterator

from .base import GameInterface
from ..config import TelemetryFrame


class RFactor2UDP(GameInterface):
    """rFactor 2 UDP listener based on the Internals Plugin broadcast."""

    name = "rf2"

    def __init__(self, port: int = 5397) -> None:
        self.port = port

    def frames(self) -> Iterator[TelemetryFrame]:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        while True:
            data, _ = sock.recvfrom(1024)
            yield self.parse_payload(data)

    def parse_payload(self, data: bytes) -> TelemetryFrame:
        # Minimal parser; align with the InternalsPluginV07 if you need more data.
        sway = float(int.from_bytes(data[:2], "little", signed=True) / 32767)
        pitch = float(int.from_bytes(data[2:4], "little", signed=True) / 32767)
        yaw = float(int.from_bytes(data[4:6], "little", signed=True) / 32767)
        return TelemetryFrame(sway=sway, pitch=pitch, yaw=yaw)

    def describe_connection(self) -> str:
        return f"rFactor 2 UDP on port {self.port}"
