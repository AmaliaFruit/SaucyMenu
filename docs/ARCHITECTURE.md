# Saucy Menu architecture and plan

## Principles
1. **Local only.** No telemetry, no servers, no downloads, no `Process.Start`, no loading other DLLs. Nothing sends Photon
   events or RPCs on your behalf.
2. **Never touch anti-cheat, reporting, k-ID, ToS, subscriptions or other mods' patches.** One Harmony patch total.
3. **Every feature declares where it may run** (`ModuleSafety`). Movement is gated to offline / private rooms / modded
   queues and is switched off the instant you are in a public lobby.
4. **Isolation.** A module that throws is switched off and reported; the menu and other modules keep running.
5. **Testable core.** Everything except drawing and game calls is plain C# with unit tests that run without Unity.
6. **Nothing deploys itself.** Building never copies into the game unless you pass `-p:DeployToGame=true`.

## Layout

```
src/SaucyMenu.Core/          Unity-free, unit tested (tests/SaucyMenu.Core.Tests)
  Modules/ModuleBase.cs        feature contract: id, name, category, safety, options, lifecycle hooks
  Modules/ModuleSafety.cs      LocalInfo | LocalVisual | PrivateRoomOnly
  Modules/RoomGate.cs          offline / private / modded => allowed; public or unknown => blocked (fails closed)
  Modules/ModuleRegistry.cs    register, enable/disable, per-phase ticks, fault isolation, gate enforcement, options
  Modules/ModuleSearch.cs      ranked multi-word search over name, tags, category, description
  Settings/                    JSON settings, debounced atomic saves, corrupt-file recovery
  Themes/                      5 built-in themes
  Menu/MenuController.cs       the menu as a state machine: views, rows, paging, keyboard, presses
src/SaucyMenu/               BepInEx plugin (compiles the Core sources into one DLL)
  Plugin.cs                    entry point; waits for GorillaTagger.OnPlayerSpawned
  Patches/LocomotionHook.cs    the only patch: prefix on GTPlayer.LateUpdate -> PreLocomotion tick
  Hosting/MenuHost.cs          composition root + frame driver
  Hosting/GameRoomState.cs     NetworkSystem -> IRoomState
  Input/VrInput.cs             one input snapshot per frame from ControllerInputPoller
  UI/VrMenuRenderer.cs         draws MenuController.Current on the hand; fingertip pointer
  UI/MenuButton.cs             trigger button with shared cooldown
  UI/HudService.cs             shared head-locked HUD for info modules
  UI/Visuals.cs                cached flat materials, boxes, TMP text
  Modules/*.cs                 one file per feature
tools/CompatCheck            verifies a built DLL against any game install without launching it
```

### Frame order
`Update`: poll input → enforce gate → controller poll → open/close panel → `Tick(Update)` → HUD → save settings if dirty.
`GTPlayer.LateUpdate` prefix: enforce gate → `Tick(PreLocomotion)` (movement).
`LateUpdate`: `Tick(LateUpdate)` (cameras/visuals) → renderer rebuild-if-changed + pose + toast.

### Adding a module
1. New file in `src/SaucyMenu/Modules/`, subclass `ModuleBase`.
2. Pick the narrowest `Safety`. If it would need network traffic or affect another player, it doesn't belong here.
3. Create everything in `OnEnable`/hooks, undo everything in `OnDisable`.
4. Register it in `MenuHost.Initialize`. Ids are permanent (they key the settings file).
5. Add/extend unit tests if it adds Core logic; add a line to `docs/TESTING.md` for the in-headset check.

## Interaction model
Hold **Y** (left secondary) to show the panel on your left hand; touch buttons with your right index fingertip.
(Swap hands in Settings → Menu hand; then it's **B**.) Module rows: main area toggles, `...` opens the detail page
(description, favourite, options, where it can run). Footer: `<` / Back / Home / `>`.

## Roadmap
Each step ships only after an in-headset check (`docs/TESTING.md`) and `CompatCheck` against the current game.

| Phase | Scope |
|---|---|
| 0.1 (this) | Core + VR panel: categories, search, favourites, enabled list, themes, settings, toasts. Modules: Performance HUD, Room Info, Smooth Camera, Platforms*, Fly*. |
| 0.2 | Fix whatever the first in-headset test finds. Menu position/rotation fine-tuning, rebindable open button, desktop keyboard fallback for testing. |
| 0.3 | More local visuals: FPS cap/vsync toggle, field-of-view for desktop camera presets, hide own cosmetics locally, local time-of-day viewer. |
| 0.4 | More private-room movement: speed boost, long arms, low gravity, noclip — each reviewed from the audit's PRIVATE-ONLY list and reimplemented. |
| 0.5 | Presets (save/load sets of enabled modules), per-module keybinds, notification log. |
| later | Desktop config window (IMGUI), localisation. |

\* private rooms / modded queues / offline only.

Never on the roadmap: anything from the audit's EXCLUDE rows, ESP/tracers, other-player targeting, auto-updaters, remote
assets, accounts, telemetry, donations.
