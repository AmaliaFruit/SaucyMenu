using System;
using System.Collections.Generic;
using System.Linq;
using SaucyMenu.Core.Modules;
using SaucyMenu.Core.Settings;
using SaucyMenu.Core.Themes;

namespace SaucyMenu.Core.Menu
{
    /// <summary>
    /// The whole menu as a state machine: which view is open, what rows it shows and what each press does.
    /// It knows nothing about Unity; the VR renderer draws <see cref="Current"/> and forwards presses here.
    /// Every state change bumps <see cref="Revision"/> so the renderer knows when to redraw.
    /// </summary>
    public sealed class MenuController
    {
        public const int RowsPerPage = 7;
        public const int SearchPreviewRows = 3;
        public const int MaxQueryLength = 24;
        public const string KeySpace = "SPACE";
        public const string KeyBackspace = "BACK";

        public static readonly IReadOnlyList<IReadOnlyList<string>> KeyboardLayout = new IReadOnlyList<string>[]
        {
            new[] { "A", "B", "C", "D", "E", "F", "G" },
            new[] { "H", "I", "J", "K", "L", "M", "N" },
            new[] { "O", "P", "Q", "R", "S", "T", "U" },
            new[] { "V", "W", "X", "Y", "Z", KeySpace, KeyBackspace }
        };

        private readonly ModuleRegistry registry;
        private readonly SettingsStore settings;
        private readonly Func<double> clock;
        private readonly string version;
        private readonly List<MenuView> stack = new List<MenuView>();

        private MenuPage cachedPage;
        private int cachedRevision = -1;
        private string lastGateReason;
        private double resetConfirmUntil;

        public MenuController(ModuleRegistry registry, SettingsStore settings, Func<double> clock, string version)
        {
            this.registry = registry ?? throw new ArgumentNullException(nameof(registry));
            this.settings = settings ?? throw new ArgumentNullException(nameof(settings));
            this.clock = clock ?? (() => 0d);
            this.version = version ?? "dev";
            stack.Add(new MenuView(ViewKind.Home));

            registry.Changed += Invalidate;
            registry.Notice += text => ShowToast(text);
        }

        /// <summary>Raised when theme, hand or size change, so the renderer can rebuild its geometry.</summary>
        public event Action AppearanceChanged;

        /// <summary>Raised whenever a press did something (renderer plays click feedback).</summary>
        public event Action Pressed;

        public int Revision { get; private set; }
        public string Query { get; private set; } = string.Empty;
        public string ToastText { get; private set; }
        public double ToastUntil { get; private set; }
        public MenuView View => stack[stack.Count - 1];
        public Theme Theme => ThemeLibrary.Get(settings.Data.ThemeId);
        public SettingsData Settings => settings.Data;

        public MenuPage Current
        {
            get
            {
                if (cachedPage == null || cachedRevision != Revision)
                {
                    cachedPage = Build();
                    cachedRevision = Revision;
                }
                return cachedPage;
            }
        }

        public void Invalidate() => Revision++;

        /// <summary>Call every frame: redraws when the room gate flips or a confirm prompt expires.</summary>
        public void Poll()
        {
            string reason = registry.Gate.Reason;
            if (reason != lastGateReason)
            {
                lastGateReason = reason;
                Invalidate();
            }

            if (resetConfirmUntil > 0 && clock() > resetConfirmUntil)
            {
                resetConfirmUntil = 0;
                Invalidate();
            }
        }

        public void ShowToast(string text, double seconds = 3.5)
        {
            ToastText = text;
            ToastUntil = clock() + seconds;
        }

        // ---------------------------------------------------------------- navigation

        public void Push(MenuView view)
        {
            stack.Add(view);
            Invalidate();
        }

        public void Back()
        {
            if (stack.Count > 1)
                stack.RemoveAt(stack.Count - 1);
            Invalidate();
            Pressed?.Invoke();
        }

        public void Home()
        {
            stack.RemoveRange(1, stack.Count - 1);
            Invalidate();
        }

        public void NextPage()
        {
            MenuPage page = Current;
            if (page.PageIndex + 1 < page.PageCount)
            {
                View.Page++;
                Invalidate();
                Pressed?.Invoke();
            }
        }

