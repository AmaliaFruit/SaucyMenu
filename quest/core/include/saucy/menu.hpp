#pragma once

#include <functional>
#include <string>
#include <vector>

#include "saucy/registry.hpp"
#include "saucy/settings.hpp"
#include "saucy/theme.hpp"

namespace saucy {

enum class ViewKind { Home, Category, Favorites, Enabled, Search, SearchResults, Settings, ModuleDetail, About };
enum class RowKind { Navigate, Module, Action, Value, Info };

struct MenuView {
    MenuView(ViewKind k = ViewKind::Home, std::string a = {}) : kind(k), arg(std::move(a)) {}
    ViewKind kind = ViewKind::Home;
    std::string arg;  ///< category name or module id
    int page = 0;
};

struct MenuRow {
    RowKind kind = RowKind::Info;
    std::string label;
    std::string value;
    bool on = false;
    bool blocked = false;
    bool favorite = false;
    std::string badge;
    std::string moduleId;
    std::function<void()> press;
    std::function<void()> pressDetail;

    bool pressable() const { return static_cast<bool>(press); }
    bool hasDetail() const { return static_cast<bool>(pressDetail); }
};

struct MenuPage {
    std::string title;
    std::string status;
    bool statusIsWarning = false;
    std::vector<MenuRow> rows;
    int pageIndex = 0;
    int pageCount = 1;
    bool canGoBack = false;
    const std::vector<std::vector<std::string>>* keyboard = nullptr;
    std::string query;
};

/// The whole menu as a state machine. The VR renderer draws current() and forwards presses here;
/// every state change bumps revision() so the renderer knows when to redraw.
class MenuController {
public:
    static constexpr int kRowsPerPage = 7;
    static constexpr int kSearchPreviewRows = 3;
    static constexpr size_t kMaxQueryLength = 24;
    static const char* const kKeySpace;
    static const char* const kKeyBackspace;
    static const std::vector<std::vector<std::string>>& keyboardLayout();

    MenuController(ModuleRegistry& registry, SettingsStore& settings, ModuleRegistry::Clock clock, std::string version);
    MenuController(const MenuController&) = delete;  // registers callbacks that capture `this`
    MenuController& operator=(const MenuController&) = delete;

    std::function<void()> onAppearanceChanged;
    std::function<void()> onPressed;

    int revision() const { return revision_; }
    const std::string& query() const { return query_; }
    const std::string& toastText() const { return toastText_; }
    double toastUntil() const { return toastUntil_; }
    const MenuView& view() const { return stack_.back(); }
    const Theme& theme() const { return themeById(settings_.data().themeId); }
    const SettingsData& settings() const { return settings_.data(); }

    const MenuPage& current();
    void invalidate() { ++revision_; }
    void poll();
    void showToast(const std::string& text, double seconds = 3.5);

    void push(MenuView view);
    void back();
    void home();
    void nextPage();
    void previousPage();
    void pressRow(int index);
    void pressRowDetail(int index);
    void pressKey(const std::string& key);
    void clearQuery();

    static float nextScale(float current);

private:
    MenuPage build();
    void buildHome(std::vector<MenuRow>& rows);
    void buildSearch(std::vector<MenuRow>& rows);
    void buildSettings(std::vector<MenuRow>& rows);
    void buildDetail(Module& module, std::vector<MenuRow>& rows);
    MenuRow moduleRow(Module& module);
    void toggleModule(const std::string& id);
    void settingsTouched(bool appearance);
    void firePressed();

    ModuleRegistry& registry_;
    SettingsStore& settings_;
    ModuleRegistry::Clock clock_;
    std::string version_;
    std::vector<MenuView> stack_;
    MenuPage cached_;
    int cachedRevision_ = -1;
    int revision_ = 0;
    std::string lastGateReason_;
    bool gatePolled_ = false;
    double resetConfirmUntil_ = 0;
    std::string query_;
    std::string toastText_;
    double toastUntil_ = 0;
};

}  // namespace saucy
