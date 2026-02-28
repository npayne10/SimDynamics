from __future__ import annotations

from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, Iterable, List
import json


@dataclass
class ActuatorConfig:
    """Per-actuator configuration.

    Attributes:
        name: Friendly identifier used in logs/UI.
        home_height_mm: Neutral height in millimeters.
        max_travel_mm: Total travel allowed from the neutral position.
        max_speed_mm_s: Max commanded speed in mm/s.
        invert: Whether to flip direction when mapping axes.
    """

    name: str
    home_height_mm: int = 0
    max_travel_mm: int = 200
    max_speed_mm_s: int = 12000
    invert: bool = False


@dataclass
class AxisMix:
    """Mapping of game axes into actuator outputs."""

    heave: float = 1.0
    surge: float = 0.0
    sway: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0


@dataclass
class ProfileConfig:
    """Overall configuration for a rig profile."""

    name: str
    actuators: List[ActuatorConfig] = field(default_factory=list)
    axis_mix: AxisMix = field(default_factory=AxisMix)
    smoothing: float = 0.3
    serial_port: str | None = None
    serial_baud: int = 115200

    def save(self, path: Path) -> None:
        payload = asdict(self)
        payload["actuators"] = [asdict(a) for a in self.actuators]
        payload["axis_mix"] = asdict(self.axis_mix)
        path.write_text(json.dumps(payload, indent=2))

    @staticmethod
    def load(path: Path) -> "ProfileConfig":
        data = json.loads(path.read_text())
        actuators = [ActuatorConfig(**item) for item in data.get("actuators", [])]
        axis_mix = AxisMix(**data.get("axis_mix", {}))
        return ProfileConfig(
            name=data["name"],
            actuators=actuators,
            axis_mix=axis_mix,
            smoothing=data.get("smoothing", 0.3),
            serial_port=data.get("serial_port"),
            serial_baud=data.get("serial_baud", 115200),
        )

    def ensure_actuator_count(self, expected: int = 4) -> None:
        if len(self.actuators) != expected:
            raise ValueError(f"Profile requires exactly {expected} actuators; found {len(self.actuators)}")


@dataclass
class TelemetryFrame:
    """Normalized motion telemetry from a game interface."""

    heave: float = 0.0
    surge: float = 0.0
    sway: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0

    @staticmethod
    def blended(frames: Iterable["TelemetryFrame"]) -> "TelemetryFrame":
        total = TelemetryFrame()
        count = 0
        for frame in frames:
            total.heave += frame.heave
            total.surge += frame.surge
            total.sway += frame.sway
            total.roll += frame.roll
            total.pitch += frame.pitch
            total.yaw += frame.yaw
            count += 1
        if count == 0:
            return total
        total.heave /= count
        total.surge /= count
        total.sway /= count
        total.roll /= count
        total.pitch /= count
        total.yaw /= count
        return total


@dataclass
class ActuatorTargets:
    """Positions for all actuators in millimeters."""

    positions: Dict[str, float]

    def clamp(self, configs: List[ActuatorConfig]) -> "ActuatorTargets":
        clamped = {}
        for cfg in configs:
            pos = self.positions.get(cfg.name, cfg.home_height_mm)
            min_pos = cfg.home_height_mm - cfg.max_travel_mm / 2
            max_pos = cfg.home_height_mm + cfg.max_travel_mm / 2
            clamped[cfg.name] = min(max(pos, min_pos), max_pos)
        return ActuatorTargets(positions=clamped)
