using System;
using System.Collections.Generic;
using System.Globalization;

namespace SaucyMenu.Core.Modules
{
    /// <summary>
    /// A module setting the player cycles through with one button press in VR.
    /// Values are stored as the chosen string so settings files stay readable.
    /// </summary>
    public sealed class ModuleOption
    {
        public ModuleOption(string id, string name, IReadOnlyList<string> choices, int defaultIndex = 0)
        {
            if (string.IsNullOrWhiteSpace(id)) throw new ArgumentException("Option id is required", nameof(id));
            if (choices == null || choices.Count == 0) throw new ArgumentException("Option needs at least one choice", nameof(choices));
            if (defaultIndex < 0 || defaultIndex >= choices.Count) throw new ArgumentOutOfRangeException(nameof(defaultIndex));

            Id = id;
            Name = string.IsNullOrWhiteSpace(name) ? id : name;
            Choices = choices;
            DefaultIndex = defaultIndex;
        }

        public string Id { get; }
        public string Name { get; }
        public IReadOnlyList<string> Choices { get; }
        public int DefaultIndex { get; }
        public string Default => Choices[DefaultIndex];

        public static ModuleOption Toggle(string id, string name, bool defaultOn) =>
            new ModuleOption(id, name, new[] { "Off", "On" }, defaultOn ? 1 : 0);

        /// <summary>Returns the stored value if it is still a valid choice, otherwise the default.</summary>
        public string Normalize(string stored)
        {
            if (stored != null)
            {
                for (int i = 0; i < Choices.Count; i++)
                {
                    if (string.Equals(Choices[i], stored, StringComparison.Ordinal))
                        return Choices[i];
                }
            }
            return Default;
        }

        public string Next(string current)
        {
            string value = Normalize(current);
            for (int i = 0; i < Choices.Count; i++)
            {
                if (Choices[i] == value)
                    return Choices[(i + 1) % Choices.Count];
            }
            return Default;
        }

        /// <summary>Parses the leading number of a choice such as "75 FOV" or "1.5x".</summary>
        public static float ParseLeadingFloat(string value, float fallback)
        {
            if (string.IsNullOrEmpty(value)) return fallback;
            int end = 0;
            while (end < value.Length && (char.IsDigit(value[end]) || value[end] == '.' || (end == 0 && value[end] == '-')))
                end++;
            return end > 0 && float.TryParse(value.Substring(0, end), NumberStyles.Float, CultureInfo.InvariantCulture, out float f)
                ? f
                : fallback;
        }
    }
}
