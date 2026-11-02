#include "modules.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>

using saucy::Module;
using saucy::ModuleOption;
using saucy::ModuleSafety;
using unity::Obj;

namespace {

// ------------------------------------------------------------------ Performance HUD (LocalInfo)
class PerformanceHud final : public Module {
public:
    explicit PerformanceHud(Hud& hud) : hud_(hud) {}
    std::string id() const override { return "performance-hud"; }
    std::string name() const override { return "Performance HUD"; }
    std::string description() const override { return "Shows FPS, frame time and ping in the corner of your view. Only you see it."; }
    std::string category() const override { return "Info"; }
    ModuleSafety safety() const override { return ModuleSafety::LocalInfo; }
    std::vector<std::string> tags() const override { return {"fps", "ping", "lag", "frame", "stats"}; }
    const std::vector<ModuleOption>& options() const override {
        static const std::vector<ModuleOption> o = {ModuleOption::toggle("ping", "Show ping", true),
                                                    ModuleOption::toggle("frametime", "Show frame time", true)};
        return o;
    }

protected:
    void onEnable() override {
        smoothed_ = std::max(unity::unscaledDeltaTime(), 0.001f);
        hud_.setLine("perf", [this] { return line(); });
    }
    void onDisable() override { hud_.removeLine("perf"); }
    void onUpdate() override {
        float dt = std::clamp(unity::unscaledDeltaTime(), 0.0001f, 0.5f);
        smoothed_ += (dt - smoothed_) * 0.08f;
    }

private:
    std::string line() const {
        char buf[96];
        std::string s = "FPS " + std::to_string(static_cast<int>(std::lround(1.0f / smoothed_)));
        if (getToggle("frametime")) {
            std::snprintf(buf, sizeof buf, "   %.1f ms", smoothed_ * 1000.0f);
            s += buf;
        }
        if (getToggle("ping")) {
            int ping = game::pingMs();
            s += ping >= 0 ? "   Ping " + std::to_string(ping) + " ms" : "   Offline";
        }
        return s;
    }

    Hud& hud_;
    float smoothed_ = 1.0f / 72.0f;
};

// ------------------------------------------------------------------ Room Info (LocalInfo)
class RoomInfo final : public Module {
public:
    RoomInfo(Hud& hud, const game::RoomState& room) : hud_(hud), room_(room) {}
    std::string id() const override { return "room-info"; }
    std::string name() const override { return "Room Info"; }
    std::string description() const override {
        return "Shows the room code, player count, region and mode on your HUD. Code is hidden by default for streaming.";
    }
    std::string category() const override { return "Info"; }
    ModuleSafety safety() const override { return ModuleSafety::LocalInfo; }
    std::vector<std::string> tags() const override { return {"code", "lobby", "players", "region", "streamer"}; }
    const std::vector<ModuleOption>& options() const override {
        static const std::vector<ModuleOption> o = {ModuleOption("code", "Room code", {"Hidden", "Shown"}, 0)};
        return o;
    }

protected:
    void onEnable() override { hud_.setLine("room", [this] { return line(); }); }
    void onDisable() override { hud_.removeLine("room"); }

private:
    std::string line() const {
        if (!room_.inRoom()) return "Not in a room";
        std::string code = getOption("code") == "Shown" ? room_.roomName() : "****";
        std::string mode = room_.gameMode();
        if (mode.size() > 22) mode = mode.substr(0, 21) + "~";
        std::string region = room_.region();
        std::transform(region.begin(), region.end(), region.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return code + (room_.isPrivate() ? " (private)" : " (public)") + "   " + std::to_string(room_.playerCount()) + "/10   " + region +
               "   " + mode;
    }

    Hud& hud_;
    const game::RoomState& room_;
};

// ------------------------------------------------------------------ Session Clock (LocalInfo)
// Replaces the PC build's Smooth Camera: the standalone Quest has no desktop view to smooth.
class SessionClock final : public Module {
public:
    explicit SessionClock(Hud& hud) : hud_(hud) {}
    std::string id() const override { return "session-clock"; }
    std::string name() const override { return "Session Clock"; }
    std::string description() const override { return "Shows the time and how long you've been playing on your HUD."; }
    std::string category() const override { return "Info"; }
    ModuleSafety safety() const override { return ModuleSafety::LocalInfo; }
    std::vector<std::string> tags() const override { return {"time", "clock", "timer", "break"}; }
    const std::vector<ModuleOption>& options() const override {
        static const std::vector<ModuleOption> o = {ModuleOption("format", "Clock", {"24 hour", "12 hour"}, 0)};
        return o;
    }

protected:
    void onEnable() override {
        start_ = std::chrono::steady_clock::now();
        hud_.setLine("clock", [this] { return line(); });
    }
    void onDisable() override { hud_.removeLine("clock"); }

private:
    std::string line() const {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        char clock[16];
        std::strftime(clock, sizeof clock, getOption("format") == "12 hour" ? "%I:%M %p" : "%H:%M", &local);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - start_).count();
        char played[32];
        std::snprintf(played, sizeof played, "%lldh %02lldm", static_cast<long long>(minutes / 60), static_cast<long long>(minutes % 60));
        return std::string(clock) + "   played " + played;
    }

