# Seralyth Menu source audit

Audited: `Seralyth/Seralyth-Menu` @ `12b416f` (tag 5.0.1, committed 2026-07-23), GPL-3.0.
Method: read of the entry points, menu core, registry, settings, patches and every network call site, plus a scripted
inventory of all 1,860 registered buttons (`docs/audit/seralyth-feature-inventory.csv`, regenerate with
`docs/audit/inventory.py` + `docs/audit/classify.py`). No upstream code was copied into Saucy Menu.

## 1. Architecture

| Concern | Where | How it works |
|---|---|---|
| Loader entry points | `Plugin.BepInEx.cs`, `Plugin.MelonLoader.cs`, `Plugin.cs` (SharpMonoInjector), `LegacyInjectCompatibility.cs` | Three loaders all call `Bootstrapper.Initialize()`. |
| Bootstrap | `Bootstrapper.cs` | Creates `SeralythMenu/` folders, applies "awake" Harmony patches, reads TOS/telemetry opt-out files, waits for `GorillaTagger.OnPlayerSpawned`, then adds `CoroutineManager`, `NotificationManager`, `CustomBoardManager`, `UI` and starts a 120 s patch-integrity loop. |
| Frame loop | `Menu/Main.cs:73` | `Main` is a Harmony prefix/postfix on `GTPlayer.LateUpdate`. `Main.Prefix()` (line 230) polls input, opens/closes the menu, and runs every enabled button's `method` each frame. |
| Menu rendering | `Menu/Main.cs` `CreateMenu` (2391), `AddButton` (1851), `RecenterMenu` (3075), `CreateReference` (2374) | Rebuilt from scratch: a scaled cube parented to the hand, a world-space `Canvas` with `TextMeshPro` labels, trigger-collider cubes for buttons, a 1 cm sphere "reference" on the other hand as the finger. Alternate layouts: ClickGUI asset, joystick menu, "bark" menu. |
| UI controls | `Classes/Menu/ButtonCollider.cs`, `KeyboardCollider.cs`, `ButtonHelper.cs` | `OnTriggerEnter` against the finger sphere, 0.2 s cooldown, calls `Main.Toggle(text)`. |
| Mod registry | `Menu/Buttons.cs:51` | One `ButtonInfo[][] buttons` jagged array (51 categories) plus a parallel `categoryNames` string array. Buttons are looked up by **display text** (`Buttons.GetIndex("Platform Gravity")`). |
| Mod model | `Classes/Menu/ButtonInfo.cs` | Delegates `method`/`postMethod`/`enableMethod`/`disableMethod`, flags `legal`, `detected`, `isSetting`, `incremental`. Feature code lives as static methods in `Mods/*.cs` (Fun 7.1k lines, Overpowered 6.6k, Visuals 6.3k, Movement 6.1k, Settings 5.8k). |
| Settings | `Classes/Menu/Preferences.cs` | JSON at `SeralythMenu/Seralyth_Preferences.json` (legacy `;;`-separated text still read); saves enabled buttons and their values by name. |
| Desktop UI | `Menu/UI.cs` | IMGUI overlay for PC. |
| Harmony | `Patches/PatchHandler.cs` + 85 files in `Patches/Menu`, 6 in `Patches/Safety` | 177 patch attributes. |
| Build | `SeralythMenu.csproj`, `Directory.Build.*` | netstandard2.1, publicizes `Assembly-CSharp`, **copies the whole game `Managed` folder into the repo** before every build and **auto-copies the DLL into `BepInEx/plugins` after every build**. |

### Dependencies
BepInEx 5.4.23.5, HarmonyX 2.9, `BepInEx.AssemblyPublicizer.MSBuild` 0.4.3, MelonLoader (optional), Newtonsoft.Json 13.0.2
(from the game), WebSocketSharp, a vendored DiscordRPC client (61 files), Photon PUN/Voice, Cinemachine, and 179 direct
references into the game folder. 63 MB of game/BepInEx DLLs and 268 MB of remote resources are committed to the repo.

## 2. Feature inventory

1,860 buttons across 38 populated categories. Full list with registration line, implementing method and file:line is in
`docs/audit/seralyth-feature-inventory.csv`. The `verdict` column is a **heuristic first pass** (category, name keywords,
and whether the method body calls Photon/RPC/serialization APIs) — anything marked REVIEW must be read by hand before porting.

| Verdict | Count | Meaning |
|---|---:|---|
| EXCLUDE: disrupts others / bypasses safety | 557 | Master, Overpowered, Projectile/Sound spam, Safety (anti-report), Admin, Detected, Internal |
| REVIEW-HIGH: network-affecting or targets others | 372 | Method body touches RPCs, RaiseEvent, serialization, projectiles, or the name targets other players ("Gun", "All", "Fling") |
| REVIEW: local, case-by-case | 349 | Mostly Fun, Visual, Room, Important |
| KEEP-PATTERN: menu infrastructure | 260 | Settings, presets, keybinds — reimplemented, not ported |
| PRIVATE-ONLY: local movement | 198 | Movement mods; only acceptable behind a private-room gate |
| EXCLUDE: unfair advantage | 34 | Tag Self, Tag Gun, Tag All, tag aura… |
| EXCLUDE-PUBLIC: information advantage | 24 | ESP/tracers/chams |
| DROP: branding / promo / monetisation | 50 | Discord, Patreon, donate, credits |
| DROP: needs Seralyth server | 16 | Friends system |

The upstream `legal = true` flag marks 403 buttons for its "Legal" build, but that set includes all 84 Admin Mods, so it is
not a usable safety signal.

