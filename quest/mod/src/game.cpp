#include "game.hpp"

namespace game {

namespace {
Obj network() {
    auto r = i2c::get_field<i2c::result<Obj>>({"", "NetworkSystem"}, "Instance");
    return r && *r && unity::alive(*r) ? *r : nullptr;
}

std::string str(StringW s) { return s ? static_cast<std::string>(s) : std::string(); }
}  // namespace

Obj tagger() {
    auto r = i2c::run_method<i2c::result<Obj>>({"", "GorillaTagger"}, "get_Instance");
    return r && *r && unity::alive(*r) ? *r : nullptr;
}

static Obj taggerField(char const* name) {
    Obj t = tagger();
    if (!t) return nullptr;
    Obj o = i2c::get_field<Obj>(t, name);
    return unity::alive(o) ? o : nullptr;
}

Obj leftHand() { return taggerField("leftHandTransform"); }
Obj rightHand() { return taggerField("rightHandTransform"); }
Obj headCamera() { return taggerField("mainCamera"); }
Obj headCollider() { return taggerField("headCollider"); }

Obj playerRigidbody() {
    Obj t = tagger();
    if (!t) return nullptr;
    Obj rb = i2c::run_method<Obj>(t, "get_rigidbody");
    return unity::alive(rb) ? rb : nullptr;
}

Obj player() {
    auto r = i2c::run_method<i2c::result<Obj>>({"GorillaLocomotion", "GTPlayer"}, "get_Instance");
    return r && *r && unity::alive(*r) ? *r : nullptr;
}

float playerScale() {
    Obj p = player();
    if (!p) return 1.0f;
    auto s = i2c::run_method<i2c::result<float>>(p, "get_scale");
    return s && *s > 0.01f ? *s : 1.0f;
}

void vibrate(bool left, float strengthFraction, float seconds) {
    Obj t = tagger();
    if (!t) return;
    float strength = i2c::get_field<float>(t, "tapHapticStrength") * strengthFraction;
    i2c::run_method(t, "StartVibration", left, strength, seconds);
}

void playLocalClick(bool left) {
    Obj rig = i2c::run_method<Obj>({"", "VRRig"}, "get_LocalRig");
    if (!unity::alive(rig)) return;
    i2c::run_method(rig, "PlayHandTapLocal", 50, left, 0.3f);
}

void Input::poll() {
    auto r = i2c::get_field<i2c::result<Obj>>({"", "ControllerInputPoller"}, "instance");
    Obj p = r && *r && unity::alive(*r) ? *r : nullptr;
    available = p != nullptr;
    if (!p) {
        *this = Input{};
        return;
    }
    leftPrimary = i2c::get_field<bool>(p, "leftControllerPrimaryButton");
    leftSecondary = i2c::get_field<bool>(p, "leftControllerSecondaryButton");
    rightPrimary = i2c::get_field<bool>(p, "rightControllerPrimaryButton");
    rightSecondary = i2c::get_field<bool>(p, "rightControllerSecondaryButton");
    leftGrip = i2c::get_field<bool>(p, "leftGrab");
    rightGrip = i2c::get_field<bool>(p, "rightGrab");
}

bool RoomState::inRoom() const {
    Obj n = network();
    return n && i2c::run_method<bool>(n, "get_InRoom");
}

bool RoomState::isPrivate() const {
    Obj n = network();
    return n && i2c::run_method<bool>(n, "get_SessionIsPrivate");
}

std::string RoomState::gameMode() const {
    Obj n = network();
    return n ? str(i2c::run_method<StringW>(n, "get_GameModeString")) : std::string();
}

std::string RoomState::roomName() const {
    Obj n = network();
    return n ? str(i2c::run_method<StringW>(n, "get_RoomName")) : std::string();
}

int RoomState::playerCount() const {
    Obj n = network();
    return n ? i2c::run_method<int>(n, "get_RoomPlayerCount") : 0;
}

std::string RoomState::region() const {
    Obj n = network();
    return n ? str(i2c::run_method<StringW>(n, "get_CurrentRegion")) : std::string();
}

int pingMs() {
    auto connected = i2c::run_method<i2c::result<bool>>({"Photon.Pun", "PhotonNetwork"}, "get_IsConnected");
    if (!connected || !*connected) return -1;
    return i2c::run_method<int>({"Photon.Pun", "PhotonNetwork"}, "GetPing");
}

}  // namespace game
