#include "saucy/settings.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace fs = std::filesystem;

namespace saucy {

const std::vector<float>& SettingsData::scaleSteps() {
    static const std::vector<float> steps = {0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.4f};
    return steps;
}

void SettingsData::sanitize() {
    if (themeId.empty()) themeId = "saucy";
    if (!(menuScale >= 0.5f && menuScale <= 2.0f)) menuScale = 1.0f;
    auto blank = [](const std::string& s) { return s.find_first_not_of(" \t") == std::string::npos; };
    favorites.erase(std::remove_if(favorites.begin(), favorites.end(), blank), favorites.end());
    enabledModules.erase(std::remove_if(enabledModules.begin(), enabledModules.end(), blank), enabledModules.end());
    schemaVersion = kSchemaVersion;
}

SettingsStore::SettingsStore(std::string path, ILog* log) : path_(std::move(path)), log_(log ? log : &NullLog::instance()) {}

// ------------------------------------------------------------------ JSON

std::string SettingsStore::toJson(const SettingsData& d) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
    w.StartObject();
    w.Key("SchemaVersion"); w.Int(d.schemaVersion);
    w.Key("ThemeId"); w.String(d.themeId);
    w.Key("MenuHand"); w.String(d.menuHand == MenuHand::Left ? "Left" : "Right");
    w.Key("MenuScale"); w.Double(d.menuScale);
    w.Key("FlipMenu"); w.Bool(d.flipMenu);
    w.Key("ButtonSounds"); w.Bool(d.buttonSounds);
    w.Key("Haptics"); w.Bool(d.haptics);
    w.Key("RememberEnabledModules"); w.Bool(d.rememberEnabledModules);
    w.Key("Favorites");
    w.StartArray();
    for (const auto& s : d.favorites) w.String(s);
    w.EndArray();
    w.Key("EnabledModules");
    w.StartArray();
    for (const auto& s : d.enabledModules) w.String(s);
    w.EndArray();
    w.Key("ModuleOptions");
    w.StartObject();
    for (const auto& [module, options] : d.moduleOptions) {
        w.Key(module);
        w.StartObject();
        for (const auto& [key, value] : options) {
            w.Key(key);
            w.String(value);
        }
        w.EndObject();
    }
    w.EndObject();
    w.EndObject();
    return buffer.GetString();
}

bool SettingsStore::fromJson(const std::string& json, SettingsData& out, std::string& error) {
    rapidjson::Document doc;
    doc.Parse(json.c_str(), json.size());
    if (doc.HasParseError()) {
        error = std::string(rapidjson::GetParseError_En(doc.GetParseError())) + " at offset " + std::to_string(doc.GetErrorOffset());
        return false;
    }
    if (!doc.IsObject()) {
        error = "root is not an object";
        return false;
    }

    SettingsData d;  // unknown or wrongly typed fields keep their defaults
    auto str = [&](const char* k, std::string& dst) { if (doc.HasMember(k) && doc[k].IsString()) dst = doc[k].GetString(); };
    auto boolean = [&](const char* k, bool& dst) { if (doc.HasMember(k) && doc[k].IsBool()) dst = doc[k].GetBool(); };
    auto list = [&](const char* k, std::vector<std::string>& dst) {
        if (!doc.HasMember(k) || !doc[k].IsArray()) return;
        for (const auto& v : doc[k].GetArray())
            if (v.IsString()) dst.emplace_back(v.GetString());
    };

    if (doc.HasMember("SchemaVersion") && doc["SchemaVersion"].IsInt()) d.schemaVersion = doc["SchemaVersion"].GetInt();
    str("ThemeId", d.themeId);
    if (doc.HasMember("MenuHand") && doc["MenuHand"].IsString())
        d.menuHand = std::string(doc["MenuHand"].GetString()) == "Right" ? MenuHand::Right : MenuHand::Left;
    if (doc.HasMember("MenuScale") && doc["MenuScale"].IsNumber()) d.menuScale = static_cast<float>(doc["MenuScale"].GetDouble());
    boolean("FlipMenu", d.flipMenu);
    boolean("ButtonSounds", d.buttonSounds);
    boolean("Haptics", d.haptics);
    boolean("RememberEnabledModules", d.rememberEnabledModules);
    list("Favorites", d.favorites);
    list("EnabledModules", d.enabledModules);
    if (doc.HasMember("ModuleOptions") && doc["ModuleOptions"].IsObject()) {
        for (const auto& m : doc["ModuleOptions"].GetObject()) {
            if (!m.value.IsObject()) continue;
            auto& opts = d.moduleOptions[m.name.GetString()];
            for (const auto& o : m.value.GetObject())
                if (o.value.IsString()) opts[o.name.GetString()] = o.value.GetString();
        }
    }
    d.sanitize();
    out = std::move(d);
    return true;
}

