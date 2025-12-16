using System.IO.Ports;
using RacingHub.Core.Interfaces;

namespace RacingHub.Arduino;

/// <summary>
/// Streams motion commands to an Arduino over a serial connection using the same layout
/// as the SimTools-based 4-actuator sketch (<255><fl><fr><rl><rr>). 
/// </summary>
public sealed class SerialActuatorPublisher : IActuatorCommandPublisher, IAsyncDisposable
{
    private readonly SerialPort _serialPort;

    public SerialActuatorPublisher(string portName, int baudRate = 115200)
    {
        _serialPort = new SerialPort(portName, baudRate)
        {
            Parity = Parity.None,
            StopBits = StopBits.One,
            DataBits = 8,
            Handshake = Handshake.None,
            NewLine = "\n",
            DtrEnable = true
        };
        _serialPort.Open();
    }

    public Task PublishAsync(ActuatorCommand command, CancellationToken cancellationToken)
    {
        // The Arduino sketch expects a start byte of 255 then 4 bytes, one per actuator.
        var packet = new byte[1 + command.Channels.Count];
        packet[0] = 255;
        for (int i = 0; i < command.Channels.Count; i++)
        {
            packet[i + 1] = command.Channels[i];
        }

        _serialPort.Write(packet, 0, packet.Length);
        return Task.CompletedTask;
    }

    public async ValueTask DisposeAsync()
    {
        await Task.Yield();
        if (_serialPort.IsOpen)
        {
            _serialPort.Close();
        }
        _serialPort.Dispose();
    }
}
