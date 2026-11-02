using System.Collections.Generic;
using SaucyMenu.Core.Modules;
using SaucyMenu.UI;

namespace SaucyMenu.Modules
{
    /// <summary>Room code, privacy, player count, region and mode on your own HUD. Read-only.</summary>
    internal sealed class RoomInfo : ModuleBase
    {
        private const string LineKey = "room";
        private readonly HudService hud;

        public RoomInfo(HudService hud) => this.hud = hud;

        public override string Id => "room-info";
        public override string Name => "Room Info";
        public override string Description => "Shows the room code, player count, region and mode on your HUD. Code is hidden by default for streaming.";
        public override string Category => "Info";
        public override ModuleSafety Safety => ModuleSafety.LocalInfo;
        public override string[] Tags => new[] { "code", "lobby", "players", "region", "streamer" };

        public override IReadOnlyList<ModuleOption> Options { get; } = new[]
        {
            new ModuleOption("code", "Room code", new[] { "Hidden", "Shown" }, 0)
        };

        protected internal override void OnEnable() => hud.SetLine(LineKey, BuildLine);
        protected internal override void OnDisable() => hud.RemoveLine(LineKey);

        private string BuildLine()
        {
            NetworkSystem net = NetworkSystem.Instance;
            if (net == null || !net.InRoom)
                return "Not in a room";

            string code = GetOption("code") == "Shown" ? net.RoomName : "****";
            string privacy = net.SessionIsPrivate ? "private" : "public";
            string mode = Shorten(net.GameModeString, 22);
            return $"{code} ({privacy})   {net.RoomPlayerCount}/10   {net.CurrentRegion?.ToUpperInvariant()}   {mode}";
        }

        private static string Shorten(string s, int max) =>
            string.IsNullOrEmpty(s) ? string.Empty : s.Length <= max ? s : s.Substring(0, max - 1) + "~";
    }
}
