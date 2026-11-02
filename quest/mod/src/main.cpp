// Saucy Menu for the standalone Quest: scotland2 entry points and the single game hook.
//
// One hook only: GorillaLocomotion.GTPlayer.LateUpdate (the local player's own movement step).
//   before the original: poll input, enforce the room gate, Update + PreLocomotion module ticks
//   after the original:  LateUpdate module ticks, menu panel, HUD, settings save
// It reads nothing from and changes nothing in the game method itself. No anti-cheat, reporting, network,
// account or other-mod code is touched.

#include <exception>
#include <filesystem>
#include <memory>
#include <optional>

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

    void before() {
        input.poll();
        registry.enforceGate();
        menu.poll();
        bool menuButton = settings.data().menuHand == saucy::MenuHand::Left ? input.leftSecondary : input.rightSecondary;
        renderer.setOpen(menuButton && game::tagger() != nullptr);
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
            logger.warn("Press feedback failed: {}", e.what());
        }
    }
};

std::unique_ptr<Host> host;
bool hostFailed = false;
int consecutiveErrors = 0;

void runSafely(const char* phase, void (Host::*step)()) {
    if (!host) return;
    try {
        (host.get()->*step)();
        consecutiveErrors = 0;
    } catch (const std::exception& e) {
        logger.error("{} failed: {}", phase, e.what());
        ++consecutiveErrors;
    } catch (...) {
        logger.error("{} failed with an unknown exception", phase);
        ++consecutiveErrors;
    }
    // Something is persistently wrong outside any one module (e.g. a game update renamed a member the menu
    // itself uses): switch everything off rather than throwing every frame.
    if (consecutiveErrors > 120) {
        logger.critical("Too many consecutive errors; disabling Saucy Menu for this session");
        try {
            host->registry.disableAllForShutdown();
            host->renderer.destroy();
            host->hud.destroy();
        } catch (...) {
        }
        host.reset();
        hostFailed = true;
    }
}

void ensureHost() {
    if (host || hostFailed) return;
    if (!game::tagger()) return;  // wait until the local player exists
    try {
        std::filesystem::path dir = get_data_dir(modInfo.id);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        host = std::make_unique<Host>((dir / "settings.json").string());
        logger.info("Saucy Menu {} ready. Hold the secondary button (Y/B) on your menu hand to open it.", VERSION);
    } catch (const std::exception& e) {
        logger.error("Failed to start: {}", e.what());
        hostFailed = true;
    }
}

}  // namespace

MAKE_HOOK(gtplayer_late_update, ({"GorillaLocomotion", "GTPlayer"}, "LateUpdate", true), void, Il2CppObject* self) {
    ensureHost();
    runSafely("pre-locomotion", &Host::before);
    gtplayer_late_update(self);
    runSafely("late update", &Host::after);
}

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    logger.info("Saucy Menu {} setup", VERSION);
}

extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    i2c::functions::initialize();
    INSTALL_HOOK(logger, gtplayer_late_update);
    logger.info("Installed GTPlayer.LateUpdate hook");
}
