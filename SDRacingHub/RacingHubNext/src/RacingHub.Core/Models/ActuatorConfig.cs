namespace RacingHub.Core.Models;

/// <summary>
/// Represents a single physical actuator in a 4DOF motion platform.
/// </summary>
public sealed class ActuatorConfig
{
    public required string Name { get; init; }
    public required int Channel { get; init; }
    public double TravelMm { get; init; } = 100.0;
    public double MaxVelocityMmPerSec { get; init; } = 150.0;
    public double MinValue { get; init; } = 0.0;
    public double MaxValue { get; init; } = 255.0;
}
