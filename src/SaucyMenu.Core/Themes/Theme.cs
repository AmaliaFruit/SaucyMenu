using System;
using System.Collections.Generic;
using System.Globalization;

namespace SaucyMenu.Core.Themes
{
    /// <summary>Unity-free colour so themes can be defined and tested without the engine.</summary>
    public readonly struct Rgba
    {
        public Rgba(float r, float g, float b, float a = 1f)
        {
            R = r;
            G = g;
            B = b;
            A = a;
        }

        public float R { get; }
        public float G { get; }
        public float B { get; }
        public float A { get; }

        /// <summary>Parses #RGB, #RRGGBB or #RRGGBBAA.</summary>
        public static Rgba Hex(string hex)
        {
            if (hex == null) throw new ArgumentNullException(nameof(hex));
            string h = hex.TrimStart('#');
            if (h.Length == 3)
                h = new string(new[] { h[0], h[0], h[1], h[1], h[2], h[2] });
            if (h.Length != 6 && h.Length != 8)
                throw new FormatException($"Bad colour '{hex}'");

            byte Part(int i) => byte.Parse(h.Substring(i, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            return new Rgba(Part(0) / 255f, Part(2) / 255f, Part(4) / 255f, h.Length == 8 ? Part(6) / 255f : 1f);
        }

        public string ToHex() =>
            "#" + To255(R).ToString("X2") + To255(G).ToString("X2") + To255(B).ToString("X2");

        private static int To255(float v) => Math.Max(0, Math.Min(255, (int)Math.Round(v * 255f)));
    }

    public sealed class Theme
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public Rgba Panel { get; set; }
        public Rgba Header { get; set; }
        public Rgba Button { get; set; }
        public Rgba ButtonOn { get; set; }
        public Rgba ButtonBlocked { get; set; }
        public Rgba Text { get; set; }
        public Rgba TextOn { get; set; }
        public Rgba TextMuted { get; set; }
        public Rgba Accent { get; set; }
        public Rgba Pointer { get; set; }
    }

    public static class ThemeLibrary
    {
        public static readonly IReadOnlyList<Theme> All = new[]
        {
            new Theme
            {
                Id = "saucy", Name = "Saucy",
                Panel = Rgba.Hex("#1E1414"), Header = Rgba.Hex("#C8382C"),
                Button = Rgba.Hex("#3A2624"), ButtonOn = Rgba.Hex("#E2573F"), ButtonBlocked = Rgba.Hex("#2A2222"),
                Text = Rgba.Hex("#F6E9DC"), TextOn = Rgba.Hex("#FFFFFF"), TextMuted = Rgba.Hex("#B39C90"),
                Accent = Rgba.Hex("#F2B33D"), Pointer = Rgba.Hex("#F2B33D")
            },
            new Theme
            {
                Id = "midnight", Name = "Midnight",
                Panel = Rgba.Hex("#0F1320"), Header = Rgba.Hex("#3B4CCA"),
                Button = Rgba.Hex("#1D2438"), ButtonOn = Rgba.Hex("#5468FF"), ButtonBlocked = Rgba.Hex("#171B27"),
                Text = Rgba.Hex("#E4E8FF"), TextOn = Rgba.Hex("#FFFFFF"), TextMuted = Rgba.Hex("#8C93B8"),
                Accent = Rgba.Hex("#7FD1FF"), Pointer = Rgba.Hex("#7FD1FF")
            },
            new Theme
            {
                Id = "mint", Name = "Mint",
                Panel = Rgba.Hex("#0F1C18"), Header = Rgba.Hex("#1F9E77"),
                Button = Rgba.Hex("#1B302A"), ButtonOn = Rgba.Hex("#2FC495"), ButtonBlocked = Rgba.Hex("#16221F"),
                Text = Rgba.Hex("#E2F5EE"), TextOn = Rgba.Hex("#06140F"), TextMuted = Rgba.Hex("#8FB3A6"),
                Accent = Rgba.Hex("#C6F36B"), Pointer = Rgba.Hex("#C6F36B")
            },
            new Theme
            {
                Id = "daylight", Name = "Daylight",
                Panel = Rgba.Hex("#F3EFE8"), Header = Rgba.Hex("#2B2B2B"),
                Button = Rgba.Hex("#E0D9CD"), ButtonOn = Rgba.Hex("#2B2B2B"), ButtonBlocked = Rgba.Hex("#ECE7DF"),
                Text = Rgba.Hex("#1F1F1F"), TextOn = Rgba.Hex("#FFFFFF"), TextMuted = Rgba.Hex("#7A7268"),
                Accent = Rgba.Hex("#D9480F"), Pointer = Rgba.Hex("#D9480F")
            },
            new Theme
            {
                Id = "contrast", Name = "High contrast",
                Panel = Rgba.Hex("#000000"), Header = Rgba.Hex("#FFFFFF"),
                Button = Rgba.Hex("#1A1A1A"), ButtonOn = Rgba.Hex("#FFE600"), ButtonBlocked = Rgba.Hex("#0D0D0D"),
                Text = Rgba.Hex("#FFFFFF"), TextOn = Rgba.Hex("#000000"), TextMuted = Rgba.Hex("#BDBDBD"),
                Accent = Rgba.Hex("#00E5FF"), Pointer = Rgba.Hex("#00E5FF")
            }
        };

        public static Theme Get(string id)
        {
            foreach (Theme t in All)
                if (t.Id == id) return t;
            return All[0];
        }

        public static Theme Next(string currentId)
        {
            for (int i = 0; i < All.Count; i++)
                if (All[i].Id == currentId) return All[(i + 1) % All.Count];
            return All[0];
        }
    }
}
