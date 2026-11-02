#pragma once
// Draws the menu on the Quest: a flat panel held in the menu hand plus a fingertip dot on the other hand.
// Layout and press detection come from saucy::buildLayout / PressTracker (unit tested in quest/core);
// this file only creates the GameObjects and moves them. No colliders are used for the menu at all —
// presses are detected by measuring the fingertip against the layout, so the player can never bump into it.

#include <vector>

#include "saucy/layout.hpp"
#include "saucy/menu.hpp"
#include "unity.hpp"

class PanelRenderer {
public:
    PanelRenderer(saucy::MenuController& controller, saucy::SettingsStore& settings);

    void setOpen(bool open);
    bool isOpen() const { return root_ != nullptr; }
    bool pointerOnLeft() const { return pointerOnLeft_; }

    /// Rebuilds if the menu changed, poses the panel, detects fingertip presses, shows toasts.
    void lateUpdate(double now);
    void destroy();

private:
    void open();
    void rebuild(double now);
    void pose();
    void createPointer(bool onLeft);
    void detectPress(double now);
    void updateToast(double now);

    saucy::MenuController& controller_;
    saucy::SettingsStore& settings_;
    saucy::Layout layout_;
    saucy::PressTracker tracker_;
    unity::Obj root_ = nullptr;
    unity::Obj content_ = nullptr;
    unity::Obj pointer_ = nullptr;
    unity::Obj toastText_ = nullptr;
    unity::Obj toastBox_ = nullptr;
    std::string shownToast_;
    bool toastVisible_ = false;
    bool pointerOnLeft_ = false;
    bool appearanceDirty_ = true;
    int builtRevision_ = -1;
    float scale_ = 1.0f;
};
