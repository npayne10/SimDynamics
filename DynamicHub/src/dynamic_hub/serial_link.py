from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable
import time

import serial

from .config import ActuatorTargets, ProfileConfig


@dataclass
class SerialLink:
    """Thin wrapper for writing actuator positions to an Arduino Mega."""

    profile: ProfileConfig

    def __post_init__(self) -> None:
        if not self.profile.serial_port:
            raise ValueError("Serial port must be set before starting the serial link")

    def connect(self) -> serial.Serial:
        return serial.Serial(self.profile.serial_port, self.profile.serial_baud, timeout=1)

    def stream(self, positions: Iterable[ActuatorTargets]) -> None:
        with self.connect() as ser:
            for tick, target in enumerate(positions):
                payload = self.format_payload(tick, target)
                ser.write(payload.encode("ascii"))
                ser.flush()
                time.sleep(0.01)  # modest pacing to avoid spamming the controller

    def format_payload(self, tick: int, target: ActuatorTargets) -> str:
        ordered = [target.positions.get(cfg.name, 0.0) for cfg in self.profile.actuators]
        values = ",".join(str(int(pos)) for pos in ordered)
        return f"{tick},{values}\n"
