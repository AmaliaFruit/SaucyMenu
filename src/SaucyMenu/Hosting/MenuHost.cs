using System;
using System.IO;
using BepInEx;
using SaucyMenu.Core;
using SaucyMenu.Core.Menu;
using SaucyMenu.Core.Modules;
using SaucyMenu.Core.Settings;
using SaucyMenu.Input;
using SaucyMenu.Modules;
using SaucyMenu.UI;
using UnityEngine;

namespace SaucyMenu.Hosting
{
    /// <summary>
    /// Composition root and frame driver. Owns settings, the module registry, the menu controller and the
    /// renderer, and calls each of them at the right point in the frame.
    /// </summary>
    internal sealed class MenuHost : MonoBehaviour
    {
        public static MenuHost Instance { get; private set; }

        private ILog log;
        private SettingsStore settings;
        private ModuleRegistry registry;
        private MenuController controller;
        private VrMenuRenderer renderer;
        private HudService hud;

        public VrInput Input { get; } = new VrInput();

        private static double Now => Time.realtimeSinceStartupAsDouble;

        public void Initialize(ILog logger)
        {
            log = logger ?? NullLog.Instance;

            string path = Path.Combine(Paths.ConfigPath, "SaucyMenu", "settings.json");
            settings = new SettingsStore(path, log);
            settings.Load();

            registry = new ModuleRegistry(settings, new GameRoomState(), log, () => Now);
            controller = new MenuController(registry, settings, () => Now, PluginInfo.Version);
            hud = new HudService(() => controller.Theme);
            renderer = new VrMenuRenderer(controller, () => settings.Data);

            registry.Notice += message => log.Info(message);
            controller.Pressed += PlayPressFeedback;

            // The first five modules. Add new ones here; each lives in its own file under Modules/.
            registry.Register(new PerformanceHud(hud));
            registry.Register(new RoomInfo(hud));
            registry.Register(new SmoothCamera());
            registry.Register(new Platforms(Input));
            registry.Register(new Fly(Input));

            registry.RestoreFromSettings();
            Instance = this;
        }

        private void Update()
        {
            if (registry == null)
                return;

            Input.Poll();
            registry.EnforceGate();
            controller.Poll();

            bool menuButton = settings.Data.MenuHand == MenuHand.Left ? Input.LeftSecondary : Input.RightSecondary;
            renderer.SetOpen(menuButton && GorillaTagger.Instance != null);

            registry.Tick(TickPhase.Update);
            hud.Update();
            settings.FlushIfDirty(Now);
        }

        /// <summary>Called by <see cref="Patches.LocomotionHook"/> just before the game moves the player.</summary>
        public void OnPreLocomotion()
        {
            if (registry == null)
                return;
            registry.EnforceGate();
            registry.Tick(TickPhase.PreLocomotion);
        }

        private void LateUpdate()
        {
            if (registry == null)
                return;
            registry.Tick(TickPhase.LateUpdate);
            renderer.LateUpdate(Now);
        }

        private void PlayPressFeedback()
        {
            if (GorillaTagger.Instance == null)
                return;

            bool left = renderer.PointerOnLeft;
            try
            {
                if (settings.Data.Haptics)
                    GorillaTagger.Instance.StartVibration(left, GorillaTagger.Instance.tapHapticStrength / 2f, 0.04f);

                // Plays only on your own hand's audio source; nothing is sent to other players.
                if (settings.Data.ButtonSounds && VRRig.LocalRig != null)
                    VRRig.LocalRig.PlayHandTapLocal(50, left, 0.3f);
            }
            catch (Exception ex)
            {
                log.Warn($"Press feedback failed: {ex.Message}");
            }
        }

        private void OnDestroy()
        {
            if (Instance == this)
                Instance = null;

            try
            {
                registry?.DisableAllForShutdown();
                renderer?.Dispose();
                hud?.Dispose();
                settings?.Save();
                Visuals.DestroyMaterials();
            }
            catch (Exception ex)
            {
                log?.Error($"Error during shutdown: {ex}");
            }
        }
    }
}
