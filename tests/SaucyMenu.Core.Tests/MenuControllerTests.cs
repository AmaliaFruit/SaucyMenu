using System.Linq;
using SaucyMenu.Core.Menu;
using SaucyMenu.Core.Modules;
using SaucyMenu.Core.Settings;
using Xunit;

namespace SaucyMenu.Core.Tests
{
    public class MenuControllerTests
    {
        private static (Harness h, MenuController menu) Make(FakeRoom room = null, int extraModules = 0)
        {
            var h = new Harness(room);
            h.Registry.Register(new FakeModule("perf", "Performance HUD", "Info"));
            h.Registry.Register(new FakeModule("fly", "Fly", "Movement", ModuleSafety.PrivateRoomOnly));
            for (int i = 0; i < extraModules; i++)
                h.Registry.Register(new FakeModule("extra" + i, "Extra " + i, "Info"));
            var menu = new MenuController(h.Registry, h.Settings, () => h.Now, "1.0.0");
            return (h, menu);
        }

        private static int RowIndex(MenuController menu, string label) =>
            menu.Current.Rows.FindIndex(r => r.Label == label);

        [Fact]
        public void Home_lists_shortcuts_categories_and_settings()
        {
            var (_, menu) = Make();
            string[] labels = menu.Current.Rows.Select(r => r.Label).ToArray();
            Assert.Equal(new[] { "Search", "Favorites", "Enabled", "Info", "Movement", "Settings" }, labels);
            Assert.False(menu.Current.CanGoBack);
        }

        [Fact]
        public void Opening_a_category_and_toggling_a_module()
        {
            var (h, menu) = Make();
            menu.PressRow(RowIndex(menu, "Info"));
            Assert.Equal("Info", menu.Current.Title);
            Assert.True(menu.Current.CanGoBack);

            menu.PressRow(RowIndex(menu, "Performance HUD"));
            Assert.True(h.Registry.Get("perf").Enabled);
            Assert.True(menu.Current.Rows.Single(r => r.ModuleId == "perf").On);

            menu.Back();
            Assert.Equal("Saucy Menu", menu.Current.Title);
            Assert.Equal("1", menu.Current.Rows.Single(r => r.Label == "Enabled").Value);
        }

        [Fact]
        public void Gated_module_shows_blocked_and_toasts_on_press()
        {
            var (h, menu) = Make(FakeRoom.Public());
            menu.PressRow(RowIndex(menu, "Movement"));
            MenuRow fly = menu.Current.Rows.Single(r => r.ModuleId == "fly");
            Assert.True(fly.Blocked);
            Assert.Equal("PRIVATE", fly.Badge);

            menu.PressRow(menu.Current.Rows.IndexOf(fly));
            Assert.False(h.Registry.Get("fly").Enabled);
            Assert.Contains("private rooms only", menu.ToastText);
            Assert.True(menu.Current.StatusIsWarning);
        }

        [Fact]
        public void Favoriting_from_detail_page_shows_in_favorites()
        {
            var (h, menu) = Make();
            menu.PressRow(RowIndex(menu, "Info"));
            menu.PressRowDetail(RowIndex(menu, "Performance HUD"));
            Assert.Equal(ViewKind.ModuleDetail, menu.View.Kind);

            menu.PressRow(RowIndex(menu, "Favorite"));
            Assert.True(h.Settings.IsFavorite("perf"));

            menu.Home();
            menu.PressRow(RowIndex(menu, "Favorites"));
            Assert.Equal(new[] { "Performance HUD" }, menu.Current.Rows.Select(r => r.Label));
        }

        [Fact]
        public void Search_keyboard_types_and_shows_results()
        {
            var (_, menu) = Make();
            menu.PressRow(RowIndex(menu, "Search"));
            Assert.NotNull(menu.Current.Keyboard);

            menu.PressKey("F");
            menu.PressKey("L");
            Assert.Equal("fl", menu.Query);
            Assert.Contains(menu.Current.Rows, r => r.ModuleId == "fly");

            menu.PressKey(MenuController.KeyBackspace);
            menu.PressKey(MenuController.KeyBackspace);
            menu.PressKey(MenuController.KeyBackspace);
            Assert.Equal("", menu.Query);
        }

        [Fact]
        public void Search_limits_preview_and_offers_all_results()
        {
            var (_, menu) = Make(extraModules: 6);
            menu.PressRow(RowIndex(menu, "Search"));
            menu.PressKey("E");
            menu.PressKey("X");
            Assert.Equal(MenuController.SearchPreviewRows, menu.Current.Rows.Count(r => r.Kind == RowKind.Module));

            menu.PressRow(RowIndex(menu, "All results"));
            Assert.Equal(ViewKind.SearchResults, menu.View.Kind);
            Assert.Equal(6, menu.Current.Rows.Count);
        }

        [Fact]
        public void Paging_splits_long_lists()
        {
            var (_, menu) = Make(extraModules: 10);
            menu.PressRow(RowIndex(menu, "Info"));
            Assert.Equal(2, menu.Current.PageCount);
            Assert.Equal(MenuController.RowsPerPage, menu.Current.Rows.Count);

            menu.NextPage();
            Assert.Equal(1, menu.Current.PageIndex);
            Assert.Equal(11 - MenuController.RowsPerPage, menu.Current.Rows.Count);

            menu.NextPage(); // no-op at the end
            Assert.Equal(1, menu.Current.PageIndex);
            menu.PreviousPage();
            Assert.Equal(0, menu.Current.PageIndex);
        }

        [Fact]
        public void Settings_cycle_theme_hand_and_size()
        {
            var (h, menu) = Make();
            int appearance = 0;
            menu.AppearanceChanged += () => appearance++;
            menu.PressRow(RowIndex(menu, "Settings"));

            menu.PressRow(RowIndex(menu, "Theme"));
            menu.PressRow(RowIndex(menu, "Menu hand"));
            menu.PressRow(RowIndex(menu, "Menu size"));

            Assert.Equal("midnight", h.Settings.Data.ThemeId);
            Assert.Equal(MenuHand.Right, h.Settings.Data.MenuHand);
            Assert.Equal(1.1f, h.Settings.Data.MenuScale);
            Assert.Equal(3, appearance);
            Assert.True(h.Settings.IsDirty);
        }

        [Fact]
        public void Reset_needs_two_presses_within_the_window()
        {
            var (h, menu) = Make();
            h.Settings.Data.ThemeId = "mint";
            menu.PressRow(RowIndex(menu, "Settings"));
            menu.NextPage(); // settings spill onto a second page

            int reset = RowIndex(menu, "Reset settings");
            Assert.True(reset >= 0);
            menu.PressRow(reset);
            Assert.Equal("mint", h.Settings.Data.ThemeId);
            Assert.Equal("Press again to reset everything", menu.Current.Rows[reset].Label);

            h.Now = 10; // confirm window (4 s) expired
            menu.Poll();
            Assert.Equal("Reset settings", menu.Current.Rows[reset].Label);

            menu.PressRow(reset);
            h.Now = 11;
            menu.PressRow(reset);
            Assert.Equal("saucy", h.Settings.Data.ThemeId);
        }

        [Fact]
        public void Gate_change_bumps_revision()
        {
            var (h, menu) = Make(FakeRoom.Private());
            menu.Poll();
            int before = menu.Revision;
            h.Room.IsPrivate = false;
            menu.Poll();
            Assert.NotEqual(before, menu.Revision);
            Assert.True(menu.Current.StatusIsWarning);
        }
    }
}
