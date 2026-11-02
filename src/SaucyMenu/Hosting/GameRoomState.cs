using SaucyMenu.Core.Modules;

namespace SaucyMenu.Hosting
{
    /// <summary>Reads the current room from the game's NetworkSystem (read-only).</summary>
    internal sealed class GameRoomState : IRoomState
    {
        public bool InRoom => NetworkSystem.Instance != null && NetworkSystem.Instance.InRoom;

        public bool IsPrivate => NetworkSystem.Instance != null && NetworkSystem.Instance.SessionIsPrivate;

        public string GameMode => NetworkSystem.Instance != null ? NetworkSystem.Instance.GameModeString ?? string.Empty : string.Empty;
    }
}
