using RacingHub.Core.Models;

namespace RacingHub.Core.Interfaces;

public interface IGameTelemetrySource
{
    Task<TelemetrySample?> ReadAsync(CancellationToken cancellationToken);
}
