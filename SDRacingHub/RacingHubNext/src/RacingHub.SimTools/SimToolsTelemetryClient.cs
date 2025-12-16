using System.Net;
using System.Net.Sockets;
using System.Text;
using RacingHub.Core.Interfaces;
using RacingHub.Core.Models;

namespace RacingHub.SimTools;

/// <summary>
/// Minimal UDP client for SimTools Game Engine UDP output.
/// </summary>
public sealed class SimToolsTelemetryClient : IGameTelemetrySource, IAsyncDisposable
{
    private readonly UdpClient _udpClient;
    private readonly int _expectedBytes;

    public SimToolsTelemetryClient(int listenPort = 4123, int expectedBytes = 24)
    {
        _udpClient = new UdpClient(listenPort);
        _udpClient.Client.ReceiveTimeout = 1000;
        _expectedBytes = expectedBytes;
    }

    public async Task<TelemetrySample?> ReadAsync(CancellationToken cancellationToken)
    {
        try
        {
            var result = await _udpClient.ReceiveAsync(cancellationToken);
            if (result.Buffer.Length < _expectedBytes)
            {
                return null;
            }

            return ParseSimToolsBuffer(result.Buffer);
        }
        catch (OperationCanceledException)
        {
            return null;
        }
        catch (SocketException)
        {
            return null;
        }
    }

    private static TelemetrySample ParseSimToolsBuffer(byte[] buffer)
    {
        // SimTools offers a configurable packet layout. The default is ASCII values separated by commas.
        // To keep this project self-contained we try to parse a simple comma delimited layout:
        // surge,sway,heave,pitch,roll,yaw in range -1..1
        var payload = Encoding.ASCII.GetString(buffer);
        var parts = payload.Split(',');
        if (parts.Length < 6)
        {
            return new TelemetrySample();
        }

        double Parse(int index) => double.TryParse(parts[index], out var value) ? value : 0;

        return new TelemetrySample
        {
            Surge = Parse(0),
            Sway = Parse(1),
            Heave = Parse(2),
            Pitch = Parse(3),
            Roll = Parse(4),
            Yaw = Parse(5),
        };
    }

    public async ValueTask DisposeAsync()
    {
        await Task.Yield();
        _udpClient.Dispose();
    }
}
