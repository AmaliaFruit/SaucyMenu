// Host-side tests for the Quest core (no Unity, no Android). Same cases as the C# suite.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "saucy/menu.hpp"
#include "saucy/registry.hpp"
#include "saucy/room_gate.hpp"
#include "saucy/search.hpp"
#include "saucy/settings.hpp"
#include "saucy/theme.hpp"

using namespace saucy;
namespace fs = std::filesystem;

// ------------------------------------------------------------------ tiny harness
struct TestCase { const char* name; std::function<void()> fn; };
static std::vector<TestCase>& tests() { static std::vector<TestCase> t; return t; }
struct Register { Register(const char* n, std::function<void()> f) { tests().push_back({n, std::move(f)}); } };
struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
#define TEST(name) static void name(); static Register reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) throw Failure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " CHECK(" #cond ")"); } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { std::ostringstream _o; _o << __FILE__ << ":" << __LINE__ << " CHECK_EQ(" #a ", " #b ")"; throw Failure(_o.str()); } } while (0)
#define CHECK_THROWS(expr) do { bool _t = false; try { expr; } catch (...) { _t = true; } CHECK(_t); } while (0)

// ------------------------------------------------------------------ fakes
struct FakeRoom : IRoomState {
    bool in = false, priv = false, throws = false;
    std::string mode;
    bool inRoom() const override { if (throws) throw std::runtime_error("boom"); return in; }
    bool isPrivate() const override { return priv; }
    std::string gameMode() const override { return mode; }
    static FakeRoom Public() { FakeRoom r; r.in = true; r.mode = "DEFAULT|INFECTION"; return r; }
    static FakeRoom Private() { FakeRoom r; r.in = true; r.priv = true; r.mode = "DEFAULT|CASUAL"; return r; }
};

struct FakeModule : Module {
    std::string id_, name_, category_, description_;
    ModuleSafety safety_;
    std::vector<std::string> tags_;
    std::vector<ModuleOption> options_;
    int enables = 0, disables = 0, updates = 0, prelocos = 0, lates = 0, optionChanges = 0;
    bool throwOnEnable = false, throwOnUpdate = false;

    FakeModule(std::string id, std::string name = "", std::string category = "Test", ModuleSafety safety = ModuleSafety::LocalInfo,
               std::string description = "A test module", std::vector<std::string> tags = {}, std::vector<ModuleOption> options = {})
        : id_(id), name_(name.empty() ? id : name), category_(category), description_(description), safety_(safety), tags_(tags), options_(options) {}

    std::string id() const override { return id_; }
    std::string name() const override { return name_; }
    std::string description() const override { return description_; }
    std::string category() const override { return category_; }
    ModuleSafety safety() const override { return safety_; }
    std::vector<std::string> tags() const override { return tags_; }
    const std::vector<ModuleOption>& options() const override { return options_; }
    std::string read(const std::string& o) const { return getOption(o); }

    void onEnable() override { ++enables; if (throwOnEnable) throw std::runtime_error("enable failed"); }
    void onDisable() override { ++disables; }
    void onUpdate() override { ++updates; if (throwOnUpdate) throw std::runtime_error("update failed"); }
    void onPreLocomotion() override { ++prelocos; }
    void onLateUpdate() override { ++lates; }
    void onOptionChanged(const std::string&) override { ++optionChanges; }
};

struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / ("saucy-tests-" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    std::string file(const std::string& n) const { return (path / n).string(); }
};

struct Harness {
    double now = 0;
    TempDir dir;
    FakeRoom room;
    SettingsStore settings;
    ModuleRegistry registry;
    std::vector<std::string> notices;
    explicit Harness(FakeRoom r = FakeRoom{})
        : room(r), settings(dir.file("settings.json")), registry(settings, &room, nullptr, [this] { return now; }) {
        registry.onNotice = [this](const std::string& n) { notices.push_back(n); };
    }
    template <typename... A> FakeModule& add(A&&... a) {
        return static_cast<FakeModule&>(registry.add(std::make_unique<FakeModule>(std::forward<A>(a)...)));
    }
};

static bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

// ------------------------------------------------------------------ room gate
TEST(gate_offline_allowed) { FakeRoom r; CHECK(evaluateGate(&r).allowed); }
TEST(gate_private_allowed) { auto r = FakeRoom::Private(); CHECK(evaluateGate(&r).allowed); }
TEST(gate_public_blocked) { auto r = FakeRoom::Public(); auto g = evaluateGate(&r); CHECK(!g.allowed); CHECK_EQ(g.reason, std::string("Public lobby")); }
TEST(gate_modded_allowed) { FakeRoom r; r.in = true; r.mode = "DEFAULT|modded_casual"; CHECK(evaluateGate(&r).allowed); }
TEST(gate_null_fails_closed) { CHECK(!evaluateGate(nullptr).allowed); }
TEST(gate_throwing_fails_closed) { FakeRoom r; r.throws = true; CHECK(!evaluateGate(&r).allowed); }
TEST(gate_only_private_modules_gated) {
    auto r = FakeRoom::Public();
    CHECK(canRun(ModuleSafety::LocalInfo, &r));
    CHECK(canRun(ModuleSafety::LocalVisual, &r));
    CHECK(!canRun(ModuleSafety::PrivateRoomOnly, &r));
}

// ------------------------------------------------------------------ registry
TEST(registry_duplicate_rejected) { Harness h; h.add("a"); CHECK_THROWS(h.add("a")); }
TEST(registry_categories_in_order) {
    Harness h; h.add("a", "a", "Info"); h.add("b", "b", "Movement"); h.add("c", "c", "Info");
    CHECK_EQ(h.registry.categories(), (std::vector<std::string>{"Info", "Movement"}));
}
TEST(registry_enable_disable_persist) {
    Harness h; auto& m = h.add("a");
    CHECK(h.registry.setEnabled("a", true));
    CHECK(m.enabled()); CHECK_EQ(m.enables, 1); CHECK(contains(h.settings.data().enabledModules, "a"));
    CHECK(h.registry.setEnabled("a", false));
    CHECK(!m.enabled()); CHECK_EQ(m.disables, 1); CHECK(!contains(h.settings.data().enabledModules, "a"));
}
TEST(registry_private_blocked_in_public) {
    Harness h(FakeRoom::Public()); auto& m = h.add("fly", "fly", "Test", ModuleSafety::PrivateRoomOnly);
    std::string msg;
    CHECK(!h.registry.setEnabled("fly", true, &msg));
    CHECK(!m.enabled()); CHECK_EQ(m.enables, 0); CHECK(msg.find("private rooms only") != std::string::npos);
}
TEST(registry_public_join_disables_private) {
    Harness h(FakeRoom::Private());
    auto& fly = h.add("fly", "fly", "Test", ModuleSafety::PrivateRoomOnly);
    auto& hud = h.add("hud");
    h.registry.setEnabled("fly", true); h.registry.setEnabled("hud", true);
    h.room.priv = false;
    h.registry.enforceGate();
    CHECK(!fly.enabled()); CHECK_EQ(fly.disables, 1); CHECK(hud.enabled());
    CHECK_EQ(h.notices.size(), size_t(1)); CHECK(!contains(h.settings.data().enabledModules, "fly"));
}
TEST(registry_throwing_module_isolated) {
    Harness h; auto& bad = h.add("bad"); bad.throwOnUpdate = true; auto& good = h.add("good");
    h.registry.setEnabled("bad", true); h.registry.setEnabled("good", true);
    h.registry.tick(TickPhase::Update); h.registry.tick(TickPhase::Update);
    CHECK(!bad.enabled()); CHECK_EQ(bad.updates, 1); CHECK_EQ(bad.disables, 1); CHECK(!bad.lastError().empty());
    CHECK(good.enabled()); CHECK_EQ(good.updates, 2);
}
TEST(registry_failing_enable_leaves_off) {
    Harness h; auto& bad = h.add("bad"); bad.throwOnEnable = true;
    CHECK(!h.registry.setEnabled("bad", true));
    CHECK(!bad.enabled()); CHECK_EQ(bad.disables, 1); CHECK(!contains(h.settings.data().enabledModules, "bad"));
}
TEST(registry_ticks_reach_hooks) {
    Harness h; auto& m = h.add("a"); h.registry.setEnabled("a", true);
    h.registry.tick(TickPhase::Update); h.registry.tick(TickPhase::PreLocomotion); h.registry.tick(TickPhase::LateUpdate);
    CHECK_EQ(m.updates, 1); CHECK_EQ(m.prelocos, 1); CHECK_EQ(m.lates, 1);
}
TEST(registry_options_cycle_persist) {
    Harness h;
    auto& m = h.add("cam", "cam", "Test", ModuleSafety::LocalInfo, "d", std::vector<std::string>{},
                    std::vector<ModuleOption>{ModuleOption("fov", "FOV", {"60", "75", "90"}, 1)});
    h.registry.setEnabled("cam", true);
    CHECK_EQ(m.read("fov"), std::string("75"));
    CHECK_EQ(h.registry.cycleOption("cam", "fov"), std::string("90"));
    CHECK_EQ(m.read("fov"), std::string("90"));
    CHECK_EQ(h.registry.cycleOption("cam", "fov"), std::string("60"));
    CHECK_EQ(m.optionChanges, 2);
    CHECK_EQ(*h.settings.getModuleOption("cam", "fov"), std::string("60"));
}
TEST(registry_invalid_option_defaults) {
    Harness h;
    auto& m = h.add("cam", "cam", "Test", ModuleSafety::LocalInfo, "d", std::vector<std::string>{},
                    std::vector<ModuleOption>{ModuleOption("fov", "FOV", {"60", "75"}, 1)});
    h.settings.setModuleOption("cam", "fov", "9000", 0);
    CHECK_EQ(m.read("fov"), std::string("75"));
}
TEST(registry_restore_respects_gate) {
    Harness h(FakeRoom::Public());
    h.settings.data().enabledModules = {"hud", "fly", "removed"};
    auto& hud = h.add("hud"); auto& fly = h.add("fly", "fly", "Test", ModuleSafety::PrivateRoomOnly);
    h.registry.restoreFromSettings();
    CHECK(hud.enabled()); CHECK(!fly.enabled());
    CHECK(!contains(h.settings.data().enabledModules, "fly")); CHECK(contains(h.settings.data().enabledModules, "removed"));
}
TEST(registry_restore_off_when_not_remembering) {
    Harness h; h.settings.data().rememberEnabledModules = false; h.settings.data().enabledModules = {"hud"};
    auto& hud = h.add("hud");
    h.registry.restoreFromSettings();
    CHECK(!hud.enabled()); CHECK(h.settings.data().enabledModules.empty());
}

