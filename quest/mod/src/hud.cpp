#include "hud.hpp"

#include <algorithm>
#include <exception>

#include "game.hpp"

using unity::Obj;

namespace {
constexpr double kRefreshSeconds = 0.2;
constexpr float kLineHeight = 0.026f;
constexpr float kWidth = 0.34f;
const saucy::Vec3 kOffset{0.0f, -0.17f, 0.6f};
}  // namespace

void Hud::setLine(const std::string& key, std::function<std::string()> provider) {
    lines_.erase(std::remove_if(lines_.begin(), lines_.end(), [&](auto& l) { return l.first == key; }), lines_.end());
    lines_.emplace_back(key, std::move(provider));
    nextRefresh_ = 0;
}

void Hud::removeLine(const std::string& key) {
    lines_.erase(std::remove_if(lines_.begin(), lines_.end(), [&](auto& l) { return l.first == key; }), lines_.end());
    nextRefresh_ = 0;
    if (lines_.empty()) destroy();
}

void Hud::update(double now) {
    if (lines_.empty()) return;
    const saucy::Theme& theme = theme_();
    if ((!unity::alive(root_) || themeId_ != theme.id) && now >= retryAt_) {
        try {
            build(theme);
        } catch (...) {
            retryAt_ = now + 5.0;  // e.g. no font found: retry occasionally instead of every frame
            throw;
        }
    }
    if (!root_ || !text_ || !background_ || now < nextRefresh_) return;
    nextRefresh_ = now + kRefreshSeconds;

    std::string text;
    for (auto& [key, provider] : lines_) {
        std::string value;
        try {
            value = provider();
        } catch (const std::exception& e) {
            value = key + ": error";
        }
        if (!text.empty()) text += '\n';
        text += value;
    }
    unity::setText(text_, text);
    float h = kLineHeight * static_cast<float>(lines_.size()) + 0.012f;
    unity::setLocalScale(unity::transformOf(background_), {kWidth, h, 0.002f});
    i2c::run_method(unity::req(i2c::run_method<Obj>(text_.get(), "get_rectTransform"), "HUD rect"), "set_sizeDelta",
                    unity::Vector2{kWidth - 0.02f, h});
}

void Hud::build(const saucy::Theme& theme) {
    destroy();
    Obj cam = game::headCamera();
    if (!cam) return;
    try {
        buildParts(theme, cam);
    } catch (...) {
        destroy();  // never leave a half-built HUD behind: update() would then call into null objects
        throw;
    }
    themeId_ = theme.id;  // last, so a failed build is retried rather than treated as current
    nextRefresh_ = 0;
}

void Hud::buildParts(const saucy::Theme& theme, Obj cam) {
    root_ = unity::newGameObject("SaucyMenu_HUD");
    Obj t = unity::transformOf(root_);
    unity::setParent(t, unity::transformOf(cam));
    unity::setLocalPosition(t, kOffset);
    unity::setLocalRotation(t, saucy::Quat{});
    unity::setLocalScale(t, {1, 1, 1});

    background_ = unity::createPrimitive(unity::PrimitiveType::Cube, "HudBackground");
    unity::destroyImmediate(unity::getComponent(background_, "UnityEngine", "Collider"));
    Obj bt = unity::transformOf(background_);
    unity::setParent(bt, t);
    unity::setLocalPosition(bt, {0, 0, 0.003f});
    unity::setLocalRotation(bt, saucy::Quat{});
    unity::setLocalScale(bt, {kWidth, kLineHeight, 0.002f});
    saucy::Rgba panel = theme.panel;
    panel.a = 1.0f;
    unity::setRendererMaterial(background_, unity::flatMaterial(panel));

    text_ = unity::createText(t, "HudText", {0, 0, 0}, kWidth - 0.02f, kLineHeight, "", theme.text, unity::TextAlignmentOptions::Center,
                              0.16f, false);
    i2c::run_method(text_.get(), "set_enableAutoSizing", false);
    i2c::run_method(text_.get(), "set_fontSize", 0.16f);
}

void Hud::destroy() {
    unity::destroy(root_);
    root_.reset();
    background_.reset();
    text_.reset();
    themeId_.clear();
}
