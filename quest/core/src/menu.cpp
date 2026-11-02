#include "saucy/menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "saucy/search.hpp"

namespace saucy {

const char* const MenuController::kKeySpace = "SPACE";
const char* const MenuController::kKeyBackspace = "BACK";

const std::vector<std::vector<std::string>>& MenuController::keyboardLayout() {
    static const std::vector<std::vector<std::string>> layout = {
        {"A", "B", "C", "D", "E", "F", "G"},
        {"H", "I", "J", "K", "L", "M", "N"},
        {"O", "P", "Q", "R", "S", "T", "U"},
        {"V", "W", "X", "Y", "Z", kKeySpace, kKeyBackspace},
    };
    return layout;
}

namespace {
std::string onOff(bool b) { return b ? "On" : "Off"; }

MenuRow info(std::string text) {
    MenuRow r;
    r.kind = RowKind::Info;
    r.label = std::move(text);
    return r;
}

MenuRow nav(std::string label, std::string value, std::function<void()> press) {
    MenuRow r;
    r.kind = RowKind::Navigate;
    r.label = std::move(label);
    r.value = std::move(value);
    r.press = std::move(press);
    return r;
}

MenuRow valueRow(std::string label, std::string value, std::function<void()> press) {
    MenuRow r;
    r.kind = RowKind::Value;
    r.label = std::move(label);
    r.value = std::move(value);
    r.press = std::move(press);
    return r;
}

MenuRow action(std::string label, std::function<void()> press) {
    MenuRow r;
    r.kind = RowKind::Action;
    r.label = std::move(label);
    r.press = std::move(press);
    return r;
}
}  // namespace

MenuController::MenuController(ModuleRegistry& registry, SettingsStore& settings, ModuleRegistry::Clock clock, std::string version)
    : registry_(registry), settings_(settings), clock_(clock ? std::move(clock) : [] { return 0.0; }), version_(std::move(version)) {
    stack_.push_back(MenuView{});
    registry_.onChanged = [this] { invalidate(); };
    registry_.onNotice = [this](const std::string& text) { showToast(text); };
}

const MenuPage& MenuController::current() {
    if (cachedRevision_ != revision_) {
        cached_ = build();
        cachedRevision_ = revision_;
    }
    return cached_;
}

void MenuController::poll() {
    std::string reason = registry_.gate().reason;
    if (!gatePolled_ || reason != lastGateReason_) {
        gatePolled_ = true;
        lastGateReason_ = reason;
        invalidate();
    }
    if (resetConfirmUntil_ > 0 && clock_() > resetConfirmUntil_) {
        resetConfirmUntil_ = 0;
        invalidate();
    }
}

void MenuController::showToast(const std::string& text, double seconds) {
    toastText_ = text;
    toastUntil_ = clock_() + seconds;
}

void MenuController::firePressed() {
    if (onPressed) onPressed();
}

// ------------------------------------------------------------------ navigation

void MenuController::push(MenuView v) {
    stack_.push_back(std::move(v));
    invalidate();
}

void MenuController::back() {
    if (stack_.size() > 1) stack_.pop_back();
    invalidate();
    firePressed();
}

void MenuController::home() {
    stack_.resize(1);
    invalidate();
}

void MenuController::nextPage() {
    const MenuPage& page = current();
    if (page.pageIndex + 1 < page.pageCount) {
        stack_.back().page++;
        invalidate();
        firePressed();
    }
}

void MenuController::previousPage() {
    if (stack_.back().page > 0) {
        stack_.back().page--;
        invalidate();
        firePressed();
    }
}

void MenuController::pressRow(int index) {
    const MenuPage& page = current();
    if (index < 0 || index >= static_cast<int>(page.rows.size())) return;
    auto press = page.rows[index].press;  // copy: the page may be rebuilt by what it does
    if (!press) return;
    press();
    invalidate();
    firePressed();
}

void MenuController::pressRowDetail(int index) {
    const MenuPage& page = current();
    if (index < 0 || index >= static_cast<int>(page.rows.size())) return;
    auto press = page.rows[index].pressDetail;
    if (!press) return;
    press();
    invalidate();
    firePressed();
}

void MenuController::pressKey(const std::string& key) {
    if (key == kKeyBackspace) {
        if (!query_.empty()) query_.pop_back();
    } else if (key == kKeySpace) {
        if (!query_.empty() && query_.size() < kMaxQueryLength && query_.back() != ' ') query_ += ' ';
    } else if (key.size() == 1 && std::isalnum(static_cast<unsigned char>(key[0]))) {
        if (query_.size() < kMaxQueryLength) query_ += static_cast<char>(std::tolower(static_cast<unsigned char>(key[0])));
    } else {
        return;
    }
    if (stack_.back().kind == ViewKind::SearchResults) stack_.back().page = 0;
    invalidate();
    firePressed();
}

void MenuController::clearQuery() {
    query_.clear();
    invalidate();
}

float MenuController::nextScale(float current) {
    const auto& steps = SettingsData::scaleSteps();
    for (float s : steps)
        if (current < s - 0.001f) return s;
    return steps.front();
}

// ------------------------------------------------------------------ building

MenuPage MenuController::build() {
    MenuView& v = stack_.back();
    MenuPage page;
    page.canGoBack = stack_.size() > 1;
    GateResult g = registry_.gate();
    page.status = g.reason + (g.allowed ? ": all mods available" : ": movement mods locked");
    page.statusIsWarning = !g.allowed;

    std::vector<MenuRow> rows;
    bool paged = true;
    auto all = [&] {
        std::vector<Module*> out;
        for (const auto& m : registry_.modules()) out.push_back(m.get());
        return out;
    };

    switch (v.kind) {
        case ViewKind::Home:
            page.title = "Saucy Menu";
            buildHome(rows);
            break;
        case ViewKind::Category:
            page.title = v.arg;
            for (Module* m : registry_.inCategory(v.arg)) rows.push_back(moduleRow(*m));
            break;
        case ViewKind::Favorites:
            page.title = "Favorites";
            for (const auto& id : settings_.data().favorites)
                if (Module* m = registry_.get(id)) rows.push_back(moduleRow(*m));
            if (rows.empty()) rows.push_back(info("No favorites yet. Open a mod's ... page and star it."));
            break;
        case ViewKind::Enabled:
            page.title = "Enabled";
            for (const auto& m : registry_.modules())
                if (m->enabled()) rows.push_back(moduleRow(*m));
            if (rows.empty()) rows.push_back(info("Nothing is turned on."));
            break;
        case ViewKind::Search:
            page.title = "Search";
            page.keyboard = &keyboardLayout();
            page.query = query_;
            buildSearch(rows);
            paged = false;
            break;
        case ViewKind::SearchResults:
            page.title = "Results: \"" + query_ + "\"";
            for (Module* m : searchModules(all(), query_)) rows.push_back(moduleRow(*m));
            if (rows.empty()) rows.push_back(info("No matches."));
            break;
        case ViewKind::Settings:
            page.title = "Settings";
            buildSettings(rows);
            break;
        case ViewKind::ModuleDetail:
            if (Module* m = registry_.get(v.arg)) {
                page.title = m->name();
                buildDetail(*m, rows);
            } else {
                page.title = "Missing mod";
                rows.push_back(info("This mod is no longer available."));
            }
            break;
        case ViewKind::About:
            page.title = "About";
            rows.push_back(info("Saucy Menu " + version_));
            rows.push_back(info("Local-only: no telemetry, no servers, no downloads."));
            rows.push_back(info("Movement mods only run offline, in private rooms or modded queues."));
            rows.push_back(info("Not affiliated with or endorsed by Another Axiom."));
            rows.push_back(info("Free software under GPL-3.0."));
            break;
    }

    if (paged) {
        int count = static_cast<int>(rows.size());
        page.pageCount = std::max(1, (count + kRowsPerPage - 1) / kRowsPerPage);
        v.page = std::clamp(v.page, 0, page.pageCount - 1);
        page.pageIndex = v.page;
        int start = v.page * kRowsPerPage;
        int end = std::min(count, start + kRowsPerPage);
        for (int i = start; i < end; ++i) page.rows.push_back(std::move(rows[i]));
    } else {
        page.rows = std::move(rows);
    }
    return page;
}

void MenuController::buildHome(std::vector<MenuRow>& rows) {
    int favs = 0;
    for (const auto& id : settings_.data().favorites)
        if (registry_.get(id)) ++favs;

    rows.push_back(nav("Search", "", [this] { push({ViewKind::Search}); }));
    rows.push_back(nav("Favorites", std::to_string(favs), [this] { push({ViewKind::Favorites}); }));
    rows.push_back(nav("Enabled", std::to_string(registry_.enabledCount()), [this] { push({ViewKind::Enabled}); }));
    for (const auto& c : registry_.categories()) {
        std::string category = c;
        rows.push_back(nav(category, std::to_string(registry_.inCategory(category).size()),
                           [this, category] { push({ViewKind::Category, category}); }));
    }
    rows.push_back(nav("Settings", "", [this] { push({ViewKind::Settings}); }));
}

void MenuController::buildSearch(std::vector<MenuRow>& rows) {
    if (tokenize(query_).empty()) {
        rows.push_back(info("Type with the keyboard to find a mod."));
        return;
    }
    std::vector<Module*> all;
    for (const auto& m : registry_.modules()) all.push_back(m.get());
    auto found = searchModules(all, query_);
    if (found.empty()) rows.push_back(info("No matches."));
    for (size_t i = 0; i < found.size() && i < static_cast<size_t>(kSearchPreviewRows); ++i) rows.push_back(moduleRow(*found[i]));
    if (found.size() > static_cast<size_t>(kSearchPreviewRows))
        rows.push_back(nav("All results", std::to_string(found.size()), [this] { push({ViewKind::SearchResults}); }));
    rows.push_back(action("Clear search", [this] { clearQuery(); }));
}

void MenuController::buildSettings(std::vector<MenuRow>& rows) {
    SettingsData& d = settings_.data();
    rows.push_back(valueRow("Theme", themeById(d.themeId).name, [this] {
        settings_.data().themeId = nextTheme(settings_.data().themeId).id;
        settingsTouched(true);
    }));
    rows.push_back(valueRow("Menu hand", d.menuHand == MenuHand::Left ? "Left" : "Right", [this] {
        auto& h = settings_.data().menuHand;
        h = h == MenuHand::Left ? MenuHand::Right : MenuHand::Left;
        settingsTouched(true);
    }));
    rows.push_back(valueRow("Menu size", std::to_string(static_cast<int>(std::lround(d.menuScale * 100))) + "%", [this] {
        settings_.data().menuScale = nextScale(settings_.data().menuScale);
        settingsTouched(true);
    }));
    rows.push_back(valueRow("Flip menu facing", onOff(d.flipMenu), [this] {
        settings_.data().flipMenu = !settings_.data().flipMenu;
        settingsTouched(true);
    }));
    rows.push_back(valueRow("Button sounds", onOff(d.buttonSounds), [this] {
        settings_.data().buttonSounds = !settings_.data().buttonSounds;
        settingsTouched(false);
    }));
    rows.push_back(valueRow("Haptics", onOff(d.haptics), [this] {
        settings_.data().haptics = !settings_.data().haptics;
        settingsTouched(false);
    }));
    rows.push_back(valueRow("Remember enabled mods", onOff(d.rememberEnabledModules), [this] {
        settings_.data().rememberEnabledModules = !settings_.data().rememberEnabledModules;
        settingsTouched(false);
    }));
    rows.push_back(action("Turn all mods off", [this] {
        for (const auto& m : registry_.modules())
            if (m->enabled()) registry_.setEnabled(m->id(), false);
        showToast("All mods turned off");
    }));

    bool confirming = resetConfirmUntil_ > 0 && clock_() <= resetConfirmUntil_;
    rows.push_back(action(confirming ? "Press again to reset everything" : "Reset settings", [this] {
        if (resetConfirmUntil_ > 0 && clock_() <= resetConfirmUntil_) {
            resetConfirmUntil_ = 0;
            for (const auto& m : registry_.modules())
                if (m->enabled()) registry_.setEnabled(m->id(), false);
            settings_.resetToDefaults(clock_());
            if (onAppearanceChanged) onAppearanceChanged();
            showToast("Settings reset");
        } else {
            resetConfirmUntil_ = clock_() + 4.0;
        }
    }));
    rows.push_back(nav("About", "", [this] { push({ViewKind::About}); }));
}

void MenuController::buildDetail(Module& module, std::vector<MenuRow>& rows) {
    const std::string id = module.id();
    rows.push_back(info(module.description()));

    bool blocked = !module.enabled() && !registry_.canRun(module);
    MenuRow enabled = valueRow("Enabled", blocked ? "Locked here" : onOff(module.enabled()), [this, id] { toggleModule(id); });
    enabled.on = module.enabled();
    enabled.blocked = blocked;
    enabled.moduleId = id;
    rows.push_back(std::move(enabled));

    bool fav = settings_.isFavorite(id);
    MenuRow favorite = valueRow("Favorite", fav ? "Yes" : "No", [this, id] { settings_.toggleFavorite(id, clock_()); });
    favorite.favorite = fav;
    rows.push_back(std::move(favorite));

    for (const auto& option : module.options()) {
        std::string optionId = option.id;
        rows.push_back(valueRow(option.name, registry_.optionValue(module, option), [this, id, optionId] { registry_.cycleOption(id, optionId); }));
    }

    rows.push_back(info(describe(module.safety())));
    if (!module.lastError().empty()) rows.push_back(info("Last error: " + module.lastError()));
}

MenuRow MenuController::moduleRow(Module& module) {
    const std::string id = module.id();
    MenuRow r;
    r.kind = RowKind::Module;
    r.label = module.name();
    r.on = module.enabled();
    r.blocked = !module.enabled() && !registry_.canRun(module);
    r.favorite = settings_.isFavorite(id);
    r.badge = badge(module.safety());
    r.moduleId = id;
    r.press = [this, id] { toggleModule(id); };
    r.pressDetail = [this, id] { push({ViewKind::ModuleDetail, id}); };
    return r;
}

void MenuController::toggleModule(const std::string& id) {
    std::string message;
    if (!registry_.toggle(id, &message) && !message.empty()) showToast(message);
}

void MenuController::settingsTouched(bool appearance) {
    settings_.markDirty(clock_());
    if (appearance && onAppearanceChanged) onAppearanceChanged();
}

}  // namespace saucy
