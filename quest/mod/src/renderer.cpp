#include "renderer.hpp"

#include "game.hpp"

using namespace saucy;
using unity::Obj;

namespace {
constexpr float kHandOffset = 0.07f;    // panel centre sits this far out from the hand, toward the viewer
constexpr float kPointerSize = 0.012f;  // fingertip dot diameter
const Vec3 kFingertip{0.0f, -0.1f, 0.0f};  // fingertip relative to the game's hand transform (same as PC menus)

unity::TextAlignmentOptions toTmp(TextAlign a) {
    switch (a) {
        case TextAlign::Center: return unity::TextAlignmentOptions::Center;
        case TextAlign::Right: return unity::TextAlignmentOptions::Right;
        default: return unity::TextAlignmentOptions::Left;
    }
}

Obj box(Obj parent, const char* name, Vec3 localPos, Vec3 size, Rgba color) {
    Obj go = unity::createPrimitive(unity::PrimitiveType::Cube, name);
    // Remove the collider immediately: menu geometry must never exist as a physics surface, even for a frame.
    unity::destroyImmediate(unity::getComponent(go, "UnityEngine", "Collider"));
    Obj t = unity::transformOf(go);
    unity::setParent(t, parent);
    unity::setLocalPosition(t, localPos);
    unity::setLocalRotation(t, Quat{});
    unity::setLocalScale(t, size);
    unity::setRendererMaterial(go, unity::flatMaterial(color));
    return go;
}
}  // namespace

PanelRenderer::PanelRenderer(MenuController& controller, SettingsStore& settings) : controller_(controller), settings_(settings) {
    controller_.onAppearanceChanged = [this] { appearanceDirty_ = true; };
}

void PanelRenderer::setOpen(bool open) {
    if (open && !root_) this->open();
    else if (!open && root_) destroy();
}

void PanelRenderer::open() {
    root_ = unity::newGameObject("SaucyMenu_Panel");
    Obj contentGo = unity::newGameObject("Content");
    content_ = unity::transformOf(contentGo);
    unity::setParent(content_, unity::transformOf(root_));
    appearanceDirty_ = true;
    tracker_.reset();
}

void PanelRenderer::destroy() {
    unity::destroy(root_);
    unity::destroy(pointer_);
    root_ = content_ = pointer_ = toastText_ = toastBox_ = nullptr;
    toastVisible_ = false;
    shownToast_.clear();
    builtRevision_ = -1;
}

void PanelRenderer::lateUpdate(double now) {
    if (!root_) return;
    if (!game::tagger()) {
        destroy();
        return;
    }

    bool menuOnLeft = settings_.data().menuHand == MenuHand::Left;
    if (!unity::alive(pointer_) || pointerOnLeft_ == menuOnLeft) createPointer(!menuOnLeft);

    if (appearanceDirty_ || builtRevision_ != controller_.revision()) rebuild(now);
    pose();
    detectPress(now);
    updateToast(now);
}

void PanelRenderer::createPointer(bool onLeft) {
    unity::destroy(pointer_);
    Obj hand = onLeft ? game::leftHand() : game::rightHand();
    if (!hand) return;
    pointer_ = unity::createPrimitive(unity::PrimitiveType::Sphere, "SaucyMenu_Pointer");
    unity::destroyImmediate(unity::getComponent(pointer_, "UnityEngine", "Collider"));
    Obj t = unity::transformOf(pointer_);
    unity::setParent(t, hand);
    unity::setLocalPosition(t, kFingertip);
    unity::setLocalScale(t, {kPointerSize, kPointerSize, kPointerSize});
    unity::setRendererMaterial(pointer_, unity::flatMaterial(controller_.theme().pointer));
    pointerOnLeft_ = onLeft;
}

void PanelRenderer::rebuild(double now) {
    appearanceDirty_ = false;
    builtRevision_ = controller_.revision();

    // Throw away the old content wholesale and draw the new page.
    Obj rootT = unity::transformOf(root_);
    Obj oldContent = content_ ? unity::gameObjectOf(content_) : nullptr;
    unity::destroy(oldContent);
    Obj contentGo = unity::newGameObject("Content");
    content_ = unity::transformOf(contentGo);
    unity::setParent(content_, rootT);
    unity::setLocalPosition(content_, {0, 0, 0});
    unity::setLocalRotation(content_, Quat{});
    unity::setLocalScale(content_, {1, 1, 1});

    layout_ = buildLayout(controller_);
    toastText_ = toastBox_ = nullptr;
    toastVisible_ = false;
    shownToast_.clear();

    for (size_t i = 0; i < layout_.boxes.size(); ++i) {
        const DrawBox& b = layout_.boxes[i];
        Obj go = box(content_, b.button >= 0 ? "Button" : "Box", b.center, b.size, b.color);
        if (static_cast<int>(i) == layout_.toastBox) {
            toastBox_ = go;
            unity::setActive(go, false);
        }
    }
    for (size_t i = 0; i < layout_.texts.size(); ++i) {
        const DrawText& t = layout_.texts[i];
        Obj tmp = unity::createText(content_, "Text", t.center, t.width, t.height, t.text, t.color, toTmp(t.align), t.maxFontSize, t.wrap);
        if (static_cast<int>(i) == layout_.toastText) {
            toastText_ = tmp;
            unity::setActive(unity::gameObjectOf(tmp), false);
        }
    }

    if (unity::alive(pointer_)) unity::setRendererMaterial(pointer_, unity::flatMaterial(controller_.theme().pointer));
    tracker_.reset();
    tracker_.blockFor(now, 0.3);  // a finger resting where the old button was must not press the new one
}

void PanelRenderer::pose() {
    const SettingsData& s = settings_.data();
    bool left = s.menuHand == MenuHand::Left;
    Obj hand = left ? game::leftHand() : game::rightHand();
    if (!hand) return;

    scale_ = s.menuScale * game::playerScale();
    Vec3 handPos = unity::position(hand);
    Quat handRot = unity::rotation(hand);

    // The panel faces out of the palm side, toward the other hand: +x of the left hand, -x of the right.
    Vec3 outward = left ? Vec3::right() : Vec3{-1, 0, 0};
    Vec3 back = outward * -1.0f;
    Quat facing = s.flipMenu ? Quat::lookRotation(outward, Vec3::forward()) : Quat::lookRotation(back, Vec3::forward());

    Obj t = unity::transformOf(root_);
    unity::setPose(t, handPos + handRot.rotate(outward * (kHandOffset * scale_)), handRot * facing);
    unity::setLocalScale(t, {scale_, scale_, scale_});
}

void PanelRenderer::detectPress(double now) {
    if (!unity::alive(pointer_) || layout_.buttons.empty()) return;
    Obj rootT = unity::transformOf(root_);
    Vec3 tip = unity::position(unity::transformOf(pointer_));
    Vec3 local = unity::rotation(rootT).inverse().rotate(tip - unity::position(rootT)) / scale_;

    int hit = tracker_.update(layout_, local, now);
    if (hit >= 0) {
        auto press = layout_.buttons[hit].press;  // copy: pressing can change the menu
        if (press) press();
    }
}

void PanelRenderer::updateToast(double now) {
    if (!toastText_ || !toastBox_) return;
    bool show = !controller_.toastText().empty() && now < controller_.toastUntil();
    if (show && shownToast_ != controller_.toastText()) {
        shownToast_ = controller_.toastText();
        unity::setText(toastText_, shownToast_);
    }
    if (show != toastVisible_) {
        toastVisible_ = show;
        unity::setActive(unity::gameObjectOf(toastText_), show);
        unity::setActive(toastBox_, show);
    }
}
