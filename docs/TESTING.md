# In-headset test checklist (0.1)

Automated: `dotnet test` (core logic) and `tools/CompatCheck` (API compatibility). Everything below needs a headset —
none of it has been verified in-game yet. Record pass/fail and notes next to each line.

Setup: BepInEx 5 installed, `SaucyMenu.dll` in `BepInEx/plugins/SaucyMenu/`, game started.
Check `BepInEx/LogOutput.log` contains `Saucy Menu 0.1.0 loading` and `Menu ready`.

## Panel
- [ ] Hold Y: panel appears on the left hand, readable (not mirrored, not facing away). If it faces away, Settings → Flip menu facing, and report.
- [ ] Release Y: panel and fingertip dot disappear.
- [ ] Touch a row with the right index fingertip: it activates once (no double-press), haptic + click.
- [ ] Resting your finger where a button was after a page change does not press the new button.
- [ ] You can't stand on / bump into the panel or the fingertip dot.
- [ ] Header shows title; status line says "Not in a room: all mods available" in the lobby/offline.
- [ ] Footer `<` `>` page through Settings (2 pages); Back/Home work.
- [ ] Search: type "fl" → Fly appears; DEL removes letters; "Clear search" empties.
- [ ] `...` on a module opens detail; Favorite → Yes; it appears under Favorites with a `*`.
- [ ] Settings → Theme cycles 5 themes; Menu size changes size; Menu hand → Right moves panel to right hand and opens with B.
- [ ] Settings persist after restarting the game (`BepInEx/config/SaucyMenu/settings.json`).

## Modules
- [ ] Performance HUD: line at bottom of view with FPS / ms / ping; options toggle parts; disabling removes it.
- [ ] Room Info: "Not in a room" offline; in a room shows `****` code, privacy, count, region, mode; option shows code.
- [ ] Both HUD modules on together: two lines, one panel.
- [ ] Smooth Camera: desktop window view follows your head smoothly; Smoothing/FOV options change it; disabling returns the normal desktop camera.
- [ ] Platforms (offline or private room): grip → slab beside the palm you can stand on; release removes it; "Show platforms: Off" makes them invisible but solid.
- [ ] Fly (offline or private room): hold A to fly where you look; speeds change.

## Room gate
- [ ] Enable Fly + Platforms in a private room, then join a public lobby: both switch off with a toast, status line turns to "Public lobby: movement mods locked", rows show LOCKED.
- [ ] Pressing a locked module in public shows "private rooms only" and does nothing.
- [ ] Info/visual modules keep working in public.

## Robustness
- [ ] No errors in `LogOutput.log` during a 10-minute session.
- [ ] Quitting the game with modules on: no errors; next launch restores them (movement only if not in public).

## Known issues from code review (PC, not yet fixed)
- The fingertip pointer is a Default-layer trigger; if the game's casts hit triggers, a hand could catch on it. Watch for this.
- Every press rebuilds the whole panel (~140 objects); typing may hitch. The Quest build updates in place; port that here.
- Platforms are placed off the controller transform rather than the game's hand offset (the Quest build uses the offset).
- A new platform's collider may sit at the world origin for one physics step before transforms sync.
