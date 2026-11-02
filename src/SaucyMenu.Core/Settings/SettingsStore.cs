using System;
using System.Collections.Generic;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace SaucyMenu.Core.Settings
{
    /// <summary>
    /// Loads and saves <see cref="SettingsData"/>. Writes are batched (call <see cref="FlushIfDirty"/> periodically)
    /// and atomic (temp file then replace), and a corrupt file is moved aside instead of crashing the menu.
    /// </summary>
    public sealed class SettingsStore
    {
        private static readonly JsonSerializerSettings JsonSettings = new JsonSerializerSettings
        {
            Formatting = Formatting.Indented,
            NullValueHandling = NullValueHandling.Include,
            ObjectCreationHandling = ObjectCreationHandling.Replace,
            Converters = { new StringEnumConverter() }
        };

        private readonly string path;
        private readonly ILog log;
        private bool dirty;
        private double lastChange;

        public SettingsStore(string path, ILog log = null)
        {
            this.path = path ?? throw new ArgumentNullException(nameof(path));
            this.log = log ?? NullLog.Instance;
            Data = new SettingsData();
        }

        public SettingsData Data { get; private set; }
        public string FilePath => path;
        public bool IsDirty => dirty;

        public void Load()
        {
            try
            {
                if (!File.Exists(path))
                {
                    Data = new SettingsData();
                    return;
                }

                string json = File.ReadAllText(path);
                SettingsData loaded = JsonConvert.DeserializeObject<SettingsData>(json, JsonSettings);
                Data = loaded ?? new SettingsData();
                Data.Sanitize();
            }
            catch (Exception ex) when (ex is JsonException || ex is IOException || ex is UnauthorizedAccessException)
            {
                log.Warn($"Settings file unreadable ({ex.Message}); starting with defaults.");
                TryMoveAside();
                Data = new SettingsData();
            }
        }

        /// <summary>Mark settings changed. <paramref name="now"/> is any monotonically increasing time in seconds.</summary>
        public void MarkDirty(double now)
        {
            dirty = true;
            lastChange = now;
        }

        /// <summary>Saves if something changed at least <paramref name="delaySeconds"/> ago.</summary>
        public bool FlushIfDirty(double now, double delaySeconds = 1.0)
        {
            if (!dirty || now - lastChange < delaySeconds)
                return false;
            Save();
            return true;
        }

        public void Save()
        {
            try
            {
                Data.Sanitize();
                string dir = Path.GetDirectoryName(path);
                if (!string.IsNullOrEmpty(dir))
                    Directory.CreateDirectory(dir);

                string tmp = path + ".tmp";
                File.WriteAllText(tmp, JsonConvert.SerializeObject(Data, JsonSettings));
                if (File.Exists(path))
                    File.Replace(tmp, path, null);
                else
                    File.Move(tmp, path);
                dirty = false;
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
            {
                log.Error($"Could not save settings: {ex.Message}");
            }
        }

        public void ResetToDefaults(double now)
        {
            Data = new SettingsData();
            MarkDirty(now);
        }

        public bool IsFavorite(string moduleId) => Data.Favorites.Contains(moduleId);

        public bool ToggleFavorite(string moduleId, double now)
        {
            bool nowFavorite;
            if (Data.Favorites.Remove(moduleId))
                nowFavorite = false;
            else
            {
                Data.Favorites.Add(moduleId);
                nowFavorite = true;
            }
            MarkDirty(now);
            return nowFavorite;
        }

        public string GetModuleOption(string moduleId, string optionId)
        {
            return Data.ModuleOptions.TryGetValue(moduleId, out Dictionary<string, string> options) &&
                   options.TryGetValue(optionId, out string value)
                ? value
                : null;
        }

        public void SetModuleOption(string moduleId, string optionId, string value, double now)
        {
            if (!Data.ModuleOptions.TryGetValue(moduleId, out Dictionary<string, string> options))
            {
                options = new Dictionary<string, string>();
                Data.ModuleOptions[moduleId] = options;
            }
            options[optionId] = value;
            MarkDirty(now);
        }

        public void SetModuleEnabled(string moduleId, bool enabled, double now)
        {
            bool changed = enabled
                ? !Data.EnabledModules.Contains(moduleId) && AddReturn(Data.EnabledModules, moduleId)
                : Data.EnabledModules.Remove(moduleId);
            if (changed)
                MarkDirty(now);
        }

        private static bool AddReturn(List<string> list, string item)
        {
            list.Add(item);
            return true;
        }

        private void TryMoveAside()
        {
            try
            {
                if (File.Exists(path))
                {
                    string backup = path + ".corrupt-" + DateTime.UtcNow.ToString("yyyyMMddHHmmss");
                    File.Move(path, backup);
                    log.Warn($"Moved unreadable settings to {backup}");
                }
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
            {
                log.Warn($"Could not move unreadable settings aside: {ex.Message}");
            }
        }
    }
}
