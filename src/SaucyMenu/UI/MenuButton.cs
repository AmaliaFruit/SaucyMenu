using System;
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
            try
            {
                onPress?.Invoke();
            }
            catch (Exception ex)
            {
                Debug.LogError($"[Saucy Menu] Button action failed: {ex}");
            }
        }
    }
}
