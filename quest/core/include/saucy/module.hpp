#pragma once
// Unity-free module contract for the Quest build. Mirrors src/SaucyMenu.Core (the PC C# version).

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace saucy {

/// Where a module may run. There is deliberately no networked tier.
enum class ModuleSafety {
    LocalInfo,       ///< reads game state, shows it only to you; allowed everywhere
    LocalVisual,     ///< changes only what you see; allowed everywhere
    PrivateRoomOnly  ///< moves your own player; offline / private room / modded queue only
};

std::string describe(ModuleSafety safety);
std::string badge(ModuleSafety safety);

/// A setting cycled with one press in VR. Stored as the chosen string.
struct ModuleOption {
    std::string id;
    std::string name;
    std::vector<std::string> choices;
    int defaultIndex = 0;

    ModuleOption(std::string id, std::string name, std::vector<std::string> choices, int defaultIndex = 0);

    static ModuleOption toggle(std::string id, std::string name, bool defaultOn);

    const std::string& defaultValue() const { return choices[defaultIndex]; }
    /// The stored value if it is still a valid choice, otherwise the default.
    std::string normalize(const std::string* stored) const;
    std::string next(const std::string* current) const;
    /// Leading number of a choice such as "75" or "10 m/s".
    static float parseLeadingFloat(const std::string& value, float fallback);
};

class ModuleRegistry;

/// Base class for every feature. A module owns what it creates and must undo all of it in onDisable().
/// The registry calls every hook inside try/catch; a module that throws is switched off on its own.
class Module {
public:
    virtual ~Module() = default;

    virtual std::string id() const = 0;           ///< permanent; keys the settings file
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::string category() const = 0;
    virtual ModuleSafety safety() const = 0;
    virtual std::vector<std::string> tags() const { return {}; }
    virtual const std::vector<ModuleOption>& options() const;

    bool enabled() const { return enabled_; }
    const std::string& lastError() const { return lastError_; }

protected:
    std::string getOption(const std::string& optionId) const;
    bool getToggle(const std::string& optionId) const { return getOption(optionId) == "On"; }
    float getFloat(const std::string& optionId, float fallback) const {
        return ModuleOption::parseLeadingFloat(getOption(optionId), fallback);
    }

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onUpdate() {}         ///< every frame
    virtual void onPreLocomotion() {}  ///< every frame, just before the game's locomotion step
    virtual void onLateUpdate() {}     ///< every frame, after locomotion (cameras, visuals)
    virtual void onOptionChanged(const std::string& /*optionId*/) {}

private:
    friend class ModuleRegistry;
    bool enabled_ = false;
    std::string lastError_;
    std::function<const std::string*(const std::string&, const std::string&)> optionReader_;
};

}  // namespace saucy
