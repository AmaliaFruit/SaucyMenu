// Saucy Menu for the standalone Quest: scotland2 entry points and the single game hook.
//
// One hook only: GorillaLocomotion.GTPlayer.LateUpdate (the local player's own movement step).
//   before the original: poll input, enforce the room gate, Update + PreLocomotion module ticks
//   after the original:  LateUpdate module ticks, menu panel, HUD, settings save
// It reads nothing from and changes nothing in the game method itself. No anti-cheat, reporting, network,
// account or other-mod code is touched.

#include <exception>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <fstream>  // paperlog's backtrace.hpp uses std::ifstream without including it
#include "scotland2/shared/loader.hpp"
#include "beatsaber-hook/shared/hooking.hpp"
#include "beatsaber-hook/shared/utils.hpp"
#include "game.hpp"
#include "hud.hpp"
#include "modules.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "renderer.hpp"
#include "saucy/menu.hpp"
#include "saucy/registry.hpp"
#include "saucy/settings.hpp"
#include "scotland2/shared/modloader.h"

namespace {

constexpr auto logger = Paper::ConstLoggerContext("SaucyMenu");

class PaperLog final : public saucy::ILog {
public:
    void info(const std::string& m) override { logger.info("{}", m); }
    void warn(const std::string& m) override { logger.warn("{}", m); }
    void error(const std::string& m) override { logger.error("{}", m); }
};

modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

std::string describe(const std::exception& e, bool withBacktrace);

/// Composition root: owns settings, registry, menu, renderer and HUD for the life of the game.
struct Host {
    PaperLog log;
    game::Input input;
    game::RoomState room;
    saucy::SettingsStore settings;
    saucy::ModuleRegistry registry;
    saucy::MenuController menu;
    PanelRenderer renderer;
    Hud hud;

    static double now() { return unity::realtime(); }

    explicit Host(std::string settingsPath)
        : settings(std::move(settingsPath), &log),
          registry(settings, &room, &log, &Host::now),
          menu(registry, settings, &Host::now, VERSION),
          renderer(menu, settings),
          hud([this]() -> const saucy::Theme& { return menu.theme(); }) {
        settings.load();
        for (auto& m : createModules(hud, input, room)) registry.add(std::move(m));
        menu.onPressed = [this] { pressFeedback(); };
        auto previousNotice = registry.onNotice;
        registry.onNotice = [this, previousNotice](const std::string& text) {
            log.info(text);
            if (previousNotice) previousNotice(text);
        };
        registry.restoreFromSettings();
    }

    /// Runs first, on its own, every frame: nothing that fails later in the frame can skip it.
    void gate() { registry.enforceGate(); }

    void before() {
        try {
            input.poll();
        } catch (...) {
            input = game::Input{};  // never act on stale button state (e.g. Fly kept flying)
            throw;
        }
        menu.poll();
        bool menuButton = settings.data().menuHand == saucy::MenuHand::Left ? input.leftSecondary : input.rightSecondary;
        renderer.setOpen(menuButton && game::tagger() != nullptr, now());
        registry.tick(saucy::TickPhase::Update);
        registry.tick(saucy::TickPhase::PreLocomotion);
    }

    void after() {
        double t = now();
        registry.tick(saucy::TickPhase::LateUpdate);
        renderer.lateUpdate(t);
        hud.update(t);
        settings.flushIfDirty(t);
    }

