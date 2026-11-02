#pragma once

#include <map>
#include <string>
#include <vector>

#include "saucy/log.hpp"

namespace saucy {

enum class MenuHand { Left, Right };

/// Everything Saucy Menu persists, as JSON on the headset's mod data folder.
struct SettingsData {
    static constexpr int kSchemaVersion = 1;
    static const std::vector<float>& scaleSteps();

    int schemaVersion = kSchemaVersion;
    std::string themeId = "saucy";
    MenuHand menuHand = MenuHand::Left;
    float menuScale = 1.0f;
    bool flipMenu = false;
    bool buttonSounds = true;
    bool haptics = true;
    bool rememberEnabledModules = true;
    std::vector<std::string> favorites;
    std::vector<std::string> enabledModules;
    std::map<std::string, std::map<std::string, std::string>> moduleOptions;

    void sanitize();
};

/// Loads/saves SettingsData. Saves are batched (flushIfDirty) and atomic (temp file + rename);
/// an unreadable file is moved aside and defaults are used.
class SettingsStore {
public:
    explicit SettingsStore(std::string path, ILog* log = nullptr);

    SettingsData& data() { return data_; }
    const SettingsData& data() const { return data_; }
    const std::string& path() const { return path_; }
    bool isDirty() const { return dirty_; }

    void load();
    bool save();
    void markDirty(double now);
    bool flushIfDirty(double now, double delaySeconds = 1.0);
    void resetToDefaults(double now);

    bool isFavorite(const std::string& moduleId) const;
    bool toggleFavorite(const std::string& moduleId, double now);
    const std::string* getModuleOption(const std::string& moduleId, const std::string& optionId) const;
    void setModuleOption(const std::string& moduleId, const std::string& optionId, const std::string& value, double now);
    void setModuleEnabled(const std::string& moduleId, bool enabled, double now);

    /// Exposed for tests.
    static std::string toJson(const SettingsData& data);
    static bool fromJson(const std::string& json, SettingsData& out, std::string& error);

private:
    std::string path_;
    ILog* log_;
    SettingsData data_;
    bool dirty_ = false;
    double lastChange_ = 0;
};

}  // namespace saucy
