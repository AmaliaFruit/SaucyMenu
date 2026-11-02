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

void PanelRenderer::setOpen(bool open, double now) {
    if (open && !root_ && now >= retryAt_) this->open();
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
    for (unity::Ref* r : {&root_, &content_, &pointer_, &toastText_, &toastBox_}) r->reset();
    boxes_.clear();
    texts_.clear();
    toastVisible_ = false;
    shownToast_.clear();
    builtRevision_ = -1;
}

void PanelRenderer::lateUpdate(double now) {
    if (!root_) return;
    if (!game::tagger() || !unity::alive(root_)) {  // e.g. the scene unloaded under us
        destroy();
        return;
    }

    bool menuOnLeft = settings_.data().menuHand == MenuHand::Left;
    if (!unity::alive(pointer_) || pointerOnLeft_ == menuOnLeft) createPointer(!menuOnLeft);

    if (appearanceDirty_ || builtRevision_ != controller_.revision()) {
        try {
            rebuild(now);
        } catch (...) {
            // Don't rebuild a failing page every frame: close, and allow reopening after a pause.
            destroy();
            retryAt_ = now + 2.0;
            throw;
        }
    }
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
    Layout next = buildLayout(controller_);

    // Same boxes in the same places (e.g. a toggle or a typed letter): update colours and text in place.
    // Recreating ~140 objects through reflection on every press would hitch the headset.
    bool reuse = !appearanceDirty_ && unity::alive(content_) && sameStructure(layout_, next) && boxes_.size() == next.boxes.size() &&
                 texts_.size() == next.texts.size();
    if (reuse) {
        for (size_t i = 0; i < next.boxes.size(); ++i)
            if (!sameColor(layout_.boxes[i].color, next.boxes[i].color)) unity::setRendererMaterial(boxes_[i], unity::flatMaterial(next.boxes[i].color));
        for (size_t i = 0; i < next.texts.size(); ++i) {
            if (static_cast<int>(i) == next.toastText) continue;
            if (layout_.texts[i].text != next.texts[i].text) unity::setText(texts_[i], next.texts[i].text);
            if (!sameColor(layout_.texts[i].color, next.texts[i].color)) unity::setTextColor(texts_[i], next.texts[i].color);
        }
        layout_ = std::move(next);
        builtRevision_ = controller_.revision();
        return;
    }

    // Different page: throw the old content away and draw the new one.
    Obj rootT = unity::transformOf(root_);
    if (unity::alive(content_)) unity::destroy(unity::gameObjectOf(content_));
    boxes_.clear();
    texts_.clear();
    toastText_.reset();
    toastBox_.reset();
    toastVisible_ = false;
    shownToast_.clear();

    Obj contentGo = unity::newGameObject("Content");
    content_ = unity::transformOf(contentGo);
    unity::setParent(content_, rootT);
    unity::setLocalPosition(content_, {0, 0, 0});
    unity::setLocalRotation(content_, Quat{});
    unity::setLocalScale(content_, {1, 1, 1});

    layout_ = std::move(next);
    for (size_t i = 0; i < layout_.boxes.size(); ++i) {
        const DrawBox& b = layout_.boxes[i];
        Obj go = box(content_, b.button >= 0 ? "Button" : "Box", b.center, b.size, b.color);
        boxes_.push_back(go);
        if (static_cast<int>(i) == layout_.toastBox) {
            toastBox_ = go;
            unity::setActive(go, false);
        }
    }
    for (size_t i = 0; i < layout_.texts.size(); ++i) {
        const DrawText& t = layout_.texts[i];
        Obj tmp = unity::createText(content_, "Text", t.center, t.width, t.height, t.text, t.color, toTmp(t.align), t.maxFontSize, t.wrap);
        texts_.push_back(tmp);
        if (static_cast<int>(i) == layout_.toastText) {
            toastText_ = tmp;
            unity::setActive(unity::gameObjectOf(tmp), false);
        }
    }

    if (unity::alive(pointer_)) unity::setRendererMaterial(pointer_, unity::flatMaterial(controller_.theme().pointer));
    tracker_.reset();
    tracker_.blockFor(now, 0.3);  // a finger resting where the old button was must not press the new one

    // Only marked built once everything exists; if anything above threw, the next frame starts over.
    appearanceDirty_ = false;
    builtRevision_ = controller_.revision();
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
    // Flipped = turned 180 degrees about the hand's forward axis: on the other side of the hand, facing the other way.
    Vec3 side = s.flipMenu ? back : outward;
    Quat facing = Quat::lookRotation(side * -1.0f, Vec3::forward());

    Obj t = unity::transformOf(root_);
    unity::setPose(t, handPos + handRot.rotate(side * (kHandOffset * scale_)), handRot * facing);
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
