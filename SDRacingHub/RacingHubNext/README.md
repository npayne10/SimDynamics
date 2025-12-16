# RacingHub Next

A clean starter project that re-implements the SDRacingHub concept from scratch with a lightweight, VS Code–friendly layout. The goal is to provide a modern baseline that can ingest telemetry from SimTools and stream actuator commands to the existing 4-actuator Arduino sketch.

## Project layout

```
RacingHubNext/
  src/
    RacingHub.Core       // Core models and motion cueing logic
    RacingHub.SimTools   // UDP reader for SimTools Game Engine output
    RacingHub.Arduino    // Serial publisher that matches the SimTools Arduino sketch (<255><fl><fr><rl><rr>)
    RacingHub.App        // Console host wiring everything together
```

Each project targets **.NET 8** with nullable reference types enabled and is suitable for editing/debugging inside Visual Studio Code.

## Getting started

1. Install the .NET 8 SDK (required to build/run the sample).
2. Open the `RacingHubNext` folder in VS Code.
3. Restore and build:
   ```bash
   dotnet restore RacingHubNext.sln
   dotnet build RacingHubNext.sln
   ```
4. Connect the Arduino running the 4-actuator SimTools sketch (listening for packets starting with `255`).
5. Start SimTools Game Engine UDP output on port **4123** with CSV payload `surge,sway,heave,pitch,roll,yaw` normalized to `-1..1`.
6. Run the host:
   ```bash
   dotnet run --project src/RacingHub.App/RacingHub.App.csproj
   ```

The console host will read telemetry, map it through a simple motion profile, and push four bytes to the Arduino after a `255` start byte.

## Configuration notes

- **Serial port**: update `SerialActuatorPublisher` in `src/RacingHub.App/Program.cs` with the correct COM/tty name.
- **Motion profile**: tweak the values in the `MotionProfile` object to increase/decrease how aggressively the rig responds to pitch/roll/heave cues.
- **Packet format**: `SimToolsTelemetryClient` currently expects comma-delimited values. Extend `ParseSimToolsBuffer` if you prefer a binary profile.

## Why a fresh project?

The original SDRacingHub solution contains many game-specific plugins and legacy dependencies. This new folder acts as a clean repository you can iterate on quickly in VS Code while still honoring the SimTools-to-Arduino workflow already proven on your 4-actuator rig.