// ------------------------------------------------------------------ settings
TEST(settings_round_trip) {
    TempDir d; SettingsStore s(d.file("s.json"));
    s.data().themeId = "mint"; s.data().menuHand = MenuHand::Right; s.toggleFavorite("fly", 0); s.setModuleOption("cam", "fov", "90", 0);
    CHECK(s.save());
    SettingsStore again(d.file("s.json")); again.load();
    CHECK_EQ(again.data().themeId, std::string("mint")); CHECK(again.data().menuHand == MenuHand::Right);
    CHECK(again.isFavorite("fly")); CHECK_EQ(*again.getModuleOption("cam", "fov"), std::string("90"));
}
TEST(settings_corrupt_moved_aside) {
    TempDir d; { std::ofstream(d.file("s.json")) << "{ not json"; }
    SettingsStore s(d.file("s.json")); s.load();
    CHECK_EQ(s.data().themeId, std::string("saucy")); CHECK(!fs::exists(d.file("s.json")));
    int backups = 0;
    for (auto& e : fs::directory_iterator(d.path)) if (e.path().filename().string().rfind("s.json.corrupt-", 0) == 0) ++backups;
    CHECK_EQ(backups, 1);
}
TEST(settings_sanitize_bad_values) {
    TempDir d; { std::ofstream(d.file("s.json")) << R"({"MenuScale": 99, "Favorites": null, "ThemeId": ""})"; }
    SettingsStore s(d.file("s.json")); s.load();
    CHECK_EQ(s.data().menuScale, 1.0f); CHECK(s.data().favorites.empty()); CHECK_EQ(s.data().themeId, std::string("saucy"));
}
TEST(settings_flush_debounces) {
    TempDir d; SettingsStore s(d.file("s.json"));
    s.markDirty(10);
    CHECK(!s.flushIfDirty(10.5)); CHECK(!fs::exists(d.file("s.json")));
    CHECK(s.flushIfDirty(11.1)); CHECK(fs::exists(d.file("s.json"))); CHECK(!s.isDirty());
}
TEST(settings_json_compatible_with_pc_names) {
    SettingsData d; std::string err;
    CHECK(SettingsStore::fromJson(R"({"ThemeId":"midnight","MenuHand":"Right","ModuleOptions":{"fly":{"speed":"20 m/s"}}})", d, err));
    CHECK_EQ(d.themeId, std::string("midnight")); CHECK(d.menuHand == MenuHand::Right); CHECK_EQ(d.moduleOptions["fly"]["speed"], std::string("20 m/s"));
}

