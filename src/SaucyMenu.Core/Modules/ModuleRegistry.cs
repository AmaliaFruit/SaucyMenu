using System;
using System.Collections.Generic;
using SaucyMenu.Core.Settings;

namespace SaucyMenu.Core.Modules
{
    public enum TickPhase
    {
        Update,
        PreLocomotion,
        LateUpdate
    }

    /// <summary>
    /// Owns every module: registration, enable/disable, per-frame ticks, option storage and the room gate.
    /// All module code runs inside try/catch here; a module that throws is switched off and reported,
    /// the rest keep running.
    /// </summary>
    public sealed class ModuleRegistry
    {
        private readonly List<ModuleBase> modules = new List<ModuleBase>();
        private readonly Dictionary<string, ModuleBase> byId = new Dictionary<string, ModuleBase>(StringComparer.Ordinal);
        private readonly List<string> categories = new List<string>();
        private readonly SettingsStore settings;
        private readonly IRoomState room;
        private readonly ILog log;
        private readonly Func<double> clock;

        public ModuleRegistry(SettingsStore settings, IRoomState room, ILog log, Func<double> clock)
        {
            this.settings = settings ?? throw new ArgumentNullException(nameof(settings));
            this.room = room;
            this.log = log ?? NullLog.Instance;
            this.clock = clock ?? (() => 0d);
        }

        /// <summary>Short user-facing messages (shown as toasts in VR).</summary>
        public event Action<string> Notice;

        /// <summary>Raised whenever any module's enabled state changes.</summary>
        public event Action Changed;

        public IReadOnlyList<ModuleBase> Modules => modules;
        public IReadOnlyList<string> Categories => categories;
        public GateResult Gate => RoomGate.Evaluate(room);

        public void Register(ModuleBase module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            if (string.IsNullOrWhiteSpace(module.Id)) throw new ArgumentException("Module id is required");
            if (string.IsNullOrWhiteSpace(module.Name)) throw new ArgumentException($"Module '{module.Id}' needs a name");
            if (string.IsNullOrWhiteSpace(module.Category)) throw new ArgumentException($"Module '{module.Id}' needs a category");
            if (byId.ContainsKey(module.Id)) throw new InvalidOperationException($"Duplicate module id '{module.Id}'");

            var optionIds = new HashSet<string>(StringComparer.Ordinal);
            foreach (ModuleOption option in module.Options)
            {
                if (!optionIds.Add(option.Id))
                    throw new InvalidOperationException($"Module '{module.Id}' has duplicate option '{option.Id}'");
            }

            module.BindOptions(settings.GetModuleOption);
            modules.Add(module);
            byId.Add(module.Id, module);
            if (!categories.Contains(module.Category))
                categories.Add(module.Category);
        }

        public ModuleBase Get(string id) => id != null && byId.TryGetValue(id, out ModuleBase m) ? m : null;

        public IEnumerable<ModuleBase> InCategory(string category)
        {
            foreach (ModuleBase m in modules)
            {
                if (m.Category == category)
                    yield return m;
            }
        }

        public int EnabledCount
        {
            get
            {
                int n = 0;
                foreach (ModuleBase m in modules)
                    if (m.Enabled) n++;
                return n;
            }
        }

        public bool CanRun(ModuleBase module) => RoomGate.CanRun(module.Safety, room);

        /// <summary>Turns a module on or off. Returns false (with a reason) if it could not be switched on.</summary>
        public bool TrySetEnabled(string id, bool enable, out string message, bool persist = true)
        {
            ModuleBase module = Get(id);
            if (module == null)
            {
                message = $"Unknown module '{id}'";
                return false;
            }

            if (module.Enabled == enable)
            {
                message = null;
                return true;
            }

            if (enable)
            {
                GateResult gate = RoomGate.Evaluate(room);
                if (module.Safety == ModuleSafety.PrivateRoomOnly && !gate.Allowed)
                {
                    message = $"{module.Name}: private rooms only ({gate.Reason})";
                    return false;
                }

                module.LastError = null;
                module.Enabled = true;
                if (!Invoke(module, m => m.OnEnable(), "enable"))
                {
                    message = $"{module.Name} failed to start";
                    return false;
                }
            }
            else
            {
                module.Enabled = false;
                Invoke(module, m => m.OnDisable(), "disable");
            }

            if (persist)
                settings.SetModuleEnabled(module.Id, enable, clock());

            message = null;
            Changed?.Invoke();
            return true;
        }

