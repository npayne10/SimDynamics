namespace RacingHub.Core.Models;

/// <summary>
/// Represents the physics data we need to drive the platform.
/// All values are normalized between -1 and 1 for the core pipeline.
/// </summary>
public sealed class TelemetrySample
{
    public double Surge { get; init; }
    public double Sway { get; init; }
    public double Heave { get; init; }
    public double Pitch { get; init; }
    public double Roll { get; init; }
    public double Yaw { get; init; }
}