// ------------------------------------------------------------------ search
static std::vector<std::unique_ptr<FakeModule>> searchFixtures() {
    std::vector<std::unique_ptr<FakeModule>> v;
    v.push_back(std::make_unique<FakeModule>("fly", "Fly", "Movement", ModuleSafety::PrivateRoomOnly, "Fly where you look", std::vector<std::string>{"noclip"}));
    v.push_back(std::make_unique<FakeModule>("platforms", "Platforms", "Movement", ModuleSafety::PrivateRoomOnly, "Grip to stand on air"));
    v.push_back(std::make_unique<FakeModule>("perf", "Performance HUD", "Info", ModuleSafety::LocalInfo, "Shows FPS and ping", std::vector<std::string>{"fps"}));
    v.push_back(std::make_unique<FakeModule>("cam", "Smooth Camera", "Visual", ModuleSafety::LocalVisual, "Smooths the desktop view"));
    return v;
}
static std::vector<std::string> ids(const std::vector<Module*>& ms) { std::vector<std::string> o; for (auto* m : ms) o.push_back(m->id()); return o; }
static std::vector<Module*> ptrs(const std::vector<std::unique_ptr<FakeModule>>& v) { std::vector<Module*> o; for (auto& m : v) o.push_back(m.get()); return o; }
TEST(search_prefix_first) { auto f = searchFixtures(); CHECK_EQ(searchModules(ptrs(f), "pla").front()->id(), std::string("platforms")); }
TEST(search_tags) { auto f = searchFixtures(); CHECK_EQ(ids(searchModules(ptrs(f), "fps")), (std::vector<std::string>{"perf"})); }
TEST(search_category) { auto f = searchFixtures(); CHECK_EQ(ids(searchModules(ptrs(f), "movement")), (std::vector<std::string>{"fly", "platforms"})); }
TEST(search_all_terms) { auto f = searchFixtures(); CHECK_EQ(ids(searchModules(ptrs(f), "smooth desk")), (std::vector<std::string>{"cam"})); }
TEST(search_case_insensitive) { auto f = searchFixtures(); CHECK_EQ(searchModules(ptrs(f), "FLY").front()->id(), std::string("fly")); }
TEST(search_empty) { auto f = searchFixtures(); CHECK(searchModules(ptrs(f), "  ").empty()); }