        public bool Toggle(string id, out string message)
        {
            ModuleBase module = Get(id);
            if (module == null)
            {
                message = $"Unknown module '{id}'";
                return false;
            }
            return TrySetEnabled(id, !module.Enabled, out message);
        }

        /// <summary>Called every frame before ticking: switches off movement modules as soon as the room becomes public.</summary>
        public void EnforceGate()
        {
            GateResult gate = RoomGate.Evaluate(room);
            if (gate.Allowed)
                return;

            foreach (ModuleBase module in modules)
            {
                if (module.Enabled && module.Safety == ModuleSafety.PrivateRoomOnly)
                {
                    module.Enabled = false;
                    Invoke(module, m => m.OnDisable(), "disable");
                    settings.SetModuleEnabled(module.Id, false, clock());
                    RaiseNotice($"{module.Name} turned off: {gate.Reason}");
                    Changed?.Invoke();
                }
            }
        }

        public void Tick(TickPhase phase)
        {
            // Index loop: a module may be disabled (not removed) while ticking.
            for (int i = 0; i < modules.Count; i++)
            {
                ModuleBase module = modules[i];
                if (!module.Enabled)
                    continue;

                switch (phase)
                {
                    case TickPhase.Update: Invoke(module, m => m.OnUpdate(), "update"); break;
                    case TickPhase.PreLocomotion: Invoke(module, m => m.OnPreLocomotion(), "pre-locomotion"); break;
                    case TickPhase.LateUpdate: Invoke(module, m => m.OnLateUpdate(), "late update"); break;
                }
            }
        }

        public string GetOptionValue(ModuleBase module, ModuleOption option) =>
            option.Normalize(settings.GetModuleOption(module.Id, option.Id));

        public string CycleOption(string moduleId, string optionId)
        {
            ModuleBase module = Get(moduleId) ?? throw new ArgumentException($"Unknown module '{moduleId}'");
            ModuleOption option = null;
            foreach (ModuleOption o in module.Options)
                if (o.Id == optionId) option = o;
            if (option == null) throw new ArgumentException($"Module '{moduleId}' has no option '{optionId}'");

            string next = option.Next(settings.GetModuleOption(moduleId, optionId));
            settings.SetModuleOption(moduleId, optionId, next, clock());
            if (module.Enabled)
                Invoke(module, m => m.OnOptionChanged(optionId), "option change");
            Changed?.Invoke();
            return next;
        }

        /// <summary>Re-enables modules saved as on, if the player has that setting turned on.</summary>
        public void RestoreFromSettings()
        {
            if (!settings.Data.RememberEnabledModules)
            {
                if (settings.Data.EnabledModules.Count > 0)
                {
                    settings.Data.EnabledModules.Clear();
                    settings.MarkDirty(clock());
                }
                return;
            }

            foreach (string id in settings.Data.EnabledModules.ToArray())
            {
                ModuleBase module = Get(id);
                if (module == null)
                    continue; // removed module; leave the id so downgrades keep it

                // Blocked, not forgotten: e.g. the room isn't known yet at startup. The module stays off until the
                // player turns it on, and the saved choice is kept for the next launch.
                if (!TrySetEnabled(id, true, out string message, persist: false) && message != null)
                    log.Info($"Not restoring {id}: {message}");
            }
        }

        /// <summary>Switches everything off without touching saved state (used on unload).</summary>
        public void DisableAllForShutdown()
        {
            foreach (ModuleBase module in modules)
            {
                if (!module.Enabled) continue;
                module.Enabled = false;
                Invoke(module, m => m.OnDisable(), "disable");
            }
        }

        private bool Invoke(ModuleBase module, Action<ModuleBase> action, string what)
        {
            try
            {
                action(module);
                return true;
            }
            catch (Exception ex)
            {
                log.Error($"Module '{module.Id}' threw during {what}: {ex}");
                module.LastError = ex.GetType().Name + ": " + ex.Message;

                if (module.Enabled)
                {
                    module.Enabled = false;
                    try { module.OnDisable(); }
                    catch (Exception cleanupEx) { log.Error($"Module '{module.Id}' also threw while cleaning up: {cleanupEx}"); }
                    settings.SetModuleEnabled(module.Id, false, clock());
                    Changed?.Invoke();
                }

                RaiseNotice($"{module.Name} hit an error and was turned off");
                return false;
            }
        }

        private void RaiseNotice(string text)
        {
            try { Notice?.Invoke(text); }
            catch (Exception ex) { log.Error($"Notice handler threw: {ex}"); }
        }
    }
}
