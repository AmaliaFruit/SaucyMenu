# Quest in-headset test checklist (0.1)

Automated so far: 53 core tests (host), arm64 compile + link against beatsaber-hook 8.2.1 and 9.4.3, and all 104
runtime-looked-up names found in the 31 Oct 2026 `global-metadata.dat`. **Nothing below has been verified on a headset.**

Logs: `adb logcat | findstr SaucyMenu` from a Windows terminal while the game runs.
Look for `Saucy Menu 0.1.0 setup`, `Installed GTPlayer.LateUpdate hook`, `ready`.

## Startup
- [ ] Game starts normally with the mod installed (no crash at the loading screen).
- [ ] Log shows the three lines above and no `Could not find` errors.

## Panel
- [ ] Hold Y: panel on the left hand, readable. If it faces away, Settings → Flip menu facing, and report.
- [ ] Release Y: panel and fingertip dot disappear.
- [ ] Touch a row with the right index fingertip: activates once, haptic + click.
- [ ] Holding your finger where a button was after a page change does not press the new one.
- [ ] You can't stand on or bump into the panel or dot.
- [ ] `<` `>` page through Settings; Back/Home work; Search keyboard types and finds "fly".
- [ ] `...` → Favorite → shows under Favorites with `*`.
- [ ] Theme / size / hand / flip settings work and persist after restarting the game.

## Modules
- [ ] Performance HUD, Room Info, Session Clock: lines at the bottom of view; options work; disabling removes them.
- [ ] Platforms (offline/private): grip → slab beside the palm you can stand on; release removes it.
- [ ] Fly (offline/private): hold A to fly where you look.

## Room gate
- [ ] Fly/Platforms on in a private room, then join a public lobby: both switch off with a toast; rows show LOCKED.
- [ ] Info modules keep working in public.

## Frame rate
- [ ] With the menu closed and all info modules on, FPS stays at your normal refresh rate.
- [ ] Opening the menu / changing pages doesn't cause a long hitch (> 0.5 s). Note any stutter.
