#include "saucy/registry.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>

namespace saucy {

ModuleRegistry::ModuleRegistry(SettingsStore& settings, const IRoomState* room, ILog* log, Clock clock)
    : settings_(settings), room_(room), log_(log ? log : &NullLog::instance()), clock_(clock ? std::move(clock) : [] { return 0.0; }) {}

Module& ModuleRegistry::add(std::unique_ptr<Module> module) {
    if (!module) throw std::invalid_argument("module is null");
    const std::string id = module->id();
    if (id.empty()) throw std::invalid_argument("Module id is required");
    if (module->name().empty()) throw std::invalid_argument("Module '" + id + "' needs a name");
    if (module->category().empty()) throw std::invalid_argument("Module '" + id + "' needs a category");
    if (get(id)) throw std::logic_error("Duplicate module id '" + id + "'");

    std::set<std::string> optionIds;
    for (const auto& option : module->options())
        if (!optionIds.insert(option.id).second)
            throw std::logic_error("Module '" + id + "' has duplicate option '" + option.id + "'");

    module->optionReader_ = [this](const std::string& m, const std::string& o) { return settings_.getModuleOption(m, o); };
    const std::string category = module->category();
    if (std::find(categories_.begin(), categories_.end(), category) == categories_.end()) categories_.push_back(category);
    modules_.push_back(std::move(module));
    return *modules_.back();
}

Module* ModuleRegistry::get(const std::string& id) const {
    for (const auto& m : modules_)
        if (m->id() == id) return m.get();
    return nullptr;
}

std::vector<Module*> ModuleRegistry::inCategory(const std::string& category) const {
    std::vector<Module*> out;
    for (const auto& m : modules_)
        if (m->category() == category) out.push_back(m.get());
    return out;
}

int ModuleRegistry::enabledCount() const {
    return static_cast<int>(std::count_if(modules_.begin(), modules_.end(), [](const auto& m) { return m->enabled(); }));
}

bool ModuleRegistry::setEnabled(const std::string& id, bool enable, std::string* message, bool persist) {
    Module* module = get(id);
    if (!module) {
        if (message) *message = "Unknown module '" + id + "'";
        return false;
    }
    if (module->enabled_ == enable) {
        if (message) message->clear();
        return true;
    }

    if (enable) {
        GateResult g = evaluateGate(room_);
        if (module->safety() == ModuleSafety::PrivateRoomOnly && !g.allowed) {
            if (message) *message = module->name() + ": private rooms only (" + g.reason + ")";
            return false;
        }
        module->lastError_.clear();
        module->enabled_ = true;
        if (!invoke(*module, [](Module& m) { m.onEnable(); }, "enable")) {
            if (message) *message = module->name() + " failed to start";
            return false;
        }
    } else {
        module->enabled_ = false;
        invoke(*module, [](Module& m) { m.onDisable(); }, "disable");
    }

    if (persist) settings_.setModuleEnabled(id, enable, clock_());
    if (message) message->clear();
    changed();
    return true;
}

bool ModuleRegistry::toggle(const std::string& id, std::string* message) {
    Module* module = get(id);
    if (!module) {
        if (message) *message = "Unknown module '" + id + "'";
        return false;
    }
    return setEnabled(id, !module->enabled_, message);
}

void ModuleRegistry::enforceGate() {
    GateResult g = evaluateGate(room_);
    if (g.allowed) return;
    for (auto& m : modules_) {
        if (m->enabled_ && m->safety() == ModuleSafety::PrivateRoomOnly) {
            m->enabled_ = false;
            invoke(*m, [](Module& mod) { mod.onDisable(); }, "disable");
            settings_.setModuleEnabled(m->id(), false, clock_());
            notice(m->name() + " turned off: " + g.reason);
            changed();
        }
    }
}

void ModuleRegistry::tick(TickPhase phase) {
    for (size_t i = 0; i < modules_.size(); ++i) {
        Module& m = *modules_[i];
        if (!m.enabled_) continue;
        switch (phase) {
            case TickPhase::Update: invoke(m, [](Module& x) { x.onUpdate(); }, "update"); break;
            case TickPhase::PreLocomotion: invoke(m, [](Module& x) { x.onPreLocomotion(); }, "pre-locomotion"); break;
            case TickPhase::LateUpdate: invoke(m, [](Module& x) { x.onLateUpdate(); }, "late update"); break;
        }
    }
}

std::string ModuleRegistry::optionValue(const Module& module, const ModuleOption& option) const {
    return option.normalize(settings_.getModuleOption(module.id(), option.id));
}

std::string ModuleRegistry::cycleOption(const std::string& moduleId, const std::string& optionId) {
    Module* module = get(moduleId);
    if (!module) throw std::invalid_argument("Unknown module '" + moduleId + "'");
    const ModuleOption* option = nullptr;
    for (const auto& o : module->options())
        if (o.id == optionId) option = &o;
    if (!option) throw std::invalid_argument("Module '" + moduleId + "' has no option '" + optionId + "'");

    std::string next = option->next(settings_.getModuleOption(moduleId, optionId));
    settings_.setModuleOption(moduleId, optionId, next, clock_());
    if (module->enabled_) invoke(*module, [&](Module& m) { m.onOptionChanged(optionId); }, "option change");
    changed();
    return next;
}

void ModuleRegistry::restoreFromSettings() {
    auto& data = settings_.data();
    if (!data.rememberEnabledModules) {
        if (!data.enabledModules.empty()) {
            data.enabledModules.clear();
            settings_.markDirty(clock_());
        }
        return;
    }
    const std::vector<std::string> saved = data.enabledModules;
    for (const auto& id : saved) {
        if (!get(id)) continue;  // removed module: keep the id so downgrades still work
        std::string message;
        // A blocked restore (e.g. room state not known yet at startup) leaves the saved choice alone, so it
        // isn't lost; the module simply stays off until the player turns it on again.
        if (!setEnabled(id, true, &message, false) && !message.empty()) log_->info("Not restoring " + id + ": " + message);
    }
}

void ModuleRegistry::disableAllForShutdown() {
    for (auto& m : modules_) {
        if (!m->enabled_) continue;
        m->enabled_ = false;
        invoke(*m, [](Module& x) { x.onDisable(); }, "disable");
    }
}

template <typename F>
bool ModuleRegistry::invoke(Module& module, F&& action, const char* what) {
    std::string error;
    try {
        action(module);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
    } catch (...) {
        error = "unknown exception";
    }

    log_->error("Module '" + module.id() + "' threw during " + what + ": " + error);
    module.lastError_ = error;
    if (module.enabled_) {
        module.enabled_ = false;
        try {
            module.onDisable();
        } catch (...) {
            log_->error("Module '" + module.id() + "' also threw while cleaning up");
        }
        settings_.setModuleEnabled(module.id(), false, clock_());
        changed();
    }
    notice(module.name() + " hit an error and was turned off");
    return false;
}

void ModuleRegistry::notice(const std::string& text) {
    if (!onNotice) return;
    try {
        onNotice(text);
    } catch (...) {
        log_->error("Notice handler threw");
    }
}

void ModuleRegistry::changed() {
    if (!onChanged) return;
    try {
        onChanged();
    } catch (...) {
        log_->error("Change handler threw");
    }
}

}  // namespace saucy
