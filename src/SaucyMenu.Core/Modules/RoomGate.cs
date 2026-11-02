using System;

namespace SaucyMenu.Core.Modules
{
    /// <summary>Snapshot of the room the local player is in. Implemented against the game on the Unity side.</summary>
    public interface IRoomState
    {
        bool InRoom { get; }
        bool IsPrivate { get; }
        string GameMode { get; }
    }

    public readonly struct GateResult
    {
        public GateResult(bool allowed, string reason)
        {
            Allowed = allowed;
            Reason = reason;
        }

        public bool Allowed { get; }
        public string Reason { get; }
    }

    /// <summary>
    /// Decides whether <see cref="ModuleSafety.PrivateRoomOnly"/> modules may run.
    /// Allowed: not in a room (solo), a private room, or a modded queue (game mode string containing "MODDED",
    /// the convention used by community modded-lobby tooling). Everything else - public lobbies - is blocked.
    /// If the room state can't be read, the gate fails closed.
    /// </summary>
    public static class RoomGate
    {
        public static GateResult Evaluate(IRoomState room)
        {
            if (room == null)
                return new GateResult(false, "Room state unavailable");

            bool inRoom;
            bool isPrivate;
            string mode;
            try
            {
                inRoom = room.InRoom;
                if (!inRoom)
                    return new GateResult(true, "Not in a room");

                isPrivate = room.IsPrivate;
                mode = room.GameMode ?? string.Empty;
            }
            catch (Exception)
            {
                return new GateResult(false, "Room state unavailable");
            }

            if (isPrivate)
                return new GateResult(true, "Private room");

            if (mode.IndexOf("MODDED", StringComparison.OrdinalIgnoreCase) >= 0)
                return new GateResult(true, "Modded queue");

            return new GateResult(false, "Public lobby");
        }

        public static bool CanRun(ModuleSafety safety, IRoomState room) =>
            safety != ModuleSafety.PrivateRoomOnly || Evaluate(room).Allowed;
    }
}
