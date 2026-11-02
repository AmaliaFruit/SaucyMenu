# Saucy Menu — standalone Quest 3S

Native C++ mod for the Quest version of Gorilla Tag (Android, IL2CPP), loaded by the **scotland2** mod loader and
using **beatsaber-hook** (a game-agnostic il2cpp library, despite the name). Same design as the PC version in `src/`.

> Not affiliated with or endorsed by Another Axiom. Modding the Quest app means installing a patched copy of the game;
> code mods can get your account banned. Movement mods only run offline, in private rooms or in modded queues.

## How it works
- **One hook**: `GorillaLocomotion.GTPlayer.LateUpdate`, the local player's movement step. Before it: input, room gate,
  module ticks. After it: panel, HUD, settings save. Nothing else in the game is patched.
- **No generated headers.** Game classes are looked up by name at runtime (`unity.cpp`, `game.cpp`). Every name is
  checked against your build's `global-metadata.dat` with `scripts/check-names.sh` (109/109 found for the 31 Oct 2026 build).
- **No colliders on the menu.** Presses are measured from your fingertip to the panel layout (`core/src/layout.cpp`),
  so you can't bump into it.
- **Fault isolation.** Every module call is wrapped; a module that throws is switched off. If the menu itself keeps
  failing (e.g. after a game update), it shuts itself down for the session instead of crashing the game.

## Modules (0.1)
| Mod | Where it runs |
|---|---|
| Performance HUD — FPS, frame time, ping | everywhere (only you see it) |
| Room Info — code (hidden by default), privacy, players, region, mode | everywhere (only you see it) |
| Session Clock — time and time played | everywhere (only you see it) |
| Platforms — hold grip to stand on air | offline / private / modded |
| Fly — hold A to fly where you look | offline / private / modded |

(The PC build's Smooth Camera isn't here: the standalone Quest has no desktop view.)

## Layout
```
core/     Unity-free C++20: registry, room gate, settings, search, themes, menu, layout + hit-testing (56 host tests)
mod/      Quest game layer: src/main.cpp (entry + hook), unity/game bridges, renderer, HUD, modules; qpm.json, CMake
scripts/  build-quest.sh (WSL build), check-names.sh (verify names against a game build)
```

## Build (WSL / Ubuntu)
```
bash quest/scripts/build-quest.sh /mnt/c/Users/<you>/Documents/files
```
Installs cmake/ninja if missing, installs `qpm`, resolves the Android NDK and dependencies, builds, and copies
`SaucyMenu.qmod` + `saucy-build.log` to that folder.

## Unity version
The beatsaber-hook binary must match the game's Unity version: 9.x is built for Unity 6000.3 only, 8.2.1 for earlier
Unity 6. The 31 Oct 2026 Gorilla Tag APK is **Unity 6000.2.9f1** (read from `libunity.so`; IL2CPP metadata v31), so
`qpm.json` pins **beatsaber-hook ^8.2.1**. Its `libil2cpp.so` exports every il2cpp API function beatsaber-hook 8.2.1
loads for Unity 6 (the few missing ones are Unity 2019/2021-only or remapped by beatsaber-hook).
If a game update moves to Unity 6000.3, switch to `^9.4.3` and configure with `-DSAUCY_UNITY_6_3=ON`.
The code compiles and links against both.

## Install (QuestPatcher, on your PC)
1. Connect the headset (developer mode on) and open QuestPatcher. Select Gorilla Tag and patch it with the
   **Scotland2** mod loader. Patching reinstalls the game; your cosmetics and currency are tied to your account.
2. Add `SaucyMenu.qmod` in QuestPatcher's mods tab (or drag it onto the window). It also installs the dependencies.
3. Start the game and hold **Y** to open the menu. Then work through `docs/TESTING-QUEST.md`.

To undo: uninstall the patched game and reinstall Gorilla Tag from the Meta store.

## Run the core tests
```
cmake -S quest/core -B build/core-tests -DRAPIDJSON_INCLUDE_DIR=<path to rapidjson>/include
cmake --build build/core-tests && ./build/core-tests/saucy_core_tests
```

## Never committed
The game APK, `libil2cpp.so`, `global-metadata.dat`, and anything generated from them.