// ------------------------------------------------------------------ themes
TEST(themes_unique_ids) {
    std::vector<std::string> seen;
    for (auto& t : allThemes()) { CHECK(!contains(seen, t.id)); seen.push_back(t.id); }
}
TEST(themes_wrap) { CHECK_EQ(nextTheme(allThemes().back().id).id, allThemes().front().id); }
TEST(themes_fallback) { CHECK_EQ(themeById("nope").id, std::string("saucy")); }
TEST(themes_hex) { CHECK_EQ(Rgba::hex("#c8382c").toHex(), std::string("#C8382C")); CHECK_THROWS(Rgba::hex("#zz0000")); }

// ------------------------------------------------------------------ menu controller
struct MenuHarness {
    Harness h;
    std::unique_ptr<MenuController> menu;
    explicit MenuHarness(FakeRoom room = FakeRoom{}, int extra = 0) : h(room) {
        h.add("perf", "Performance HUD", "Info");
        h.add("fly", "Fly", "Movement", ModuleSafety::PrivateRoomOnly);
        for (int i = 0; i < extra; ++i) h.add("extra" + std::to_string(i), "Extra " + std::to_string(i), "Info");
        menu = std::make_unique<MenuController>(h.registry, h.settings, [this] { return h.now; }, "1.0.0");
    }
    int row(const std::string& label) {
        const auto& rows = menu->current().rows;
        for (size_t i = 0; i < rows.size(); ++i) if (rows[i].label == label) return static_cast<int>(i);
        return -1;
    }
    const MenuRow& rowById(const std::string& id) {
        for (auto& r : menu->current().rows) if (r.moduleId == id) return r;
        throw Failure("row not found: " + id);
    }
};

