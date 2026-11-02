using System;
using System.Reflection;
using GorillaLocomotion;
using HarmonyLib;
using SaucyMenu.Core;
using SaucyMenu.Hosting;

namespace SaucyMenu.Patches
{
    /// <summary>
    /// The only Harmony patch in Saucy Menu: a prefix on the local player's GTPlayer.LateUpdate so movement
    /// modules run right before the game's own locomotion step. It reads nothing from and changes nothing in
    /// the game method itself. If the method is missing after a game update, movement modules refuse to start.
    /// </summary>
    internal static class LocomotionHook
    {
        public static bool Available { get; private set; }

        public static void Install(Harmony harmony, ILog log)
        {
            try
            {
                MethodInfo target = AccessTools.Method(typeof(GTPlayer), "LateUpdate");
                if (target == null)
                {
                    log.Error("GTPlayer.LateUpdate not found - this game version changed; movement mods are disabled.");
                    return;
                }

                harmony.Patch(target, prefix: new HarmonyMethod(typeof(LocomotionHook), nameof(Prefix)));
                Available = true;
            }
            catch (Exception ex)
            {
                log.Error($"Could not install locomotion hook; movement mods are disabled: {ex}");
            }
        }

        private static void Prefix()
        {
            try
            {
                MenuHost.Instance?.OnPreLocomotion();
            }
            catch (Exception)
            {
                // Never let a menu problem break the game's movement; the registry already logs module errors.
            }
        }
    }
}
