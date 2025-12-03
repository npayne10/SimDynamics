from __future__ import annotations

from collections import deque
from typing import Deque, Iterable, Iterator, List

from .config import ActuatorTargets, ProfileConfig, TelemetryFrame
from .game_interfaces.base import GameInterface


class TelemetryRouter:
    """Blends game telemetry into actuator positions."""

    def __init__(self, profile: ProfileConfig, interface: GameInterface) -> None:
        self.profile = profile
        self.interface = interface
        self.history: Deque[TelemetryFrame] = deque(maxlen=10)

    def target_stream(self) -> Iterator[ActuatorTargets]:
        self.profile.ensure_actuator_count()
        for frame in self.interface.frames():
            smoothed = self._smooth(frame)
            yield self._map_to_actuators(smoothed)

    def _smooth(self, frame: TelemetryFrame) -> TelemetryFrame:
        if self.profile.smoothing <= 0:
            return frame
        self.history.append(frame)
        blend_count = max(1, int(len(self.history) * self.profile.smoothing))
        recent: Iterable[TelemetryFrame] = list(self.history)[-blend_count:]
        return TelemetryFrame.blended(recent)

    def _map_to_actuators(self, frame: TelemetryFrame) -> ActuatorTargets:
        axis = self.profile.axis_mix
        positions: List[float] = []
        for idx, actuator in enumerate(self.profile.actuators):
            sway_factor = -axis.sway if actuator.invert else axis.sway
            surge_factor = -axis.surge if actuator.invert else axis.surge
            roll_factor = axis.roll if idx % 2 == 0 else -axis.roll
            pitch_factor = axis.pitch if idx < 2 else -axis.pitch
            yaw_factor = axis.yaw if idx in (0, 3) else -axis.yaw

            mixed = (
                axis.heave * frame.heave
                + sway_factor * frame.sway
                + surge_factor * frame.surge
                + roll_factor * frame.roll
                + pitch_factor * frame.pitch
                + yaw_factor * frame.yaw
            )
            positions.append(actuator.home_height_mm + mixed)
        targets = ActuatorTargets(positions={cfg.name: pos for cfg, pos in zip(self.profile.actuators, positions)})
        return targets.clamp(self.profile.actuators)
