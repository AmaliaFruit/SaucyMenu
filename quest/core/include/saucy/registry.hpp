#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "saucy/log.hpp"
#include "saucy/module.hpp"
#include "saucy/room_gate.hpp"
#include "saucy/settings.hpp"

namespace saucy {

enum class TickPhase { Update, PreLocomotion, LateUpdate };

/// Owns every module: registration, enable/disable, per-frame ticks, options and the room gate.
/// All module code runs inside try/catch; a module that throws is switched off and reported.
class ModuleRegistry {
public:
    using Clock = std::function<double()>;

    ModuleRegistry(SettingsStore& settings, const IRoomState* room, ILog* log, Clock clock);
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    std::function<void(const std::string&)> onNotice;  ///< short user-facing messages (toasts)
    std::function<void()> onChanged;                   ///< any enabled state changed

    /// Throws std::invalid_argument / std::logic_error on bad ids, names, duplicates.
    Module& add(std::unique_ptr<Module> module);

    Module* get(const std::string& id) const;
    const std::vector<std::unique_ptr<Module>>& modules() const { return modules_; }
    const std::vector<std::string>& categories() const { return categories_; }
    std::vector<Module*> inCategory(const std::string& category) const;
    int enabledCount() const;
    GateResult gate() const { return evaluateGate(room_); }
    bool canRun(const Module& module) const { return saucy::canRun(module.safety(), room_); }

    bool setEnabled(const std::string& id, bool enable, std::string* message = nullptr, bool persist = true);
    bool toggle(const std::string& id, std::string* message = nullptr);
    void enforceGate();
    void tick(TickPhase phase);

    std::string optionValue(const Module& module, const ModuleOption& option) const;
    std::string cycleOption(const std::string& moduleId, const std::string& optionId);

    void restoreFromSettings();
    void disableAllForShutdown();

private:
    template <typename F>
    bool invoke(Module& module, F&& action, const char* what);
    void notice(const std::string& text);
    void changed();

    SettingsStore& settings_;
    const IRoomState* room_;
    ILog* log_;
    Clock clock_;
    std::vector<std::unique_ptr<Module>> modules_;
    std::vector<std::string> categories_;
};

}  // namespace saucy
