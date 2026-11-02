using System;
using System.Collections.Generic;

namespace SaucyMenu.Core.Modules
{
    /// <summary>
    /// Base class for every feature. A module is self-contained: it owns the objects it creates,
    /// must clean all of them up in <see cref="OnDisable"/>, and never talks to the network.
    /// The registry calls the hooks inside try/catch, so a failing module is switched off on its own
    /// instead of taking the whole menu down.
    /// </summary>
    public abstract class ModuleBase
    {
        private static readonly IReadOnlyList<ModuleOption> NoOptions = Array.Empty<ModuleOption>();
        private static readonly string[] NoTags = Array.Empty<string>();

        private Func<string, string, string> optionReader;

        /// <summary>Stable id used in settings files. Never rename once released.</summary>
        public abstract string Id { get; }
        public abstract string Name { get; }
        public abstract string Description { get; }
        public abstract string Category { get; }
        public abstract ModuleSafety Safety { get; }

        /// <summary>Extra search terms not shown in the menu.</summary>
        public virtual string[] Tags => NoTags;
        public virtual IReadOnlyList<ModuleOption> Options => NoOptions;

        public bool Enabled { get; internal set; }

        /// <summary>Message from the last time this module threw and was switched off, if any.</summary>
        public string LastError { get; internal set; }

        internal void BindOptions(Func<string, string, string> reader) => optionReader = reader;

        /// <summary>Current value of one of this module's options (always a valid choice).</summary>
        protected string GetOption(string optionId)
        {
            foreach (ModuleOption option in Options)
            {
                if (option.Id == optionId)
                    return option.Normalize(optionReader?.Invoke(Id, optionId));
            }
            throw new ArgumentException($"Module '{Id}' has no option '{optionId}'", nameof(optionId));
        }

        protected bool GetToggle(string optionId) => GetOption(optionId) == "On";

        protected float GetFloat(string optionId, float fallback) =>
            ModuleOption.ParseLeadingFloat(GetOption(optionId), fallback);

        /// <summary>Called once when the module is switched on.</summary>
        protected internal virtual void OnEnable() { }

        /// <summary>Called once when switched off, faulted, gated or the menu unloads. Must undo everything.</summary>
        protected internal virtual void OnDisable() { }

        /// <summary>Every frame from MonoBehaviour.Update.</summary>
        protected internal virtual void OnUpdate() { }

        /// <summary>Every frame just before the game's locomotion update (GTPlayer.LateUpdate). Use for movement.</summary>
        protected internal virtual void OnPreLocomotion() { }

        /// <summary>Every frame from MonoBehaviour.LateUpdate, after locomotion. Use for cameras and visuals.</summary>
        protected internal virtual void OnLateUpdate() { }

        protected internal virtual void OnOptionChanged(string optionId) { }
    }
}