// ------------------------------------------------------------------ disk

void SettingsStore::load() {
    std::error_code ec;
    if (!fs::exists(path_, ec)) {
        data_ = SettingsData{};
        return;
    }

    std::ifstream in(path_, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string error;
    SettingsData loaded;
    if (in.bad() || !fromJson(ss.str(), loaded, error)) {
        if (error.empty()) error = "read failed";
        log_->warn("Settings file unreadable (" + error + "); starting with defaults.");
        in.close();
        std::string backup = path_ + ".corrupt-" + std::to_string(static_cast<long long>(std::time(nullptr)));
        fs::rename(path_, backup, ec);
        if (ec) log_->warn("Could not move unreadable settings aside: " + ec.message());
        data_ = SettingsData{};
        return;
    }
    data_ = std::move(loaded);
}

bool SettingsStore::save() {
    data_.sanitize();
    std::error_code ec;
    fs::path p(path_);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    const std::string tmp = path_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << toJson(data_);
        if (!out) {
            log_->error("Could not write settings to " + tmp);
            return false;
        }
    }
    fs::rename(tmp, path_, ec);  // atomic replace on the same filesystem
    if (ec) {
        log_->error("Could not save settings: " + ec.message());
        return false;
    }
    dirty_ = false;
    return true;
}

void SettingsStore::markDirty(double now) {
    dirty_ = true;
    lastChange_ = now;
}

bool SettingsStore::flushIfDirty(double now, double delaySeconds) {
    if (!dirty_ || now - lastChange_ < delaySeconds) return false;
    return save();
}

void SettingsStore::resetToDefaults(double now) {
    data_ = SettingsData{};
    markDirty(now);
}

// ------------------------------------------------------------------ accessors

bool SettingsStore::isFavorite(const std::string& id) const {
    return std::find(data_.favorites.begin(), data_.favorites.end(), id) != data_.favorites.end();
}

bool SettingsStore::toggleFavorite(const std::string& id, double now) {
    auto it = std::find(data_.favorites.begin(), data_.favorites.end(), id);
    bool nowFavorite = it == data_.favorites.end();
    if (nowFavorite) data_.favorites.push_back(id);
    else data_.favorites.erase(it);
    markDirty(now);
    return nowFavorite;
}

const std::string* SettingsStore::getModuleOption(const std::string& moduleId, const std::string& optionId) const {
    auto m = data_.moduleOptions.find(moduleId);
    if (m == data_.moduleOptions.end()) return nullptr;
    auto o = m->second.find(optionId);
    return o == m->second.end() ? nullptr : &o->second;
}

void SettingsStore::setModuleOption(const std::string& moduleId, const std::string& optionId, const std::string& value, double now) {
    data_.moduleOptions[moduleId][optionId] = value;
    markDirty(now);
}

void SettingsStore::setModuleEnabled(const std::string& id, bool enabled, double now) {
    auto& list = data_.enabledModules;
    auto it = std::find(list.begin(), list.end(), id);
    if (enabled && it == list.end()) {
        list.push_back(id);
        markDirty(now);
    } else if (!enabled && it != list.end()) {
        list.erase(it);
        markDirty(now);
    }
}

}  // namespace saucy
