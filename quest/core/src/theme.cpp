#include "saucy/theme.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace saucy {

Rgba Rgba::hex(const std::string& input) {
    std::string h = (!input.empty() && input[0] == '#') ? input.substr(1) : input;
    if (h.size() == 3) h = std::string{h[0], h[0], h[1], h[1], h[2], h[2]};
    if (h.size() != 6 && h.size() != 8) throw std::invalid_argument("Bad colour '" + input + "'");
    auto part = [&](size_t i) {
        unsigned v = 0;
        for (size_t k = i; k < i + 2; ++k) {
            char c = h[k];
            unsigned d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 99;
            if (d > 15) throw std::invalid_argument("Bad colour '" + input + "'");
            v = v * 16 + d;
        }
        return v / 255.0f;
    };
    return Rgba{part(0), part(2), part(4), h.size() == 8 ? part(6) : 1.0f};
}

std::string Rgba::toHex() const {
    auto to255 = [](float v) { return static_cast<int>(std::lround(std::fmax(0.0f, std::fmin(1.0f, v)) * 255.0f)); };
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", to255(r), to255(g), to255(b));
    return buf;
}

namespace {
Theme make(const char* id, const char* name, const char* panel, const char* header, const char* button, const char* buttonOn,
           const char* buttonBlocked, const char* text, const char* textOn, const char* textMuted, const char* accent) {
    return Theme{id,
                 name,
                 Rgba::hex(panel),
                 Rgba::hex(header),
                 Rgba::hex(button),
                 Rgba::hex(buttonOn),
                 Rgba::hex(buttonBlocked),
                 Rgba::hex(text),
                 Rgba::hex(textOn),
                 Rgba::hex(textMuted),
                 Rgba::hex(accent),
                 Rgba::hex(accent)};
}
}  // namespace

const std::vector<Theme>& allThemes() {
    static const std::vector<Theme> themes = {
        make("saucy", "Saucy", "#1E1414", "#C8382C", "#3A2624", "#E2573F", "#2A2222", "#F6E9DC", "#FFFFFF", "#B39C90", "#F2B33D"),
        make("midnight", "Midnight", "#0F1320", "#3B4CCA", "#1D2438", "#5468FF", "#171B27", "#E4E8FF", "#FFFFFF", "#8C93B8", "#7FD1FF"),
        make("mint", "Mint", "#0F1C18", "#1F9E77", "#1B302A", "#2FC495", "#16221F", "#E2F5EE", "#06140F", "#8FB3A6", "#C6F36B"),
        make("daylight", "Daylight", "#F3EFE8", "#2B2B2B", "#E0D9CD", "#2B2B2B", "#ECE7DF", "#1F1F1F", "#FFFFFF", "#7A7268", "#D9480F"),
        make("contrast", "High contrast", "#000000", "#FFFFFF", "#1A1A1A", "#FFE600", "#0D0D0D", "#FFFFFF", "#000000", "#BDBDBD", "#00E5FF"),
    };
    return themes;
}

const Theme& themeById(const std::string& id) {
    for (const auto& t : allThemes())
        if (t.id == id) return t;
    return allThemes().front();
}

const Theme& nextTheme(const std::string& currentId) {
    const auto& all = allThemes();
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].id == currentId) return all[(i + 1) % all.size()];
    return all.front();
}

}  // namespace saucy
