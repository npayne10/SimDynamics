using RacingHub.Core.Interfaces;
using RacingHub.Core.Models;

namespace RacingHub.Core.Services;

public sealed class MotionCueingService
{
    private readonly MotionProfile _profile;
    private readonly IReadOnlyList<ActuatorConfig> _actuators;

    public MotionCueingService(MotionProfile profile, IReadOnlyList<ActuatorConfig> actuators)
    {
        _profile = profile;
        _actuators = actuators;
    }

    public ActuatorCommand ToCommand(TelemetrySample sample)
    {
        // Very small sample cueing layer: heave drives all four actuators, pitch/roll create opposing deltas.
        double frontBias = sample.Pitch * _profile.PitchScale;
        double rollBias = sample.Roll * _profile.RollScale;
        double heave = sample.Heave * _profile.HeaveScale;

        double[] normalized =
        {
            Clamp(heave + frontBias - rollBias), // front left
            Clamp(heave + frontBias + rollBias), // front right
            Clamp(heave - frontBias - rollBias), // rear left
            Clamp(heave - frontBias + rollBias), // rear right
        };

        var values = _actuators
            .Select((actuator, index) => ScaleToByte(normalized[index], actuator))
            .ToArray();

        return new ActuatorCommand(values);
    }

    private static double Clamp(double value) => Math.Max(-1, Math.Min(1, value));

    private static byte ScaleToByte(double normalized, ActuatorConfig config)
    {
        double range = config.MaxValue - config.MinValue;
        double scaled = (normalized + 1) / 2 * range + config.MinValue;
        return (byte)Math.Round(Math.Clamp(scaled, config.MinValue, config.MaxValue));
    }
}
