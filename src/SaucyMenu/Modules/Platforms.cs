using System.Collections.Generic;
using GorillaLocomotion;
using SaucyMenu.Core.Modules;
using SaucyMenu.Input;
using SaucyMenu.UI;
using UnityEngine;

namespace SaucyMenu.Modules
{
    /// <summary>
    /// Hold grip to put a solid slab under that hand, let go to remove it. The slabs exist only in your
    /// game (nothing is networked); other players just see you standing in the air, which is why this is
    /// limited to private rooms and modded queues.
    /// </summary>
    internal sealed class Platforms : ModuleBase
    {
        private static readonly Vector3 SlabSize = new Vector3(0.025f, 0.28f, 0.38f);

        private readonly VrInput input;
        private GameObject left;
        private GameObject right;

        public Platforms(VrInput input) => this.input = input;

        public override string Id => "platforms";
        public override string Name => "Platforms";
        public override string Description => "Hold grip to stand on a platform under that hand. Release to remove it. Only exists in your game.";
        public override string Category => "Movement";
        public override ModuleSafety Safety => ModuleSafety.PrivateRoomOnly;
        public override string[] Tags => new[] { "grip", "air", "climb", "plats" };

        public override IReadOnlyList<ModuleOption> Options { get; } = new[]
        {
            ModuleOption.Toggle("visible", "Show platforms", true)
        };

        protected internal override void OnUpdate()
        {
            GorillaTagger tagger = GorillaTagger.Instance;
            if (tagger == null)
                return;

            Handle(ref left, input.LeftGrip, tagger.leftHandTransform, true);
            Handle(ref right, input.RightGrip, tagger.rightHandTransform, false);
        }

        protected internal override void OnDisable()
        {
            Remove(ref left);
            Remove(ref right);
        }

        protected internal override void OnOptionChanged(string optionId)
        {
            bool visible = GetToggle("visible");
            if (left != null) left.GetComponent<Renderer>().enabled = visible;
            if (right != null) right.GetComponent<Renderer>().enabled = visible;
        }

        private void Handle(ref GameObject slab, bool grip, Transform hand, bool isLeft)
        {
            if (grip && slab == null && hand != null)
                slab = Create(hand, isLeft);
            else if (!grip && slab != null)
                Remove(ref slab);
        }

        private GameObject Create(Transform hand, bool isLeft)
        {
            float scale = GTPlayer.Instance != null ? GTPlayer.Instance.scale : 1f;
            Vector3 size = SlabSize * scale;

            // Collider kept: this is the surface you stand on. Default layer, which the game's locomotion collides with.
            GameObject slab = Visuals.Box(isLeft ? "SaucyMenu_PlatformL" : "SaucyMenu_PlatformR", null, Vector3.zero, size,
                new Color(0.95f, 0.7f, 0.24f), keepCollider: true);

            // Placed just beside the palm (not around the hand) so the hand doesn't start inside the collider.
            Vector3 side = hand.right * (isLeft ? 1f : -1f);
            slab.transform.SetPositionAndRotation(hand.position + side * (size.x / 2f + 0.025f * scale), hand.rotation);
            slab.GetComponent<Renderer>().enabled = GetToggle("visible");
            return slab;
        }

        private static void Remove(ref GameObject slab)
        {
            if (slab != null)
            {
                slab.SetActive(false); // Destroy is deferred to end of frame; stop it being solid now
                Object.Destroy(slab);
            }
            slab = null;
        }
    }
}
