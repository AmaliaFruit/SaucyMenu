using System;
using System.Collections.Generic;
using UnityEngine;

namespace SaucyMenu.UI
{
    /// <summary>
    /// A pressable panel button. Fires when the fingertip pointer enters its trigger collider.
    /// One shared cooldown stops a single poke registering twice, and stops a freshly drawn page from
    /// being pressed by a finger that is still resting where the old button was.
    /// </summary>
    internal sealed class MenuButton : MonoBehaviour
    {
        private const float Cooldown = 0.25f;
        private static float nextAllowed;

        private Action onPress;

        public void Init(Action press) => onPress = press;

        /// <summary>Called whenever the panel is rebuilt.</summary>
        public static void BlockPressesFor(float seconds) =>
            nextAllowed = Mathf.Max(nextAllowed, Time.unscaledTime + seconds);

        private void OnTriggerEnter(Collider other)
        {
            if (other == null || other != VrMenuRenderer.PointerCollider)
                return;
            if (Time.unscaledTime < nextAllowed)
                return;

            nextAllowed = Time.unscaledTime + Cooldown;
            // Queued, not run here: Unity forbids DestroyImmediate inside physics callbacks, and module
            // OnEnable/OnDisable may create or destroy objects. MenuHost runs the queue in Update.
            if (onPress != null)
                Pending.Enqueue(onPress);
        }

        private static readonly Queue<Action> Pending = new Queue<Action>();

        public static void RunPending()
        {
            while (Pending.Count > 0)
            {
                Action action = Pending.Dequeue();
                try
                {
                    action();
                }
                catch (Exception ex)
                {
                    Debug.LogError($"[Saucy Menu] Button action failed: {ex}");
                }
            }
        }
    }
}
