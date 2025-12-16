namespace RacingHub.Core.Models;

/// <summary>
/// High level scaling options used to map normalized cues to actuator travel.
/// </summary>
public sealed class MotionProfile
{
    public double SurgeScale { get; init; } = 1.0;
    public double SwayScale { get; init; } = 1.0;
    public double HeaveScale { get; init; } = 1.0;
    public double PitchScale { get; init; } = 1.0;
    public double RollScale { get; init; } = 1.0;
    public double YawScale { get; init; } = 0.0;
}
