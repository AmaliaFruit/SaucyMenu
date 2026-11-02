using System.IO;
using System.Linq;
using SaucyMenu.Core.Modules;
using SaucyMenu.Core.Settings;
using SaucyMenu.Core.Themes;
using Xunit;

namespace SaucyMenu.Core.Tests
{
    public class SettingsTests
    {
        [Fact]
        public void Round_trips_through_disk()
        {
            using var dir = new TempDir();
            var store = new SettingsStore(dir.File("s.json"));
            store.Data.ThemeId = "mint";
            store.Data.MenuHand = MenuHand.Right;
            store.ToggleFavorite("fly", 0);
            store.SetModuleOption("cam", "fov", "90", 0);
            store.Save();

            var again = new SettingsStore(dir.File("s.json"));
            again.Load();
            Assert.Equal("mint", again.Data.ThemeId);
            Assert.Equal(MenuHand.Right, again.Data.MenuHand);
            Assert.True(again.IsFavorite("fly"));
            Assert.Equal("90", again.GetModuleOption("cam", "fov"));
            Assert.Contains("\"Right\"", File.ReadAllText(dir.File("s.json")));
        }

        [Fact]
        public void Corrupt_file_is_moved_aside_and_defaults_used()
        {
            using var dir = new TempDir();
            File.WriteAllText(dir.File("s.json"), "{ not json");
            var store = new SettingsStore(dir.File("s.json"));
            store.Load();

            Assert.Equal("saucy", store.Data.ThemeId);
            Assert.False(File.Exists(dir.File("s.json")));
            Assert.Single(Directory.GetFiles(dir.Path, "s.json.corrupt-*"));
        }

        [Fact]
        public void Sanitize_repairs_bad_values()
        {
            using var dir = new TempDir();
            File.WriteAllText(dir.File("s.json"), "{\"MenuScale\": 99, \"Favorites\": null, \"ThemeId\": \"\"}");
            var store = new SettingsStore(dir.File("s.json"));
            store.Load();
            Assert.Equal(1f, store.Data.MenuScale);
            Assert.NotNull(store.Data.Favorites);
            Assert.Equal("saucy", store.Data.ThemeId);
        }

        [Fact]
        public void Flush_waits_for_the_debounce_delay()
        {
            using var dir = new TempDir();
            var store = new SettingsStore(dir.File("s.json"));
            store.MarkDirty(10);
            Assert.False(store.FlushIfDirty(10.5));
            Assert.False(File.Exists(dir.File("s.json")));
            Assert.True(store.FlushIfDirty(11.1));
            Assert.True(File.Exists(dir.File("s.json")));
            Assert.False(store.IsDirty);
        }
    }

    public class SearchTests
    {
        private static readonly FakeModule[] Modules =
        {
            new FakeModule("fly", "Fly", "Movement", ModuleSafety.PrivateRoomOnly, "Fly where you look", new[] { "noclip" }),
            new FakeModule("platforms", "Platforms", "Movement", ModuleSafety.PrivateRoomOnly, "Grip to stand on air"),
            new FakeModule("perf", "Performance HUD", "Info", ModuleSafety.LocalInfo, "Shows FPS and ping", new[] { "fps" }),
            new FakeModule("cam", "Smooth Camera", "Visual", ModuleSafety.LocalVisual, "Smooths the desktop view")
        };

        [Fact]
        public void Name_prefix_ranks_first() =>
            Assert.Equal("platforms", ModuleSearch.Find(Modules, "pla").First().Id);

        [Fact]
        public void Tags_and_descriptions_match() =>
            Assert.Equal(new[] { "perf" }, ModuleSearch.Find(Modules, "fps").Select(m => m.Id));

        [Fact]
        public void Category_matches() =>
            Assert.Equal(new[] { "fly", "platforms" }, ModuleSearch.Find(Modules, "movement").Select(m => m.Id));

        [Fact]
        public void All_terms_must_match() =>
            Assert.Equal(new[] { "cam" }, ModuleSearch.Find(Modules, "smooth desk").Select(m => m.Id));

        [Fact]
        public void Search_is_case_insensitive() =>
            Assert.Equal("fly", ModuleSearch.Find(Modules, "FLY").First().Id);

        [Fact]
        public void Empty_query_returns_nothing() => Assert.Empty(ModuleSearch.Find(Modules, "  "));
    }

    public class ThemeTests
    {
        [Fact]
        public void Theme_ids_are_unique() =>
            Assert.Equal(ThemeLibrary.All.Count, ThemeLibrary.All.Select(t => t.Id).Distinct().Count());

        [Fact]
        public void Next_wraps_around() =>
            Assert.Equal(ThemeLibrary.All[0].Id, ThemeLibrary.Next(ThemeLibrary.All[ThemeLibrary.All.Count - 1].Id).Id);

        [Fact]
        public void Unknown_theme_falls_back_to_default() => Assert.Equal("saucy", ThemeLibrary.Get("nope").Id);

        [Fact]
        public void Hex_parses() => Assert.Equal("#C8382C", Rgba.Hex("#c8382c").ToHex());
    }
}