    void pressFeedback() {
        bool left = renderer.pointerOnLeft();
        try {
            if (settings.data().haptics) game::vibrate(left, 0.5f, 0.04f);
            if (settings.data().buttonSounds) game::playLocalClick(left);
        } catch (const std::exception& e) {
            logger.warn("Press feedback failed: {}", describe(e, false));
        }
    }
};

// Deliberately never deleted: a destructor running at process exit could race a last hook call.
// Shutting down after repeated errors switches everything off and stops using it instead.
Host* host = nullptr;
bool hostFailed = false;
bool frameFailed = false;
int failedFrames = 0;
constexpr int kMaxFailedFrames = 120;  // about 1.5 s of every frame failing

/// The message without side effects. beatsaber-hook's trace_exception::what() also writes a 256-frame
/// backtrace to the log, which would flood it if called every frame; that's done once per message instead.
std::string describe(const std::exception& e, bool withBacktrace) {
    if (auto* t = dynamic_cast<const i2c::trace_exception*>(&e)) {
        if (withBacktrace) t->log_backtrace();
        return t->std::runtime_error::what();
    }
    return e.what();
}
std::string rawMessage(const std::exception* e) { return e ? describe(*e, false) : std::string("unknown exception"); }

/// Logs the first few occurrences of each distinct error, then goes quiet about that one.
void reportError(const char* phase, const std::exception* e) {
    static std::unordered_map<std::string, int> seen;
    if (seen.size() > 64) seen.clear();
    int& count = seen[std::string(phase) + ": " + rawMessage(e)];
    if (++count > 3) return;
    std::string message = e ? describe(*e, count == 1) : rawMessage(nullptr);
    logger.error("{} failed: {}{}", phase, message, count == 3 ? " (further repeats not logged)" : "");
}

void runSafely(const char* phase, void (Host::*step)()) {
    if (!host) return;
    try {
        (host->*step)();
    } catch (const std::exception& e) {
        reportError(phase, &e);
        frameFailed = true;
    } catch (...) {
        reportError(phase, nullptr);
        frameFailed = true;
    }
}

/// Counts frames in which anything failed (not individual successes, which would reset a shared counter).
/// Something persistently wrong outside any one module (e.g. a game update renamed a member the menu itself
/// uses) switches everything off rather than failing every frame.
void endFrame() {
    if (!host) return;
    failedFrames = frameFailed ? failedFrames + 1 : 0;
    frameFailed = false;
    if (failedFrames <= kMaxFailedFrames) return;
    logger.critical("Something failed on {} frames in a row; disabling Saucy Menu for this session", failedFrames);
    try {
        host->registry.disableAllForShutdown();
        host->renderer.destroy();
        host->hud.destroy();
    } catch (...) {
    }
    host = nullptr;  // leaked on purpose, see above
    hostFailed = true;
}

void ensureHost() {
    if (host || hostFailed) return;
    if (!game::tagger()) return;  // wait until the local player exists
    try {
        std::filesystem::path dir = get_data_dir(modInfo.id);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        host = new Host((dir / "settings.json").string());
        logger.info("Saucy Menu {} ready. Hold the secondary button (Y/B) on your menu hand to open it.", VERSION);
    } catch (const std::exception& e) {
        logger.error("Failed to start: {}", describe(e, true));
        hostFailed = true;
    } catch (...) {
        logger.error("Failed to start: unknown exception");
        hostFailed = true;
    }
}

constexpr char kHookNamespace[] = "GorillaLocomotion";
constexpr char kHookClass[] = "GTPlayer";
constexpr char kHookMethod[] = "LateUpdate";

}  // namespace

MAKE_HOOK(gtplayer_late_update, ({kHookNamespace, kHookClass}, kHookMethod, true), void, Il2CppObject* self, MethodInfo* method) {
    ensureHost();
    runSafely("room gate", &Host::gate);
    runSafely("pre-locomotion", &Host::before);
    gtplayer_late_update(self, method);
    runSafely("late update", &Host::after);
    endFrame();
}

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    logger.info("Saucy Menu {} setup", VERSION);
}

extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    i2c::functions::initialize();
    // INSTALL_HOOK aborts the game if the target is missing. Check first, so a game update that renames it
    // just leaves the menu off (with a log line) instead of crashing Gorilla Tag at startup.
    MethodInfo const* target = i2c::find_method({kHookNamespace, kHookClass}, {kHookMethod, 0});
    if (!target || !target->methodPointer) {
        logger.critical("{}.{}.{} not found in this game version; Saucy Menu is not loading", kHookNamespace, kHookClass, kHookMethod);
        return;
    }
    INSTALL_HOOK(logger, gtplayer_late_update);
    logger.info("Installed GTPlayer.LateUpdate hook");
}
