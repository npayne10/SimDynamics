from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict

from ..config import ActuatorConfig, AxisMix, ProfileConfig
from ..game_interfaces.assetto_corsa import AssettoCorsaUDP
from ..game_interfaces.assetto_corsa_competizione import AssettoCorsaCompetizioneUDP
from ..game_interfaces.rfactor2 import RFactor2UDP
from ..serial_link import SerialLink
from ..telemetry_router import TelemetryRouter


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="dynamichub", description="Motion rig router")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="Run with a selected game interface")
    run.add_argument("--game", choices=["ac", "acc", "rf2"], required=True)
    run.add_argument("--port", required=True, help="Serial port to Arduino Mega")
    run.add_argument("--baud", type=int, default=115200)
    run.add_argument("--config", type=Path, help="Path to saved profile JSON")

    conf = subparsers.add_parser("actuators", help="Manage actuator settings")
    conf_sub = conf.add_subparsers(dest="subcommand", required=True)

    configure = conf_sub.add_parser("configure", help="Create or update a profile")
    configure.add_argument("--name", required=True)
    configure.add_argument("--height", type=int, default=500, help="Home height in mm")
    configure.add_argument("--max-speed", type=int, default=12000)
    configure.add_argument("--max-travel", type=int, default=200)
    configure.add_argument("--invert", action="store_true")
    configure.add_argument("--port", required=True)
    configure.add_argument("--baud", type=int, default=115200)
    configure.add_argument("--output", type=Path, default=Path("profile.json"))

    return parser


def profile_from_args(args: argparse.Namespace) -> ProfileConfig:
    actuators = [
        ActuatorConfig(name=f"actuator_{idx+1}", home_height_mm=args.height, max_travel_mm=args.max_travel, max_speed_mm_s=args.max_speed, invert=args.invert)
        for idx in range(4)
    ]
    axis_mix = AxisMix()
    return ProfileConfig(
        name=args.name,
        actuators=actuators,
        axis_mix=axis_mix,
        serial_port=args.port,
        serial_baud=args.baud,
    )


def select_interface(code: str):
    mapping: Dict[str, object] = {
        "ac": AssettoCorsaUDP,
        "acc": AssettoCorsaCompetizioneUDP,
        "rf2": RFactor2UDP,
    }
    return mapping[code]()


def handle_run(args: argparse.Namespace) -> None:
    if args.config:
        profile = ProfileConfig.load(args.config)
    else:
        dummy_args = argparse.Namespace(
            name="runtime", height=500, max_travel=200, max_speed=12000, invert=False, port=args.port, baud=args.baud
        )
        profile = profile_from_args(dummy_args)
    profile.serial_port = args.port
    profile.serial_baud = args.baud
    interface = select_interface(args.game)
    router = TelemetryRouter(profile, interface)
    serial_link = SerialLink(profile)
    serial_link.stream(router.target_stream())


def handle_configure(args: argparse.Namespace) -> None:
    profile = profile_from_args(args)
    profile.save(args.output)
    print(f"Saved profile to {args.output}")


def main(argv: list[str] | None = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.command == "run":
        handle_run(args)
    elif args.command == "actuators" and args.subcommand == "configure":
        handle_configure(args)


if __name__ == "__main__":
    main()