    Hud& hud_;
    std::chrono::steady_clock::time_point start_;
};

// ------------------------------------------------------------------ Platforms (PrivateRoomOnly)
class Platforms final : public Module {
public:
    explicit Platforms(const game::Input& input) : input_(input) {}
    std::string id() const override { return "platforms"; }
    std::string name() const override { return "Platforms"; }
    std::string description() const override {
        return "Hold grip to stand on a platform under that hand. Release to remove it. Only exists in your game.";
    }
    std::string category() const override { return "Movement"; }
    ModuleSafety safety() const override { return ModuleSafety::PrivateRoomOnly; }
    std::vector<std::string> tags() const override { return {"grip", "air", "climb", "plats"}; }
    const std::vector<ModuleOption>& options() const override {
        static const std::vector<ModuleOption> o = {ModuleOption::toggle("visible", "Show platforms", true)};
        return o;
    }

protected:
    void onUpdate() override {
        handle(left_, input_.leftGrip, true);
        handle(right_, input_.rightGrip, false);
    }
    void onDisable() override {
        remove(left_);
        remove(right_);
    }
    void onOptionChanged(const std::string&) override {
        bool visible = getToggle("visible");
        if (unity::alive(left_)) unity::setRendererEnabled(left_, visible);
        if (unity::alive(right_)) unity::setRendererEnabled(right_, visible);
    }

private:
    void handle(Obj& slab, bool grip, bool isLeft) {
        if (grip && !unity::alive(slab)) slab = create(isLeft);
        else if (!grip && slab) remove(slab);
    }

    Obj create(bool isLeft) {
        game::HandPose hand = game::trueHand(isLeft);
        float scale = game::playerScale();
        saucy::Vec3 size{0.025f * scale, 0.28f * scale, 0.38f * scale};

        // Keeps its BoxCollider: this is the surface you stand on. It exists only in your game.
        Obj slab = unity::createPrimitive(unity::PrimitiveType::Cube, isLeft ? "SaucyMenu_PlatformL" : "SaucyMenu_PlatformR");
        Obj t = unity::transformOf(slab);
        unity::setLocalScale(t, size);
        // Beside the palm (not around the hand) so the hand doesn't start inside the collider.
        saucy::Vec3 side = hand.rotation.rotate(saucy::Vec3::right()) * (isLeft ? 1.0f : -1.0f);
        unity::setPose(t, hand.position + side * (size.x / 2.0f + 0.025f * scale), hand.rotation);
        unity::setRendererMaterial(slab, unity::flatMaterial({0.95f, 0.7f, 0.24f, 1.0f}));
        unity::setRendererEnabled(slab, getToggle("visible"));
        // Push the new pose to physics now, so this frame's locomotion sees the slab at the hand, not a cube at the origin.
        unity::syncPhysicsTransforms();
        return slab;
    }

    static void remove(Obj& slab) {
        // Deactivate first: Destroy is deferred to the end of the frame and the slab must stop being solid now.
        if (unity::alive(slab)) unity::setActive(slab, false);
        unity::destroy(slab);
        slab = nullptr;
    }

    const game::Input& input_;
    Obj left_ = nullptr;
    Obj right_ = nullptr;
};

// ------------------------------------------------------------------ Fly (PrivateRoomOnly)
class Fly final : public Module {
public:
    explicit Fly(const game::Input& input) : input_(input) {}
    std::string id() const override { return "fly"; }
    std::string name() const override { return "Fly"; }
    std::string description() const override { return "Hold A (right primary) to fly where you look. Release to fall normally."; }
    std::string category() const override { return "Movement"; }
    ModuleSafety safety() const override { return ModuleSafety::PrivateRoomOnly; }
    std::vector<std::string> tags() const override { return {"flight", "noclip", "move"}; }
    const std::vector<ModuleOption>& options() const override {
        static const std::vector<ModuleOption> o = {ModuleOption("speed", "Speed", {"5 m/s", "10 m/s", "20 m/s"}, 1)};
        return o;
    }

protected:
    void onPreLocomotion() override {
        if (!input_.rightPrimary) return;
        Obj player = game::player();
        Obj head = game::headCollider();
        Obj rb = game::playerRigidbody();
        if (!player || !head || !rb) return;
        if (i2c::run_method<bool>(rb, "get_isKinematic")) return;  // the game itself skips movement in this state

        float scale = game::playerScale();
        float speed = getFloat("speed", 10.0f) * scale;
        Obj pt = unity::transformOf(player);
        saucy::Vec3 dir = unity::forward(unity::transformOf(head));
        unity::setPose(pt, unity::position(pt) + dir * (unity::deltaTime() * speed), unity::rotation(pt));
        i2c::run_method(rb, "set_linearVelocity", unity::Vector3{0, 0, 0});
    }

private:
    const game::Input& input_;
};

}  // namespace

std::vector<std::unique_ptr<Module>> createModules(Hud& hud, const game::Input& input, const game::RoomState& room) {
    std::vector<std::unique_ptr<Module>> m;
    m.push_back(std::make_unique<PerformanceHud>(hud));
    m.push_back(std::make_unique<RoomInfo>(hud, room));
    m.push_back(std::make_unique<SessionClock>(hud));
    m.push_back(std::make_unique<Platforms>(input));
    m.push_back(std::make_unique<Fly>(input));
    return m;
}
