# Saucy Menu — Quest 3S standalone (in progress)

Native C++ build for the standalone Quest (Android, IL2CPP). Same design as the PC version in `src/`:
module registry with fault isolation, room gate (movement only offline / private / modded), settings,
search, favourites, themes, and the menu state machine.

## Status
- [x] `core/` — Unity-free C++20 port of the menu core, 44 host tests (also clean under ASan/UBSan).
- [ ] Android toolchain: NDK + scotland2 mod loader + beatsaber-hook (hooking / il2cpp access).
- [ ] Game layer: hooks, hand-held VR panel, HUD, and the five modules, built against **your** game build.
- [ ] Packaging as a `.qmod` and installing with QuestPatcher.

## Run the core tests
```
cmake -S quest/core -B build/core-tests -DRAPIDJSON_INCLUDE_DIR=<path to rapidjson>/include
cmake --build build/core-tests
./build/core-tests/saucy_core_tests
```

## What is never committed
The game APK, `libil2cpp.so`, `global-metadata.dat`, and any headers generated from them.
