#pragma once

#include <string>
#include <vector>

namespace saucy {

struct Rgba {
    float r = 0, g = 0, b = 0, a = 1;
    /// #RGB, #RRGGBB or #RRGGBBAA; throws std::invalid_argument on bad input.
    static Rgba hex(const std::string& hex);
    std::string toHex() const;
};

struct Theme {
    std::string id;
    std::string name;
    Rgba panel, header, button, buttonOn, buttonBlocked, text, textOn, textMuted, accent, pointer;
};

const std::vector<Theme>& allThemes();
const Theme& themeById(const std::string& id);
const Theme& nextTheme(const std::string& currentId);

}  // namespace saucy
