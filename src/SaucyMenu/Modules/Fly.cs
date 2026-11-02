using System;
using System.Collections.Generic;
using GorillaLocomotion;
using SaucyMenu.Core.Modules;
using SaucyMenu.Input;
using SaucyMenu.Patches;
using UnityEngine;

namespace SaucyMenu.Modules
{
    /// <summary>
    /// Hold the right primary button (A) to fly where you're looking. Moves only your own player, just before
    /// the game's locomotion step. Limited to private rooms and modded queues.
    /// </summary>
    internal sealed class Fly : ModuleBase
    {
        private readonly VrInput input;

        public Fly(VrInput input) => this.input = input;

        public override string Id => "fly";
        public override string Name => "Fly";
        public override string Description => "Hold A (right primary) to fly where you look. Release to fall normally.";
        public override string Category => "Movement";
        public override ModuleSafety Safety => ModuleSafety.PrivateRoomOnly;
        public override string[] Tags => new[] { "flight", "noclip", "move" };

        public override IReadOnlyList<ModuleOption> Options { get; } = new[]
        {
            new ModuleOption("speed", "Speed", new[] { "5 m/s", "10 m/s", "20 m/s" }, 1)
        };

        protected internal override void OnEnable()
        {
            if (!LocomotionHook.Available)
                throw new InvalidOperationException("Movement hook unavailable in this game version");
        }

        protected internal override void OnPreLocomotion()
        {
            if (!input.RightPrimary)
                return;

            GTPlayer player = GTPlayer.Instance;
            GorillaTagger tagger = GorillaTagger.Instance;
            if (player == null || tagger == null || tagger.headCollider == null || tagger.rigidbody == null)
                return;

            float speed = GetFloat("speed", 10f) * player.scale;
            player.transform.position += tagger.headCollider.transform.forward * (Time.deltaTime * speed);
            tagger.rigidbody.linearVelocity = Vector3.zero;
        }
    }
}