TEST(menu_home_rows) {
    MenuHarness m;
    std::vector<std::string> labels;
    for (auto& r : m.menu->current().rows) labels.push_back(r.label);
    CHECK_EQ(labels, (std::vector<std::string>{"Search", "Favorites", "Enabled", "Info", "Movement", "Settings"}));
    CHECK(!m.menu->current().canGoBack);
}
TEST(menu_category_toggle) {
    MenuHarness m;
    m.menu->pressRow(m.row("Info"));
    CHECK_EQ(m.menu->current().title, std::string("Info"));
    m.menu->pressRow(m.row("Performance HUD"));
    CHECK(m.h.registry.get("perf")->enabled()); CHECK(m.rowById("perf").on);
    m.menu->back();
    CHECK_EQ(m.menu->current().title, std::string("Saucy Menu"));
    CHECK_EQ(m.menu->current().rows[m.row("Enabled")].value, std::string("1"));
}
TEST(menu_gated_row_blocked_and_toasts) {
    MenuHarness m(FakeRoom::Public());
    m.menu->pressRow(m.row("Movement"));
    CHECK(m.rowById("fly").blocked); CHECK_EQ(m.rowById("fly").badge, std::string("PRIVATE"));
    m.menu->pressRow(0);
    CHECK(!m.h.registry.get("fly")->enabled());
    CHECK(m.menu->toastText().find("private rooms only") != std::string::npos);
    CHECK(m.menu->current().statusIsWarning);
}
TEST(menu_favorite_from_detail) {
    MenuHarness m;
    m.menu->pressRow(m.row("Info"));
    m.menu->pressRowDetail(m.row("Performance HUD"));
    CHECK(m.menu->view().kind == ViewKind::ModuleDetail);
    m.menu->pressRow(m.row("Favorite"));
    CHECK(m.h.settings.isFavorite("perf"));
    m.menu->home();
    m.menu->pressRow(m.row("Favorites"));
    CHECK_EQ(m.menu->current().rows.size(), size_t(1)); CHECK_EQ(m.menu->current().rows[0].label, std::string("Performance HUD"));
}
TEST(menu_search_keyboard) {
    MenuHarness m;
    m.menu->pressRow(m.row("Search"));
    CHECK(m.menu->current().keyboard != nullptr);
    m.menu->pressKey("F"); m.menu->pressKey("L");
    CHECK_EQ(m.menu->query(), std::string("fl"));
    bool found = false; for (auto& r : m.menu->current().rows) if (r.moduleId == "fly") found = true;
    CHECK(found);
    for (int i = 0; i < 3; ++i) m.menu->pressKey(MenuController::kKeyBackspace);
    CHECK_EQ(m.menu->query(), std::string(""));
}
TEST(menu_search_preview_and_all_results) {
    MenuHarness m(FakeRoom{}, 6);
    m.menu->pressRow(m.row("Search"));
    m.menu->pressKey("E"); m.menu->pressKey("X");
    int modules = 0; for (auto& r : m.menu->current().rows) if (r.kind == RowKind::Module) ++modules;
    CHECK_EQ(modules, MenuController::kSearchPreviewRows);
    m.menu->pressRow(m.row("All results"));
    CHECK(m.menu->view().kind == ViewKind::SearchResults);
    CHECK_EQ(m.menu->current().rows.size(), size_t(6));
}
TEST(menu_paging) {
    MenuHarness m(FakeRoom{}, 10);
    m.menu->pressRow(m.row("Info"));
    CHECK_EQ(m.menu->current().pageCount, 2);
    CHECK_EQ(m.menu->current().rows.size(), size_t(MenuController::kRowsPerPage));
    m.menu->nextPage();
    CHECK_EQ(m.menu->current().pageIndex, 1);
    CHECK_EQ(m.menu->current().rows.size(), size_t(11 - MenuController::kRowsPerPage));
    m.menu->nextPage(); CHECK_EQ(m.menu->current().pageIndex, 1);
    m.menu->previousPage(); CHECK_EQ(m.menu->current().pageIndex, 0);
}
TEST(menu_settings_cycle) {
    MenuHarness m; int appearance = 0;
    m.menu->onAppearanceChanged = [&] { ++appearance; };
    m.menu->pressRow(m.row("Settings"));
    m.menu->pressRow(m.row("Theme")); m.menu->pressRow(m.row("Menu hand")); m.menu->pressRow(m.row("Menu size"));
    CHECK_EQ(m.h.settings.data().themeId, std::string("midnight"));
    CHECK(m.h.settings.data().menuHand == MenuHand::Right);
    CHECK_EQ(m.h.settings.data().menuScale, 1.1f);
    CHECK_EQ(appearance, 3); CHECK(m.h.settings.isDirty());
}
TEST(menu_reset_needs_confirm) {
    MenuHarness m; m.h.settings.data().themeId = "mint";
    m.menu->pressRow(m.row("Settings"));
    m.menu->nextPage();
    int reset = m.row("Reset settings");
    CHECK(reset >= 0);
    m.menu->pressRow(reset);
    CHECK_EQ(m.h.settings.data().themeId, std::string("mint"));
    CHECK_EQ(m.menu->current().rows[reset].label, std::string("Press again to reset everything"));
    m.h.now = 10; m.menu->poll();
    CHECK_EQ(m.menu->current().rows[reset].label, std::string("Reset settings"));
    m.menu->pressRow(reset); m.h.now = 11; m.menu->pressRow(reset);
    CHECK_EQ(m.h.settings.data().themeId, std::string("saucy"));
}
TEST(menu_gate_change_bumps_revision) {
    MenuHarness m(FakeRoom::Private());
    m.menu->poll(); int before = m.menu->revision();
    m.h.room.priv = false; m.menu->poll();
    CHECK(m.menu->revision() != before); CHECK(m.menu->current().statusIsWarning);
}

int main() {
    int failed = 0;
    for (auto& t : tests()) {
        try {
            t.fn();
        } catch (const std::exception& e) {
            ++failed;
            std::printf("FAIL %s: %s\n", t.name, e.what());
        }
    }
    std::printf("%s - %zu tests, %d failed\n", failed ? "FAILED" : "PASSED", tests().size(), failed);
    return failed ? 1 : 0;
}
