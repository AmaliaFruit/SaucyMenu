#include "saucy/layout.hpp"

#include <cmath>

namespace saucy {

using namespace layout_metrics;

namespace {

/// Walks down from the top edge (y = 0) in negative y; everything is re-centred at the end.
struct Cursor {
    float y = 0;
    float center(float h) const { return -(y + h / 2.0f); }
    void advance(float a) { y += a; }
};

struct Builder {
    Layout out;

    int button(Vec3 c, float w, float h, Rgba color, std::function<void()> press) {
        int id = -1;
        if (press) {
            id = static_cast<int>(out.buttons.size());
            out.buttons.push_back({c, w, h, std::move(press)});
        }
        out.boxes.push_back({c, {w, h, kButtonDepth}, color, id});
        return id;
    }

    void box(Vec3 c, Vec3 size, Rgba color) { out.boxes.push_back({c, size, color, -1}); }

    void text(Vec3 c, float w, float h, std::string t, Rgba color, TextAlign a, float maxFont = 0.18f, bool wrap = false) {
        DrawText d;
        d.center = {c.x, c.y, kTextZ};
        d.width = w;
        d.height = h;
        d.text = std::move(t);
        d.color = color;
        d.align = a;
        d.maxFontSize = maxFont;
        d.wrap = wrap;
        out.texts.push_back(std::move(d));
    }
};

}  // namespace

Layout buildLayout(MenuController& controller) {
    const Theme& theme = controller.theme();
    const MenuPage& page = controller.current();
    Builder b;
    Cursor y;
    const float inner = kWidth - kPad * 2.0f;

    // Header
    y.advance(kPad);
    float hy = y.center(kHeaderH);
    b.box({0, hy, 0}, {inner, kHeaderH, kButtonDepth}, theme.header);
    b.text({-0.02f, hy, 0}, inner - 0.06f, kHeaderH * 0.8f, page.title, theme.textOn, TextAlign::Left, 0.24f);
    if (page.pageCount > 1)
        b.text({inner / 2.0f - 0.025f, hy, 0}, 0.045f, kHeaderH * 0.6f,
               std::to_string(page.pageIndex + 1) + "/" + std::to_string(page.pageCount), theme.textOn, TextAlign::Right);
    y.advance(kHeaderH + kGap);

    // Room status
    b.text({0, y.center(kStatusH), 0}, inner, kStatusH, page.status, page.statusIsWarning ? theme.accent : theme.textMuted,
           TextAlign::Left, 0.12f);
    y.advance(kStatusH + kGap);

    // Search box + keyboard
    if (page.keyboard) {
        float qy = y.center(kRowH);
        b.box({0, qy, 0}, {inner, kRowH, kButtonDepth}, theme.buttonBlocked);
        bool empty = page.query.empty();
        b.text({0, qy, 0}, inner - 0.012f, kRowH * 0.75f, empty ? "search..." : page.query + "_", empty ? theme.textMuted : theme.text,
               TextAlign::Left);
        y.advance(kRowH + kGap);

        for (const auto& keyRow : *page.keyboard) {
            int n = static_cast<int>(keyRow.size());
            float keyW = (inner - kKeyGap * (n - 1)) / n;
            float cy = y.center(keyW);
            for (int k = 0; k < n; ++k) {
                std::string key = keyRow[k];
                float cx = -inner / 2.0f + keyW / 2.0f + k * (keyW + kKeyGap);
                std::string label = key == MenuController::kKeySpace ? "SPC" : key == MenuController::kKeyBackspace ? "DEL" : key;
                b.button({cx, cy, 0}, keyW, keyW, theme.button, [&controller, key] { controller.pressKey(key); });
                b.text({cx, cy, 0}, keyW * 0.8f, keyW * 0.6f, label, theme.text, TextAlign::Center);
            }
            y.advance(keyW + kKeyGap);
        }
        y.advance(kGap);
    }

    // Rows
    for (size_t i = 0; i < page.rows.size(); ++i) {
        const MenuRow& row = page.rows[i];
        int index = static_cast<int>(i);
        float h = row.kind == RowKind::Info ? kInfoH : kRowH;
        float cy = y.center(h);

        if (row.kind == RowKind::Info) {
            b.text({0, cy, 0}, inner, h, row.label, theme.textMuted, TextAlign::Left, 0.12f, true);
        } else if (row.kind == RowKind::Module) {
            Rgba fill = row.on ? theme.buttonOn : row.blocked ? theme.buttonBlocked : theme.button;
            Rgba text = row.on ? theme.textOn : row.blocked ? theme.textMuted : theme.text;
            float mainW = inner - kDetailW - kGap;
            float mainX = -inner / 2.0f + mainW / 2.0f;
            b.button({mainX, cy, 0}, mainW, h, fill, [&controller, index] { controller.pressRow(index); });
            b.text({mainX - 0.012f, cy, 0}, mainW - 0.04f, h * 0.7f, (row.favorite ? "* " : "") + row.label, text, TextAlign::Left);
            std::string state = row.on ? "ON" : row.blocked ? "LOCKED" : row.badge.empty() ? "OFF" : row.badge;
            b.text({mainX + mainW / 2.0f - 0.022f, cy, 0}, 0.04f, h * 0.5f, state, row.on ? text : theme.textMuted, TextAlign::Right, 0.1f);

            float dx = inner / 2.0f - kDetailW / 2.0f;
            b.button({dx, cy, 0}, kDetailW, h, theme.button, [&controller, index] { controller.pressRowDetail(index); });
            b.text({dx, cy, 0}, kDetailW * 0.8f, h * 0.6f, "...", theme.accent, TextAlign::Center);
        } else {
            Rgba fill = row.blocked ? theme.buttonBlocked : row.on ? theme.buttonOn : theme.button;
            Rgba text = row.blocked ? theme.textMuted : row.on ? theme.textOn : theme.text;
            b.button({0, cy, 0}, inner, h, fill, [&controller, index] { controller.pressRow(index); });

            bool centred = row.kind == RowKind::Action;
            float labelW = centred ? inner - 0.016f : inner * 0.62f;
            float labelX = centred ? 0.0f : -inner / 2.0f + 0.008f + labelW / 2.0f;
            b.text({labelX, cy, 0}, labelW, h * 0.7f, row.label, text, centred ? TextAlign::Center : TextAlign::Left);

            std::string right = row.kind == RowKind::Navigate ? (row.value.empty() ? ">" : row.value + "  >") : row.value;
            if (!right.empty()) {
                float rw = inner * 0.34f;
                b.text({inner / 2.0f - 0.008f - rw / 2.0f, cy, 0}, rw, h * 0.6f, right, row.on ? text : theme.accent, TextAlign::Right);
            }
        }
        y.advance(h + kGap);
    }

    // Footer: previous page, back, home, next page
    y.advance(kGap);
    float footW = (inner - kGap * 3.0f) / 4.0f;
    float fy = y.center(kFooterH);
    struct Foot {
        const char* label;
        bool enabled;
        std::function<void()> press;
    };
    Foot feet[4] = {
        {"<", page.pageIndex > 0, [&controller] { controller.previousPage(); }},
        {"Back", page.canGoBack, [&controller] { controller.back(); }},
        {"Home", page.canGoBack, [&controller] { controller.home(); }},
        {">", page.pageIndex + 1 < page.pageCount, [&controller] { controller.nextPage(); }},
    };
    for (int slot = 0; slot < 4; ++slot) {
        float cx = -inner / 2.0f + footW / 2.0f + slot * (footW + kGap);
        b.button({cx, fy, 0}, footW, kFooterH, feet[slot].enabled ? theme.button : theme.buttonBlocked,
                 feet[slot].enabled ? feet[slot].press : std::function<void()>{});
        b.text({cx, fy, 0}, footW * 0.8f, kFooterH * 0.6f, feet[slot].label, feet[slot].enabled ? theme.text : theme.textMuted,
               TextAlign::Center);
    }
    y.advance(kFooterH + kPad);

    // Background, then shift everything so the panel is centred on its origin.
    const float height = y.y;
    b.box({0, -height / 2.0f, kButtonDepth / 2.0f + kPanelDepth / 2.0f}, {kWidth, height, kPanelDepth}, theme.panel);

    // Toast just under the panel (hidden by the renderer unless a toast is active).
    b.out.toastBox = static_cast<int>(b.out.boxes.size());
    b.box({0, -height - 0.022f, 0}, {kWidth, 0.032f, kButtonDepth}, theme.header);
    b.out.toastText = static_cast<int>(b.out.texts.size());
    b.text({0, -height - 0.022f, 0}, kWidth - 0.016f, 0.026f, "", theme.textOn, TextAlign::Center, 0.14f);
    b.out.texts.back().isToast = true;

    const float shift = height / 2.0f;
    for (auto& box : b.out.boxes) box.center.y += shift;
    for (auto& t : b.out.texts) t.center.y += shift;
    for (auto& btn : b.out.buttons) btn.center.y += shift;
    b.out.width = kWidth;
    b.out.height = height;
    return std::move(b.out);
}

int hitTest(const Layout& layout, Vec3 p) {
    if (std::fabs(p.z) > kPressDepth) return -1;
    for (size_t i = 0; i < layout.buttons.size(); ++i) {
        const auto& b = layout.buttons[i];
        if (std::fabs(p.x - b.center.x) <= b.width / 2.0f && std::fabs(p.y - b.center.y) <= b.height / 2.0f)
            return static_cast<int>(i);
    }
    return -1;
}

int PressTracker::update(const Layout& layout, Vec3 localPoint, double now) {
    int hit = hitTest(layout, localPoint);
    int previous = current_;
    current_ = hit;
    if (hit < 0 || hit == previous || now < blockedUntil_) return -1;
    blockedUntil_ = now + 0.25;
    return hit;
}

void PressTracker::blockFor(double now, double seconds) {
    if (now + seconds > blockedUntil_) blockedUntil_ = now + seconds;
}

}  // namespace saucy
