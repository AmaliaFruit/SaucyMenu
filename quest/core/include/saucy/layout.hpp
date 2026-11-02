#pragma once
// Turns a MenuPage into flat draw instructions (boxes, text, pressable buttons) in panel-local metres,
// plus fingertip hit-testing. Unity-free so the whole panel layout and press logic is unit tested;
// the Quest renderer only instantiates what this produces.
//
// Panel-local space: x = right, y = up, readable face points down -z, origin at the panel centre.

#include <functional>
#include <string>
#include <vector>

#include "saucy/menu.hpp"
#include "saucy/theme.hpp"
#include "saucy/vecmath.hpp"

namespace saucy {

enum class TextAlign { Left, Center, Right };

struct DrawBox {
    Vec3 center;
    Vec3 size;
    Rgba color;
    int button = -1;  ///< index into Layout::buttons, or -1
};

struct DrawText {
    Vec3 center;
    float width = 0, height = 0;
    std::string text;
    Rgba color;
    TextAlign align = TextAlign::Left;
    float maxFontSize = 0.18f;
    bool wrap = false;
    bool isToast = false;
};

struct LayoutButton {
    Vec3 center;
    float width = 0, height = 0;
    std::function<void()> press;
};

struct Layout {
    float width = 0, height = 0;
    std::vector<DrawBox> boxes;
    std::vector<DrawText> texts;
    std::vector<LayoutButton> buttons;
    int toastText = -1;  ///< index into texts of the toast line
    int toastBox = -1;   ///< index into boxes of the toast background
};

namespace layout_metrics {
constexpr float kWidth = 0.24f;
constexpr float kPad = 0.008f;
constexpr float kRowH = 0.026f;
constexpr float kInfoH = 0.034f;
constexpr float kGap = 0.005f;
constexpr float kHeaderH = 0.032f;
constexpr float kStatusH = 0.016f;
constexpr float kFooterH = 0.026f;
constexpr float kKeyGap = 0.004f;
constexpr float kDetailW = 0.036f;
constexpr float kButtonDepth = 0.008f;
constexpr float kPanelDepth = 0.004f;
constexpr float kTextZ = -(kButtonDepth / 2.0f + 0.0015f);
/// A fingertip counts as touching a button within this distance in front of or behind its face.
constexpr float kPressDepth = 0.02f;
}  // namespace layout_metrics

Layout buildLayout(MenuController& controller);

/// Index of the button the fingertip (panel-local, unscaled) is inside, or -1.
int hitTest(const Layout& layout, Vec3 localPoint);

/// Edge-triggered press detection with a shared cooldown: fires once when the fingertip enters a button,
/// never while it rests there, and not right after the panel is rebuilt.
class PressTracker {
public:
    /// Returns the button index pressed this frame, or -1.
    int update(const Layout& layout, Vec3 localPoint, double now);
    void blockFor(double now, double seconds);
    void reset() { current_ = -1; }

private:
    int current_ = -1;
    double blockedUntil_ = 0;
};

}  // namespace saucy
