using SaucyMenu.Core.Modules;

namespace SaucyMenu.Hosting
{
    /// <summary>Reads the current room from the game's NetworkSystem (read-only).</summary>
    internal sealed class GameRoomState : IRoomState
    {
        // No NetworkSystem means we can't tell where we are: throw so RoomGate fails closed.
        public bool InRoom => NetworkSystem.Instance != null
            ? NetworkSystem.Instance.InRoom
            : throw new System.InvalidOperationException("NetworkSystem not available");

        public bool IsPrivate => NetworkSystem.Instance != null && NetworkSystem.Instance.SessionIsPrivate;

        public string GameMode => NetworkSystem.Instance != null ? NetworkSystem.Instance.GameModeString ?? string.Empty : string.Empty;
    }
}
