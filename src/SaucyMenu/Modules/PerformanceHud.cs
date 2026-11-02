using System.Collections.Generic;
using Photon.Pun;
using SaucyMenu.Core.Modules;
using SaucyMenu.UI;
using UnityEngine;

namespace SaucyMenu.Modules
{
    /// <summary>Frame rate, frame time and ping, shown on your own HUD. Read-only.</summary>
    internal sealed class PerformanceHud : ModuleBase
    {
        private const string LineKey = "perf";
        private readonly HudService hud;
        private float smoothedDelta = 1f / 90f;

        public PerformanceHud(HudService hud) => this.hud = hud;

        public override string Id => "performance-hud";
        public override string Name => "Performance HUD";
        public override string Description => "Shows FPS, frame time and ping in the corner of your view. Only you see it.";
        public override string Category => "Info";
        public override ModuleSafety Safety => ModuleSafety.LocalInfo;
        public override string[] Tags => new[] { "fps", "ping", "lag", "frame", "stats" };

        public override IReadOnlyList<ModuleOption> Options { get; } = new[]
        {
            ModuleOption.Toggle("ping", "Show ping", true),
            ModuleOption.Toggle("frametime", "Show frame time", true)
        };

        protected internal override void OnEnable()
        {
            smoothedDelta = Mathf.Max(Time.unscaledDeltaTime, 0.001f);
            hud.SetLine(LineKey, BuildLine);
        }

        protected internal override void OnDisable() => hud.RemoveLine(LineKey);

        protected internal override void OnUpdate()
        {
            float dt = Mathf.Clamp(Time.unscaledDeltaTime, 0.0001f, 0.5f);
            smoothedDelta = Mathf.Lerp(smoothedDelta, dt, 0.08f);
        }

        private string BuildLine()
        {
            string line = $"FPS {Mathf.RoundToInt(1f / smoothedDelta)}";
            if (GetToggle("frametime"))
                line += $"   {smoothedDelta * 1000f:0.0} ms";
            if (GetToggle("ping"))
                line += PhotonNetwork.IsConnected ? $"   Ping {PhotonNetwork.GetPing()} ms" : "   Offline";
            return line;
        }
    }
}
