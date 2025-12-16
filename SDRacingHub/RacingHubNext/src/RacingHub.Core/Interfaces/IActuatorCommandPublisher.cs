using RacingHub.Core.Models;

namespace RacingHub.Core.Interfaces;

public interface IActuatorCommandPublisher
{
    Task PublishAsync(ActuatorCommand command, CancellationToken cancellationToken);
}

public sealed record ActuatorCommand(IReadOnlyList<byte> Channels);
