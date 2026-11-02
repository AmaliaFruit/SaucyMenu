# Saucy Menu

A local-only VR mod menu for Gorilla Tag.

- **Standalone Quest 3S** (main target): native C++ mod in [`quest/`](quest/README.md), installed with QuestPatcher.
- **PC VR** (Steam / Meta PC app, or Quest over Link): BepInEx plugin in `src/` (this page).

> Saucy Menu is a fan-made mod. It is **not affiliated with or endorsed by Another Axiom**. Using code mods in Gorilla Tag
> can get your account banned; movement features only run offline, in private rooms or in modded queues, but you use it at your own risk.

- No telemetry, servers, downloads, auto-updates, donations, or remote control.
- No anti-cheat, reporting, age-check or ToS bypasses. One Harmony patch (a prefix on the player's own locomotion update).
- Movement mods switch themselves off the moment you're in a public lobby.

## Features (0.1)
| Mod | Category | Where it runs |
|---|---|---|
| Performance HUD — FPS, frame time, ping | Info | Everywhere (only you see it) |
| Room Info — code (hidden by default), privacy, players, region, mode | Info | Everywhere (only you see it) |
| Smooth Camera — steadier desktop/stream view, FOV | Visual | Everywhere (desktop view only) |
| Platforms — hold grip to stand on air | Movement | Offline / private / modded |
| Fly — hold A to fly where you look | Movement | Offline / private / modded |

Menu: categories, search with an on-panel keyboard, favourites, enabled list, 5 themes, menu hand/size/facing,
sounds, haptics, remember-enabled, reset. Hold **Y** to open, touch with your right index finger.

## Build
Requirements: .NET SDK 8+, Gorilla Tag with **BepInEx 5** installed and launched once.

```
dotnet test tests/SaucyMenu.Core.Tests          # core logic, no game needed
dotnet build src/SaucyMenu -c Release           # compiles against YOUR game install
```

If your game isn't in a standard Steam/Meta folder, pass `-p:GamePath="X:\path\to\Gorilla Tag"` or create
`GamePath.user.props` (see `Directory.Build.props`). The game's DLLs are never committed to this repo.

Check compatibility with your installed game version (no need to launch it):
```
dotnet run --project tools/CompatCheck -c Release -- src/SaucyMenu/bin/Release/SaucyMenu.dll "C:\...\Gorilla Tag"
```

Install by copying `src/SaucyMenu/bin/Release/SaucyMenu.dll` to `Gorilla Tag/BepInEx/plugins/SaucyMenu/`
(or build with `-p:DeployToGame=true` once you've reviewed the code). Then work through `docs/TESTING.md`.

## Docs
- `docs/AUDIT.md` — audit of Seralyth Menu 5.0.1 (architecture, inventory, telemetry/remote admin/bypasses found).
- `docs/audit/seralyth-feature-inventory.csv` — all 1,860 upstream features with file locations and a first-pass verdict.
- `docs/ARCHITECTURE.md` — design, how to add a module, roadmap.
- `docs/TESTING.md` — in-headset checklist.

## License
GPL-3.0 (see `LICENSE`). Saucy Menu is an independent implementation; see `NOTICE`.
