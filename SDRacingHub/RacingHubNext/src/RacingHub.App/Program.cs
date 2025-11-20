using RacingHub.Arduino;
using RacingHub.Core.Interfaces;
using RacingHub.Core.Models;
using RacingHub.Core.Services;
using RacingHub.SimTools;

var cancellationSource = new CancellationTokenSource();
Console.CancelKeyPress += (_, args) =>
{
    args.Cancel = true;
    cancellationSource.Cancel();
};

var actuators = new List<ActuatorConfig>
{
    new() { Name = "FrontLeft", Channel = 0 },
    new() { Name = "FrontRight", Channel = 1 },
    new() { Name = "RearLeft", Channel = 2 },
    new() { Name = "RearRight", Channel = 3 },
};

var profile = new MotionProfile
{
    HeaveScale = 0.6,
    PitchScale = 0.5,
    RollScale = 0.5
};

var cueing = new MotionCueingService(profile, actuators);

Console.WriteLine("Starting RacingHub Next prototype. Press Ctrl+C to exit.");
Console.WriteLine("Waiting for SimTools UDP telemetry on port 4123 and streaming to Arduino.");

await using IGameTelemetrySource telemetry = new SimToolsTelemetryClient();
await using IActuatorCommandPublisher publisher = new SerialActuatorPublisher(portName: "/dev/ttyUSB0");

while (!cancellationSource.IsCancellationRequested)
{
    var sample = await telemetry.ReadAsync(cancellationSource.Token);
    if (sample is null)
    {
        continue;
    }

    var command = cueing.ToCommand(sample);
    await publisher.PublishAsync(command, cancellationSource.Token);
}

Console.WriteLine("Stopped RacingHub Next prototype.");