        public void PreviousPage()
        {
            if (View.Page > 0)
            {
                View.Page--;
                Invalidate();
                Pressed?.Invoke();
            }
        }

        public void PressRow(int index)
        {
            MenuPage page = Current;
            if (index < 0 || index >= page.Rows.Count) return;
            Action press = page.Rows[index].OnPress;
            if (press == null) return;
            press();
            Invalidate();
            Pressed?.Invoke();
        }

        public void PressRowDetail(int index)
        {
            MenuPage page = Current;
            if (index < 0 || index >= page.Rows.Count) return;
            Action press = page.Rows[index].OnPressDetail;
            if (press == null) return;
            press();
            Invalidate();
            Pressed?.Invoke();
        }

        public void PressKey(string key)
        {
            if (key == KeyBackspace)
            {
                if (Query.Length > 0) Query = Query.Substring(0, Query.Length - 1);
            }
            else if (key == KeySpace)
            {
                if (Query.Length > 0 && Query.Length < MaxQueryLength && !Query.EndsWith(" ", StringComparison.Ordinal))
                    Query += " ";
            }
            else if (!string.IsNullOrEmpty(key) && key.Length == 1 && char.IsLetterOrDigit(key[0]))
            {
                if (Query.Length < MaxQueryLength) Query += key.ToLowerInvariant();
            }
            else
            {
                return;
            }

            if (View.Kind == ViewKind.SearchResults)
                View.Page = 0;
            Invalidate();
            Pressed?.Invoke();
        }

        public void ClearQuery()
        {
            Query = string.Empty;
            Invalidate();
        }

        // ---------------------------------------------------------------- page building

        private MenuPage Build()
        {
            MenuView view = View;
            var page = new MenuPage { CanGoBack = stack.Count > 1 };
            GateResult gate = registry.Gate;
            page.Status = gate.Allowed ? $"{gate.Reason}: all mods available" : $"{gate.Reason}: movement mods locked";
            page.StatusIsWarning = !gate.Allowed;

            var rows = new List<MenuRow>();
            bool paged = true;

            switch (view.Kind)
            {
                case ViewKind.Home:
                    page.Title = "Saucy Menu";
                    BuildHome(rows);
                    break;

                case ViewKind.Category:
                    page.Title = view.Arg;
                    rows.AddRange(registry.InCategory(view.Arg).Select(ModuleRow));
                    break;

                case ViewKind.Favorites:
                    page.Title = "Favorites";
                    rows.AddRange(settings.Data.Favorites.Select(registry.Get).Where(m => m != null).Select(ModuleRow));
                    if (rows.Count == 0)
                        rows.Add(Info("No favorites yet. Open a mod's ... page and star it."));
                    break;

                case ViewKind.Enabled:
                    page.Title = "Enabled";
                    rows.AddRange(registry.Modules.Where(m => m.Enabled).Select(ModuleRow));
                    if (rows.Count == 0)
                        rows.Add(Info("Nothing is turned on."));
                    break;

                case ViewKind.Search:
                    page.Title = "Search";
                    page.Keyboard = KeyboardLayout;
                    page.Query = Query;
                    BuildSearch(rows);
                    paged = false;
                    break;

                case ViewKind.SearchResults:
                    page.Title = $"Results: \"{Query}\"";
                    rows.AddRange(ModuleSearch.Find(registry.Modules, Query).Select(ModuleRow));
                    if (rows.Count == 0)
                        rows.Add(Info("No matches."));
                    break;

                case ViewKind.Settings:
                    page.Title = "Settings";
                    BuildSettings(rows);
                    break;

                case ViewKind.ModuleDetail:
                    ModuleBase module = registry.Get(view.Arg);
                    if (module == null)
                    {
                        page.Title = "Missing mod";
                        rows.Add(Info("This mod is no longer available."));
                    }
                    else
                    {
                        page.Title = module.Name;
                        BuildDetail(module, rows);
                    }
                    break;

                case ViewKind.About:
                    page.Title = "About";
                    rows.Add(Info($"Saucy Menu {version}"));
                    rows.Add(Info("Local-only: no telemetry, no servers, no downloads."));
                    rows.Add(Info("Movement mods only run offline, in private rooms or modded queues."));
                    rows.Add(Info("Not affiliated with or endorsed by Another Axiom."));
                    rows.Add(Info("Free software under GPL-3.0."));
                    break;
            }

            if (paged)
            {
                page.PageCount = Math.Max(1, (rows.Count + RowsPerPage - 1) / RowsPerPage);
                if (view.Page >= page.PageCount) view.Page = page.PageCount - 1;
                if (view.Page < 0) view.Page = 0;
                page.PageIndex = view.Page;
                page.Rows.AddRange(rows.Skip(view.Page * RowsPerPage).Take(RowsPerPage));
            }
            else
            {
                page.Rows.AddRange(rows);
            }

            return page;
        }

