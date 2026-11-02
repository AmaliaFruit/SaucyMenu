using System;
using BepInEx;
using HarmonyLib;
using SaucyMenu.Core;
using SaucyMenu.Hosting;
using SaucyMenu.Patches;
using UnityEngine;

namespace SaucyMenu
{
    /// <summary>
    /// BepInEx entry point. Keeps startup minimal: install the one locomotion hook, then wait for the
    /// game to spawn the local player before building anything.
    /// </summary>
    [BepInPlugin(PluginInfo.Guid, PluginInfo.Name, PluginInfo.Version)]
    public sealed class Plugin : BaseUnityPlugin
    {
        private Harmony harmony;
        private ILog log;
        private MenuHost host;

        private void Awake()
        {
            log = new DelegateLog(Logger.LogInfo, Logger.LogWarning, Logger.LogError);
            log.Info($"{PluginInfo.Name} {PluginInfo.Version} loading");

            harmony = new Harmony(PluginInfo.Guid);
            LocomotionHook.Install(harmony, log);

            GorillaTagger.OnPlayerSpawned(OnPlayerSpawned);
        }

        private void OnPlayerSpawned()
        {
            if (host != null)
                return;

            try
            {
                var go = new GameObject("SaucyMenu");
                DontDestroyOnLoad(go);
                host = go.AddComponent<MenuHost>();
                host.Initialize(log);
                log.Info("Menu ready. Hold the secondary button (Y/B) on your menu hand to open it.");
            }
            catch (Exception ex)
            {
                log.Error($"Failed to start menu: {ex}");
                if (host != null)
                    Destroy(host.gameObject);
                host = null;
            }
        }

        private void OnDestroy()
        {
            if (host != null)
                Destroy(host.gameObject);
            harmony?.UnpatchSelf();
        }
    }
}
