#include "saucy/module.hpp"

#include <cctype>
#include <cstdlib>

#include "saucy/log.hpp"

namespace saucy {

NullLog& NullLog::instance() {
    static NullLog log;
    return log;
}

std::string describe(ModuleSafety safety) {
    switch (safety) {
        case ModuleSafety::LocalInfo: return "Local info - works everywhere";
        case ModuleSafety::LocalVisual: return "Local visual - works everywhere";
        case ModuleSafety::PrivateRoomOnly: return "Private / modded rooms only";
    }
    return "Unknown";
}

std::string badge(ModuleSafety safety) { return safety == ModuleSafety::PrivateRoomOnly ? "PRIVATE" : ""; }

ModuleOption::ModuleOption(std::string id_, std::string name_, std::vector<std::string> choices_, int defaultIndex_)
    : id(std::move(id_)), name(std::move(name_)), choices(std::move(choices_)), defaultIndex(defaultIndex_) {
    if (id.empty()) throw std::invalid_argument("Option id is required");
    if (choices.empty()) throw std::invalid_argument("Option '" + id + "' needs at least one choice");
    if (defaultIndex < 0 || defaultIndex >= static_cast<int>(choices.size()))
        throw std::out_of_range("Option '" + id + "' default index out of range");
    if (name.empty()) name = id;
}

ModuleOption ModuleOption::toggle(std::string id, std::string name, bool defaultOn) {
    return ModuleOption(std::move(id), std::move(name), {"Off", "On"}, defaultOn ? 1 : 0);
}

std::string ModuleOption::normalize(const std::string* stored) const {
    if (stored) {
        for (const auto& c : choices)
            if (c == *stored) return c;
    }
    return defaultValue();
}

std::string ModuleOption::next(const std::string* current) const {
    std::string value = normalize(current);
    for (size_t i = 0; i < choices.size(); ++i)
        if (choices[i] == value) return choices[(i + 1) % choices.size()];
    return defaultValue();
}

float ModuleOption::parseLeadingFloat(const std::string& value, float fallback) {
    size_t end = 0;
    while (end < value.size() &&
           (std::isdigit(static_cast<unsigned char>(value[end])) || value[end] == '.' || (end == 0 && value[end] == '-')))
        ++end;
    if (end == 0) return fallback;
    std::string number = value.substr(0, end);
    char* parsedEnd = nullptr;
    float f = std::strtof(number.c_str(), &parsedEnd);
    return parsedEnd == number.c_str() ? fallback : f;
}

const std::vector<ModuleOption>& Module::options() const {
    static const std::vector<ModuleOption> none;
    return none;
}

std::string Module::getOption(const std::string& optionId) const {
    for (const auto& option : options()) {
        if (option.id == optionId)
            return option.normalize(optionReader_ ? optionReader_(id(), optionId) : nullptr);
    }
    throw std::invalid_argument("Module '" + id() + "' has no option '" + optionId + "'");
}

}  // namespace saucy