        private void BuildHome(List<MenuRow> rows)
        {
            rows.Add(Nav("Search", null, () => Push(new MenuView(ViewKind.Search))));
            rows.Add(Nav("Favorites", CountFavorites().ToString(), () => Push(new MenuView(ViewKind.Favorites))));
            rows.Add(Nav("Enabled", registry.EnabledCount.ToString(), () => Push(new MenuView(ViewKind.Enabled))));
            foreach (string category in registry.Categories)
            {
                string c = category;
                rows.Add(Nav(c, registry.InCategory(c).Count().ToString(), () => Push(new MenuView(ViewKind.Category, c))));
            }
            rows.Add(Nav("Settings", null, () => Push(new MenuView(ViewKind.Settings))));
        }

        private void BuildSearch(List<MenuRow> rows)
        {
            if (string.IsNullOrWhiteSpace(Query))
            {
                rows.Add(Info("Type with the keyboard to find a mod."));
                return;
            }

            List<ModuleBase> found = ModuleSearch.Find(registry.Modules, Query);
            if (found.Count == 0)
                rows.Add(Info("No matches."));

            rows.AddRange(found.Take(SearchPreviewRows).Select(ModuleRow));

            if (found.Count > SearchPreviewRows)
                rows.Add(Nav("All results", found.Count.ToString(), () => Push(new MenuView(ViewKind.SearchResults))));

            rows.Add(new MenuRow { Kind = RowKind.Action, Label = "Clear search", OnPress = ClearQuery });
        }

        private void BuildSettings(List<MenuRow> rows)
        {
            SettingsData d = settings.Data;
            rows.Add(Value("Theme", ThemeLibrary.Get(d.ThemeId).Name, () =>
            {
                d.ThemeId = ThemeLibrary.Next(d.ThemeId).Id;
                SettingsTouched(appearance: true);
            }));
            rows.Add(Value("Menu hand", d.MenuHand.ToString(), () =>
            {
                d.MenuHand = d.MenuHand == MenuHand.Left ? MenuHand.Right : MenuHand.Left;
                SettingsTouched(appearance: true);
            }));
            rows.Add(Value("Menu size", $"{Math.Round(d.MenuScale * 100)}%", () =>
            {
                d.MenuScale = NextScale(d.MenuScale);
                SettingsTouched(appearance: true);
            }));
            rows.Add(Value("Flip menu facing", OnOff(d.FlipMenu), () =>
            {
                d.FlipMenu = !d.FlipMenu;
                SettingsTouched(appearance: true);
            }));
            rows.Add(Value("Button sounds", OnOff(d.ButtonSounds), () =>
            {
                d.ButtonSounds = !d.ButtonSounds;
                SettingsTouched(false);
            }));
            rows.Add(Value("Haptics", OnOff(d.Haptics), () =>
            {
                d.Haptics = !d.Haptics;
                SettingsTouched(false);
            }));
            rows.Add(Value("Remember enabled mods", OnOff(d.RememberEnabledModules), () =>
            {
                d.RememberEnabledModules = !d.RememberEnabledModules;
                SettingsTouched(false);
            }));
            rows.Add(new MenuRow
            {
                Kind = RowKind.Action,
                Label = "Turn all mods off",
                OnPress = () =>
                {
                    foreach (ModuleBase m in registry.Modules.Where(m => m.Enabled).ToList())
                        registry.TrySetEnabled(m.Id, false, out _);
                    ShowToast("All mods turned off");
                }
            });

            bool confirming = resetConfirmUntil > 0 && clock() <= resetConfirmUntil;
            rows.Add(new MenuRow
            {
                Kind = RowKind.Action,
                Label = confirming ? "Press again to reset everything" : "Reset settings",
                OnPress = () =>
                {
                    if (resetConfirmUntil > 0 && clock() <= resetConfirmUntil)
                    {
                        resetConfirmUntil = 0;
                        foreach (ModuleBase m in registry.Modules.Where(m => m.Enabled).ToList())
                            registry.TrySetEnabled(m.Id, false, out _);
                        settings.ResetToDefaults(clock());
                        AppearanceChanged?.Invoke();
                        ShowToast("Settings reset");
                    }
                    else
                    {
                        resetConfirmUntil = clock() + 4.0;
                    }
                }
            });
            rows.Add(Nav("About", null, () => Push(new MenuView(ViewKind.About))));
        }