## 3. Compatibility

- Upstream targets the game build whose assemblies it committed (Assembly-CSharp MVID `37e79143-82ea-4527-b941-57334f472c11`,
  July 2026), Unity 6 (`Rigidbody.linearVelocity`), BepInEx 5.4.23.5.
- Saucy Menu compiles against those references with 0 warnings, and `tools/CompatCheck` resolves all 206 type/member
  references (including the reflected `GTPlayer.LateUpdate`) against them.
- **Not yet checked against your installed game.** Run `tools/CompatCheck` against your install (see README); it prints
  your build's MVID and lists anything missing. Gorilla Tag updates often; re-run after every game update.
- Quest 3S standalone: BepInEx/Harmony do not run on the Quest's Android IL2CPP build. This project targets **PCVR**
  (Steam or Meta PC app, Quest 3S over Link/Air Link/Steam Link).

## 4. Problems found

### Telemetry and data collection (all removed)
- `Classes/Menu/ServerData.cs:416` `TelemetryRequest` POSTs room code, your nickname, region, **your user ID**, privacy,
  player count and mode to `menu.seralyth.software/telemetry` on every room join (`OnJoinRoom`, line 153). Opt-out only by
  creating `Seralyth_DisableTelemetry.txt`.
- `ServerData.cs:463` `PlayerDataSync` uploads **every other player's** user ID, nickname, cosmetics, colour and platform
  to `/syncdata` — third-party data collected without their knowledge.
- `ServerData.cs:502` `ReportFailureMessage` posts your enabled mods to `/reportban` when you are banned.
- Also contacts: `/usercount`, `/reportdata`, `/vote`, `/getfriends` and friend actions (`Managers/FriendManager.cs:706,882`),
  `/tts` (`Menu/Main.cs:4734`), `text.pollinations.ai` (`Managers/AIManager.cs:100`), `translate.googleapis.com`
  (`Managers/TranslationManager.cs:96`), `lazypy.ro` TTS (`Main.cs:4769,4831`), and ~20 assets fetched from GitHub raw at runtime.

### Remote administration (removed)
- `Classes/Menu/Console.cs` listens for Photon events from accounts on a server-supplied admin list
  (`ServerData.cs:285`) and lets them **kick, crash, freeze, teleport, mute, vibrate, set velocity, force-toggle mods,
  override controller input, spawn assets and play sounds** on any menu user's client (`Console.cs:1019-1607`).
- `ServerData.cs:159` `ShouldWeReport` makes every menu user **auto-report players listed by the server** via
  `GorillaPlayerScoreboardLine.ReportPlayer` — server-directed mass reporting.

### Anti-cheat, safety and compliance bypasses (removed)
- `Patches/Safety/AntiCheatPatches.cs` disables `MonkeAgent` reporting, report dispatch, invalid-room closing,
  disconnect-on-flag and the ban quit path.
- `Patches/PatchHandler.cs:202,297` re-applies its patches every 120 s and **unpatches other mods' Harmony patches**
  on the methods it protects.
- `Patches/Menu/PermissionPatch.cs` forces `KIDManager.HasPermissionToUseFeature` to true and `TOSPatches.cs:85`
  short-circuits `KIDManager.UseKID` — bypasses k-ID age/parental-permission checks.
- `TOSPatches.cs` auto-accepts the Terms of Service; `SubscriptionPatches.cs` spoofs subscription status;
  `Patches/Safety/*Telemetry*.cs` block the game's own telemetry and device reporting.

### Unsafe code execution (removed)
- `Mods/Important.cs:455` downloads `QuickSong.exe` from GitHub and runs it.
- `Mods/Settings.cs:857-975` self-updater writes a batch/shell script that `curl`s the latest DLL over the installed one and restarts the game.
- `Managers/PluginManager.cs:230` `Assembly.Load`s DLLs from `SeralythMenu/Plugins`, including ones it downloads.
- `Mods/CustomMaps/Manager.cs:170` instantiates types by reflection.

### Rule conflicts
Another Axiom's [Fan Content and Mod Policy](https://www.GorillaTagVR.com/fan-content-mod-policy) forbids hacks, third-party
servers and in-mod monetisation (donations/subscriptions), and requires a "not affiliated with Another Axiom" disclaimer.
Upstream ships a Discord/Patreon donate button and Patreon-gated mods (`Managers/PatreonManager.cs`), runs its own servers,
and its Master/Overpowered/Advantage/Detected categories are cheats. Note that the policy licenses mods made with the
official Modding Tools; BepInEx code mods are tolerated by the community in private/modded lobbies but carry ban risk.

### Obsolete / maintenance issues
- Three loader paths (BepInEx, MelonLoader, SharpMonoInjector) for one plugin.
- Buttons addressed by display string; renaming a label silently breaks settings and cross-references.
- Static god-classes (`Main.cs` 7.3k lines) with hundreds of public static fields; no isolation — one throwing mod can break the frame loop.
- Committed game assemblies (redistribution of Another Axiom's DLLs) and auto-deploy after every build.
- April-fools, "annoying mode", commented-out theme code, duplicate `Legal Debug` configuration.

## 5. What Saucy Menu keeps
Ideas only, reimplemented from scratch: hand-held panel + fingertip trigger interaction, a per-frame prefix on
`GTPlayer.LateUpdate` for movement, driving the game's "Shoulder Camera" for a smooth desktop view, and favourites/search/themes
as concepts. See `ARCHITECTURE.md`.
