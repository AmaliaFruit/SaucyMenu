using System.Collections.Generic;

namespace SaucyMenu.Core.Settings
{
    public enum MenuHand
    {
        Left,
        Right
    }

    /// <summary>Everything Saucy Menu persists. Stored as JSON in BepInEx/config/SaucyMenu/settings.json.</summary>
    public sealed class SettingsData
    {
        public const int CurrentSchemaVersion = 1;

        public int SchemaVersion { get; set; } = CurrentSchemaVersion;
        public string ThemeId { get; set; } = "saucy";
        public MenuHand MenuHand { get; set; } = MenuHand.Left;
        public float MenuScale { get; set; } = 1f;

        /// <summary>Turns the panel around if it faces away from you on your controllers.</summary>
        public bool FlipMenu { get; set; }
        public bool ButtonSounds { get; set; } = true;
        public bool Haptics { get; set; } = true;

        /// <summary>Re-enable modules that were on last session (still subject to the room gate).</summary>
        public bool RememberEnabledModules { get; set; } = true;

        public List<string> Favorites { get; set; } = new List<string>();
        public List<string> EnabledModules { get; set; } = new List<string>();

        /// <summary>moduleId -> optionId -> chosen value.</summary>
        public Dictionary<string, Dictionary<string, string>> ModuleOptions { get; set; } =
            new Dictionary<string, Dictionary<string, string>>();

        public static readonly float[] ScaleSteps = { 0.8f, 0.9f, 1f, 1.1f, 1.25f, 1.4f };

        /// <summary>Repairs anything a hand-edited or older file could get wrong.</summary>
        public void Sanitize()
        {
            if (Favorites == null) Favorites = new List<string>();
            if (EnabledModules == null) EnabledModules = new List<string>();
            if (ModuleOptions == null) ModuleOptions = new Dictionary<string, Dictionary<string, string>>();
            if (string.IsNullOrWhiteSpace(ThemeId)) ThemeId = "saucy";
            if (!(MenuScale >= 0.5f && MenuScale <= 2f)) MenuScale = 1f;
            Favorites.RemoveAll(string.IsNullOrWhiteSpace);
            EnabledModules.RemoveAll(string.IsNullOrWhiteSpace);
            SchemaVersion = CurrentSchemaVersion;
        }
    }
}
