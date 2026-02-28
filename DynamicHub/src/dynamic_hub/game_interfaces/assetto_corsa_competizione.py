from __future__ import annotations

import socket
from typing import Iterator

from .base import GameInterface
from ..config import TelemetryFrame


class AssettoCorsaCompetizioneUDP(GameInterface):
    """ACC UDP listener using the public broadcast format."""

    name = "acc"

    def __init__(self, port: int = 9999) -> None:
        self.port = port

    def frames(self) -> Iterator[TelemetryFrame]:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        while True:
            data, _ = sock.recvfrom(2048)
            yield self.parse_payload(data)

    def parse_payload(self, data: bytes) -> TelemetryFrame:
        # Simplified parsing; replace with ACC's broadcast packet schema for more fidelity.
        heave = float(int.from_bytes(data[:2], "little", signed=True) / 32767)
        surge = float(int.from_bytes(data[2:4], "little", signed=True) / 32767)
        roll = float(int.from_bytes(data[4:6], "little", signed=True) / 32767)
        pitch = float(int.from_bytes(data[6:8], "little", signed=True) / 32767)
        return TelemetryFrame(heave=heave, surge=surge, roll=roll, pitch=pitch)

    def describe_connection(self) -> str:
        return f"ACC UDP broadcast on port {self.port}"