        private void BuildDetail(ModuleBase module, List<MenuRow> rows)
        {
            rows.Add(Info(module.Description));

            bool blocked = !module.Enabled && !registry.CanRun(module);
            rows.Add(new MenuRow
            {
                Kind = RowKind.Value,
                Label = "Enabled",
                Value = blocked ? "Locked here" : OnOff(module.Enabled),
                On = module.Enabled,
                Blocked = blocked,
                ModuleId = module.Id,
                OnPress = () => ToggleModule(module.Id)
            });

            bool fav = settings.IsFavorite(module.Id);
            rows.Add(new MenuRow
            {
                Kind = RowKind.Value,
                Label = "Favorite",
                Value = fav ? "Yes" : "No",
                Favorite = fav,
                OnPress = () => settings.ToggleFavorite(module.Id, clock())
            });

            foreach (ModuleOption option in module.Options)
            {
                ModuleOption o = option;
                rows.Add(Value(o.Name, registry.GetOptionValue(module, o), () => registry.CycleOption(module.Id, o.Id)));
            }

            rows.Add(Info(module.Safety.Describe()));
            if (!string.IsNullOrEmpty(module.LastError))
                rows.Add(Info("Last error: " + module.LastError));
        }

        private MenuRow ModuleRow(ModuleBase module)
        {
            string id = module.Id;
            return new MenuRow
            {
                Kind = RowKind.Module,
                Label = module.Name,
                On = module.Enabled,
                Blocked = !module.Enabled && !registry.CanRun(module),
                Favorite = settings.IsFavorite(id),
                Badge = module.Safety.Badge(),
                ModuleId = id,
                OnPress = () => ToggleModule(id),
                OnPressDetail = () => Push(new MenuView(ViewKind.ModuleDetail, id))
            };
        }

        private void ToggleModule(string id)
        {
            if (!registry.Toggle(id, out string message) && message != null)
                ShowToast(message);
        }

        private int CountFavorites() => settings.Data.Favorites.Count(id => registry.Get(id) != null);

        private void SettingsTouched(bool appearance)
        {
            settings.MarkDirty(clock());
            if (appearance)
                AppearanceChanged?.Invoke();
        }

        internal static float NextScale(float current)
        {
            float[] steps = SettingsData.ScaleSteps;
            for (int i = 0; i < steps.Length; i++)
            {
                if (current < steps[i] - 0.001f)
                    return steps[i];
            }
            return steps[0];
        }

        private static string OnOff(bool b) => b ? "On" : "Off";

        private static MenuRow Info(string text) => new MenuRow { Kind = RowKind.Info, Label = text ?? string.Empty };

        private static MenuRow Nav(string label, string value, Action press) =>
            new MenuRow { Kind = RowKind.Navigate, Label = label, Value = value, OnPress = press };

        private static MenuRow Value(string label, string value, Action press) =>
            new MenuRow { Kind = RowKind.Value, Label = label, Value = value, OnPress = press };
    }
}
