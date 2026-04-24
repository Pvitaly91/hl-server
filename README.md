# hl-server

`hl-server` bootstraps an empty repository into a Windows-first, VS2022-ready server-side Half-Life workspace for gameplay experiments that must stay compatible with the stock Steam Half-Life client.

The repository vendors a pinned snapshot of Valve's official Half-Life source base under `third_party/valve-halflife-sdk/`, builds only the server-side GameDLL (`hl.dll`) first, keeps no-client runtime edits inside `testbed/runtime/`, and now runs live client-attached testing through a managed `hlserver_testbed` mod folder created directly under the user's real Half-Life install.

## Why the official Valve SDK

- The compatibility baseline is Valve's official `ValveSoftware/halflife` repository, pinned and vendored directly in-tree.
- No TWHL Updated, Unified SDK, Xash3D, ReGameDLL, or other compatibility-diverging fork is used.
- The stock client target still stays DLL-compatible. No custom client DLL is required for either the disposable `-game valve` path or the managed `hlserver_testbed` live mod path.

## What this repo is today

- A reproducible VS2022 Win32 build for the vanilla Half-Life server GameDLL.
- A disposable HLDS test stand for no-client and dedicated flows under `testbed/runtime/`.
- A managed same-root live mod at `Half-Life\hlserver_testbed` for client-attached sessions.
- Server-only experimental Glock, MP5, 357, and shotgun paths for manual stock-client-compatible gameplay iteration without changing the stock client DLL.
- A shared server-side tuning core for Glock, MP5, 357, and shotgun so future weapons can reuse the same spread and damage primitives instead of copying ad-hoc math per weapon.
- A server-side round, team-round, buy, armor, and first-pass match-progression layer for live gameplay testing.

It is not yet a gameplay conversion and it does not ship any proprietary game assets, Steam files, or HLDS binaries.

## Weapon Feel Direction

The current gameplay direction is intentionally narrower than "make full Counter-Strike in Half-Life."

- The goal is to improve stock-client-compatible HLDM weapon feel on the server side.
- Glock should reward careful single shots and punish rapid spam through cadence growth plus timed recovery, not by depending on a hard server-only tap-fire gate.
- 357 should reward patient precision and punish rushed follow-up clicks through the same server-side cadence model.
- MP5 should reward controlled bursts, let long sprays bloom, and recover accuracy when the player pauses.
- Glock and MP5 can now also layer an optional deterministic follow-up pattern over the shared spread model so second and third shots feel more learnable and less like a pure random cone.
- Glock and 357 can now also layer cadence-sensitive penalties over their first-shot and movement logic so click timing matters more than simply holding attack.
- 357 can now also layer an optional deterministic follow-up pattern over that precision model so careful second and third clicks feel more learnable and less like a pure random cone.
- Shotgun primary fire can now also layer a deterministic pellet layout over its existing spread scale so repeated shots feel less like a pure random cone and more like a learnable server-side pattern.
- Movement, air state, crouch stability, and readable headshot damage are part of the tuning target.
- Real-player and fake-verification-client hit telemetry now logs armor before/after, health before/after, damage absorbed by armor, helmet/head-protection state, and direct headshot evidence on the hit line itself.
- The recommended Improved HLDM packs package these weapon-feel changes into deathmatch-oriented configs, with `hldm_skill_default` as the everyday baseline.
- Stock client compatibility is preserved, but client-side recoil and prediction are still only approximated because this repository does not ship a custom client DLL.

## Headshot / Armor Verification

The current armor and helmet model is intentionally server-side and tuning-oriented, not a claim of exact Counter-Strike parity.

- Real players and fake verification clients both use the same custom bullet armor/head-protection path when the experimental armor system is enabled.
- Body armor and head protection are surfaced separately in telemetry even though they still share the player's underlying `armorvalue`.
- A direct `type=hit` or `type=kill` weapon-log line now carries the fields needed for headshot tuning:
  - `victim_is_player`
  - `victim_is_fake`
  - `helmet_equipped`
  - `head_protection_active`
  - `armor_hit_protected`
  - `armor_model`
  - `health_before` / `health_after`
  - `armor_before` / `armor_after`
  - `damage_raw`
  - `damage_to_health`
  - `damage_absorbed`
  - `armor_drain`
  - `verification`

Focused live verification commands:

- `exp_armor_status [player]`
- `exp_armor_set <player> <armor>`
- `exp_helmet_set <player> <0|1>`
- `exp_player_hit_test <attacker> <victim> <weapon> <hitgroup>`
- `exp_target_profile <name>`
- `exp_target_tp_front`
- `exp_dummy_hit_test <weapon> <hitgroup> [attacker]`

Recommended live verification loop:

1. Launch a cfg-driven live session with armor and helmet enabled.
2. Create fake verification clients with `exp_team_fake_add team1 verify_alpha` and `exp_team_fake_add team2 verify_bravo`.
3. Use `exp_armor_set` and `exp_helmet_set` to build a helmeted or unhelmeted target state.
4. Trigger direct body or head hits with `exp_player_hit_test`.
5. Inspect the resulting `type=hit` and `type=kill` lines in the current weapon log, or run the analyzer to summarize direct player/fake-player evidence.

The standing dummy is now also a trustworthy direct-damage verification target for the same workflow. Use:

1. `exp_target_profile unarmored`
2. `exp_target_tp_front`
3. `exp_dummy_hit_test glock chest`
4. `exp_target_profile vest`
5. `exp_target_tp_front`
6. `exp_dummy_hit_test mp5 chest`
7. `exp_target_profile vest_headprotected`
8. `exp_target_tp_front`
9. `exp_dummy_hit_test glock head`
10. `exp_dummy_hit_test 357 head`

After the April 24, 2026 dummy-fidelity fix, both the direct `type=hit` / `type=kill` lines and the matching `type=lab_console` verification summary use the same authoritative before/after snapshot instead of late monster state. The analyzer now reports `consistent dummy hits` and `consistent dummy kills` so the old "one odd dummy line" caveat should not be needed for fresh logs anymore.

For the fuller model notes and the exact live verification example, see [docs/headshot-armor-model.md](docs/headshot-armor-model.md).

## Prerequisites

- Windows 10/11
- Visual Studio 2022 with Desktop development with C++
- Windows SDK
- PowerShell 5.1 or newer
- Git
- One of:
  - an existing Half-Life Dedicated Server install
  - an existing Half-Life install that already contains `hlds.exe`
  - SteamCMD, or permission to let the scripts download SteamCMD into `testbed/cache/`

Recent Windows startup blockers such as `FATAL ERROR (shutting down): Unable to initialize Steam.` and `Assertion Failed: Failed to load "SDL3.dll"` are now surfaced through `scripts/doctor-testbed.ps1` instead of being left as opaque launch failures.

## Quick start

```powershell
.\scripts\bootstrap.ps1
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\doctor-testbed.ps1 -Repair
.\scripts\run-server.ps1 -Configuration Debug -Detached
```

To smoke-test the whole flow non-interactively:

```powershell
.\scripts\smoke-test.ps1 -Configuration Debug -AllowSteamCmdDownload
```

## C++ Config Editor

`tools\HlConfigEditorCpp\` contains the standalone native Win32 editor used to save reusable editor projects as `.hlcfg.json` and export server-ready GoldSrc `.cfg` files for HLDS. It is a Visual Studio 2022 desktop app and stays separate from the server DLL build.

Exact repo paths:

- project folder: `<repo-root>\tools\HlConfigEditorCpp\`
- solution: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.sln`
- project file: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj`
- `Debug|Win32` executable: `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe`
- `Release|Win32` executable: `<repo-root>\tools\HlConfigEditorCpp\bin\Release\Win32\HlConfigEditorCpp.exe`
- deployed live-mod executable: `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`
- self-test summary: `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`
- default live export folder: `<HalfLifeRoot>\hlserver_testbed\`
- staged fallback export folder: `<repo-root>\testbed\mods\hlserver_testbed\`

The `.vcxproj` sets `OutDir` to `$(ProjectDir)bin\$(Configuration)\$(Platform)\`, so the executable paths above are the exact expected build outputs for the two available solution configurations.

Open and build it in Visual Studio 2022:

1. Open `tools\HlConfigEditorCpp\HlConfigEditorCpp.sln` in Visual Studio 2022.
2. If you prefer to open a single project instead of the solution, use `tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj`.
3. Select `Debug` or `Release` and `Win32` in the Visual Studio toolbar. Those are the only configurations defined by the solution.
4. Build the solution. After a successful build, Visual Studio keeps the normal `bin\...\HlConfigEditorCpp.exe` output and also tries to copy the EXE into `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
5. If the Half-Life root cannot be resolved from `HL_EXE`, `HLDS_EXE`, `.env`, or the common install paths, the build still succeeds and prints a warning instead of failing.

Simplest workflow:

1. Build `HlConfigEditorCpp` in Visual Studio 2022.
2. Run the deployed EXE from `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
3. Save the editable project as `.hlcfg.json`.
4. On the `Export` tab, confirm the resolved path fields show the expected Half-Life root, live mod root, and quick-export target.
5. Click `Quick Export to Live Mod`.
6. The editor writes `<HalfLifeRoot>\hlserver_testbed\<project-or-config-name>.cfg`, verifies the file exists, and shows a visible success or error dialog instead of failing silently.
7. In the running HLDS console, use `exp_cfg_apply my_glock.cfg` for a pure cfg refresh or `exp_lab_apply my_glock.cfg` to refresh the cfg and rebuild the dummy in one step.
8. `exec my_glock.cfg` remains available as a legacy fallback, but the `exp_cfg_*` and `exp_lab_apply` commands are the intended live-tuning loop now.

How to use the editor:

1. Edit values on the `General`, `Glock`, `MP5`, `357`, `Shotgun`, `Target Dummy`, `Round Mode`, `Team Round`, and `Buy & Equipment` tabs.
2. Use `File -> Save` or `File -> Save As` to store the editable project as `.hlcfg.json`.
3. Open the `Export` tab. The default filename follows the project or config name, for example `editor_glock_simple.cfg`.
4. `Quick Export to Live Mod` is the primary action. It exports directly into `<HalfLifeRoot>\hlserver_testbed\` and prompts before overwriting an existing file.
5. The `Export` tab now shows the resolved Half-Life root, live mod root, quick-export target, and running editor EXE so the destination is visible before you click anything.
6. `Copy exec command`, `Copy launcher`, `Copy raw cfg`, and the folder-opening buttons all show visible success or error feedback.
7. `Copy exec command` now defaults to `exec my_glock.cfg` when the cfg is exported to the live mod root.
8. `Export to chosen folder` is still available for advanced/custom locations.
9. `Copy launcher` still works for cfgs inside the live mod, including legacy `cfg_profiles\...` exports.

The editor now covers the full currently checked-in server config surface in one project:

- weapon tuning for Glock, MP5, 357, and shotgun
- target dummy tuning
- round-mode rules
- team-round rules
- buy prototype rules
- armor, helmet, and first utility settings

The `General` page now also shows a compact "What This Config Will Affect" summary so it is obvious whether the current project enables round mode, team round mode, buy mode, a specific main loadout, or armor and helmet support.

The editor now also has a dedicated `Browser` tab for the existing preset and match-pack surface:

- weapon presets are listed directly from `configs\glock-presets\`, `configs\mp5-presets\`, `configs\357-presets\`, and `configs\shotgun-presets\`
- match packs are listed from `<HalfLifeRoot>\hlserver_testbed\match_packs\`
- selecting an entry shows a preview plus a simple `current -> incoming` cvar diff
- `Load Into Current Project` merges the selected preset or pack into the open project
- `Copy Apply Command` copies the exact live command for the currently loaded project
- `Quick Export + Copy Apply Command` writes the cfg first, then copies the exact `exp_cfg_apply ...` or `exp_matchcfg_apply ...` command

The editor also has a `Live Server` tab for a thinner direct-apply loop. Enter the target host, port, and `rcon_password`, then use:

- `Test Connection` to send `status`
- `Apply Current CFG` to quick-export the current project and send `exp_cfg_apply <cfg>`
- `Apply Selected Match Pack` to send `exp_matchcfg_apply <pack>`
- `Sandbox Reset` or the combined apply-plus-reset buttons for weapon-lab iteration
- `Apply Sandbox Setup` to send the generated `exp_sandbox_*` setup sequence

If RCON is unavailable or rejected, the editor keeps the workflow usable by copying the exact commands for manual HLDS paste. This is a convenience helper over the existing server console commands, not a new server-control protocol.

The editor also has a `Telemetry` tab for the post-shooting feedback loop. It finds the latest `weapon-debug-*.log` from the Half-Life root, the live `hlserver_testbed\logs\` folder, or the repo `testbed\logs\` folder, then runs `scripts\analyze-weapon-log.ps1` with a selected weapon filter (`all`, `glock`, `mp5`, `357`, or `shotgun`). This keeps the manual analyzer available while making the common "shoot, analyze latest, adjust config" loop visible inside the editor.

The editor also has a `Guided Tests` tab for repeatable manual weapon-feel checks. Choose a weapon, source config or match pack, target profile, saved target spot, and test scenario, then click `Start Test` to send the existing `exp_sandbox_*` / `exp_matchcfg_apply` setup commands through the Live Server RCON helper. If RCON is missing or rejected, the editor copies the exact fallback command sequence for manual HLDS paste. After shooting in the stock client, click `Finish & Analyze` to run the analyzer against the latest weapon log and use `Save Test Report` to write a text report under `<repo-root>\testbed\logs\reports\guided-tests\`.

The editor now also has a `Reports` tab for comparing saved guided runs. New guided reports keep the readable `.txt` file and add a small `.json` sidecar with normalized metadata and metrics; older text-only reports are still listed and parsed where possible. Select two or more reports to compare accepted shots, hits, kills, headshot evidence, pattern/cadence/burst evidence, spread/damage summaries, and consistency warnings, then export the comparison under `<repo-root>\testbed\logs\reports\guided-tests\comparisons\`.

For the current Glock and MP5 feel pass, the most important editor fields are now labeled more directly:

- `Shot growth (cadence bloom per shot)` or `Burst growth (cadence bloom per shot)`
- `Recovery seconds to clear cadence bloom`
- `Crouch stability scale`
- `Max spread clamp`
- `Deterministic pattern mode`
- `Horizontal pattern scale`
- `Vertical pattern scale`
- `Pattern reset time`
- `Pattern max index`

The recommended preset names for this pass are:

- `glock_cadence_soft`
- `glock_cadence_tight`
- `357_precision_duel`
- `357_cadence_headshot`
- `357_pattern_soft`
- `357_pattern_tight`
- `glock_pattern_soft`
- `glock_pattern_tight`
- `mp5_pattern_burst`
- `mp5_pattern_mobile`

JSON vs CFG:

- `.hlcfg.json` is the editor project file. Keep it if you want to reopen the same tuning session later and continue editing.
- `.cfg` is the GoldSrc server config file. HLDS loads this file with `exec`, and the live launchers use it through `-CfgProfile` or `-CfgPath`.

Export behavior:

- When the editor can resolve the real Half-Life root from `HL_EXE` or `HLDS_EXE`, it defaults the export folder to `<HalfLifeRoot>\hlserver_testbed\`.
- If the live root is not available, it falls back to `<repo-root>\testbed\mods\hlserver_testbed\`.
- If you choose a custom folder inside `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`, the legacy `cfg_profiles\...` launcher and `exec cfg_profiles/...` workflow remains supported.
- The `Live Server` tab can hot-send exported cfg, match-pack, sandbox-reset, and custom commands through GoldSrc RCON when the running HLDS has a matching `rcon_password`; manual console paste remains the fallback.
- Exported `.cfg` files can now represent full match configs, not just weapon and dummy tuning. The generated file can include round, team round, buy, armor, and helmet cvars alongside the normal weapon and target settings.

Troubleshooting:

- If `Quick Export to Live Mod` appears to do nothing, the current editor should now always show either a success dialog or a clear error dialog plus an updated `Last Action` status field on the `Export` tab.
- The default quick-export file should appear directly in `<HalfLifeRoot>\hlserver_testbed\`, for example `D:\Steam\steamapps\common\Half-Life\hlserver_testbed\editor_gui_simple.cfg`, `editor_357_test.cfg`, or `editor_shotgun_test.cfg`.
- To confirm you are testing the deployed build, start `D:\Steam\steamapps\common\Half-Life\hlserver_testbed\HlConfigEditorCpp.exe` and compare its timestamp with the normal build output under `tools\HlConfigEditorCpp\bin\Debug\Win32\`.
- If Visual Studio warns that deployment failed because the destination EXE is in use, close the running `HlConfigEditorCpp.exe` from the mod root and rebuild.

Built-in self-test:

- Run `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe --self-test`.
- When the editor resolves the repository root, it writes the summary to `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`.
- When the live Half-Life root is available, the self-test writes example Glock, MP5, 357, shotgun, and full match `.hlcfg.json` projects plus exported `.cfg` files such as `<HalfLifeRoot>\hlserver_testbed\editor_glock_simple.cfg`, `<HalfLifeRoot>\hlserver_testbed\editor_mp5_simple.cfg`, `<HalfLifeRoot>\hlserver_testbed\editor_357_test.cfg`, `<HalfLifeRoot>\hlserver_testbed\editor_shotgun_test.cfg`, `<HalfLifeRoot>\hlserver_testbed\editor_duel_357.cfg`, `<HalfLifeRoot>\hlserver_testbed\editor_team_mp5.cfg`, and `<HalfLifeRoot>\hlserver_testbed\editor_buy_armor.cfg`.
- If the live root is not available, the self-test falls back to `<repo-root>\testbed\mods\hlserver_testbed\`.

For a step-by-step walkthrough with exact file paths, Glock and MP5 examples, and live launch commands, see [docs/cpp-config-editor.md](docs/cpp-config-editor.md).

## Using exported editor configs in live play

The live same-root launcher supports two config entry points:

- `-CfgProfile <mod-relative-path-or-bare-filename>` for cfg files already under `<HalfLifeRoot>\hlserver_testbed\`, for example `my_glock.cfg` or `cfg_profiles\my_glock.cfg`
- `-CfgPath <absolute-or-repo-relative-path>` for cfg files stored anywhere else

Cfg-driven live sessions apply settings in this order:

1. base engine and server defaults
2. launcher infrastructure settings such as networking, logging, and `servercfgfile`
3. the exported editor `.cfg`
4. explicit command-line overrides, if any

When `-CfgPath` or `-CfgProfile` is supplied, the exported cfg becomes the source of truth and the built-in demo presets are skipped for that session.

Bare filenames now resolve in this order:

1. `<HalfLifeRoot>\hlserver_testbed\<name>.cfg`
2. `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\<name>.cfg`

The launcher prints the exact resolved path before launch.

## Recommended stable live-lab workflow

This is the recommended integrated path for future server work. Legacy demo and older export paths still exist, but the direct live-mod, editor-exported cfg, live cfg command, and reliable target workflow below is the preferred day-to-day loop.

1. Edit values in `HlConfigEditorCpp`.
2. `Quick Export to Live Mod` so the cfg lands directly in `<HalfLifeRoot>\hlserver_testbed\`.
3. Join the live server and stand where you want the dummy loop to anchor.
4. Run `exp_target_mark default` once to save a persistent named spot for the current map.
5. Apply your tuning cfg with `exp_cfg_apply editor_glock_simple.cfg`, or use `exp_lab_apply editor_glock_simple.cfg` to apply the cfg and rebuild the dummy in one command.
6. For later sessions, select the persisted spot with `exp_target_use_saved default`, then rebuild the dummy with `exp_target_respawn`. Use `exp_target_tp_front` only when you want a temporary fresh anchor-front placement.
7. Change dummy presets on the fly with `exp_target_profile unarmored`, `exp_target_profile vest`, or `exp_target_profile vest_headprotected`.
8. Inspect state any time with `exp_cfg_status`, `exp_target_list`, and `exp_target_status`.
9. Test in-game, then review the normal weapon log and analyzer output.

## Editor Match Config Workflow

The editor can now build whole match-rule configs instead of only weapon or dummy presets. The intended daily path is:

1. Open the deployed `HlConfigEditorCpp.exe` from `<HalfLifeRoot>\hlserver_testbed\`.
2. Choose a weapon/loadout baseline on the weapon tabs.
3. Configure the rules layer on `Round Mode`, `Team Round`, and `Buy & Equipment`.
4. Use one of the built-in match templates when you want a starting point:
   `duel_glock`, `duel_357`, `team_mp5`, `team_shotgun`, `armor_test`, or `buy_test`.
5. Save the editable project as `.hlcfg.json`.
6. Quick-export the runtime cfg into `<HalfLifeRoot>\hlserver_testbed\`.
7. In the running server, load it with `exp_cfg_apply my_match.cfg` or legacy `exec my_match.cfg`.

Practical examples:

- Duel config: enable `sv_exp_round_mode`, keep `sv_exp_team_round_mode 0`, and set `sv_exp_round_loadout_mode` to `glock` or `357`.
- Team config: enable both `sv_exp_round_mode` and `sv_exp_team_round_mode`, set team names, choose `dm_spawns` or `manual_spots`, and assign per-team loadouts.
- Buy-enabled config: enable `sv_exp_buy_mode`, set freeze-only buying, start money, rewards, weapon catalog toggles, and costs.
- Armor/helmet test config: enable `sv_exp_armor_mode` and `sv_exp_helmet_mode`, then export a cfg such as `editor_buy_armor.cfg`.

One exported full-match config was verified live in a real client-attached session on `2026-04-22`: `exp_cfg_apply editor_buy_armor.cfg` flipped round, buy, armor, and helmet cvars on the running server, including `sv_exp_round_mode "1"`, `sv_exp_buy_mode "1"`, `sv_exp_armor_mode "1"`, and `sv_exp_helmet_mode "1"`.

## Match Config Packs

Match packs are a thin usability layer over the existing cfg workflow. They do not replace plain cfg files. Each pack is simple JSON metadata plus a referenced cfg, stored under:

- `<HalfLifeRoot>\hlserver_testbed\match_packs\`

The testbed installer also syncs the checked-in starter packs into the disposable runtime mod folder so `scripts\run-server.ps1` can apply the same pack names without a manual copy step.

The built-in checked-in starter packs currently include:

- `hldm_skill_default`
- `hldm_precision_duel`
- `hldm_mp5_burst`
- `hldm_shotgun_control`
- `hldm_headshot_lab`
- `duel_glock`
- `duel_357`
- `team_mp5_buy`
- `team_shotgun`
- `armor_test`
- `aim_lab`

The `hldm_*` packs are the recommended Improved HLDM path. They keep classic deathmatch framing by default and do not force round mode, team round mode, buy mode, economy, or match progression unless the pack is explicitly a lab pack. The default recommendation is:

```text
exp_matchcfg_apply hldm_skill_default
```

Use the focused variants when tuning one area: `hldm_precision_duel` for Glock/357 precision, `hldm_mp5_burst` for controlled MP5 bursts, `hldm_shotgun_control` for deterministic shotgun pellets, and `hldm_headshot_lab` for dummy/headshot verification. These are experimental feel presets, not final balance and not full Counter-Strike parity.

Each pack describes at least:

- pack name
- description
- cfg path
- recommended weapon under test
- recommended target profile when useful
- recommended mode/tags/notes

Live commands:

- `exp_matchcfg_list`
- `exp_matchcfg_apply <name>`
- `exp_matchcfg_status`

Recommended daily flow:

1. Start a live session.
2. Run `exp_matchcfg_list`.
3. Apply the recommended Improved HLDM default with `exp_matchcfg_apply hldm_skill_default`, or choose a focused/advanced pack such as `exp_matchcfg_apply hldm_mp5_burst`, `exp_matchcfg_apply duel_glock`, or `exp_matchcfg_apply team_mp5_buy`.
4. Inspect the resolved state with `exp_matchcfg_status`, `exp_cfg_status`, and the normal round/team/buy status commands.
5. If you want to leave pack mode and go back to a plain cfg, use `exp_cfg_apply my_match.cfg` or legacy `exec my_match.cfg`.

The native editor now understands match-pack metadata too. If the `General` page includes a pack name and the cfg is exported into the live mod, the editor also writes a sidecar JSON file into `match_packs\` so that exported configs can be reused through `exp_matchcfg_apply`.

The same workflow is now available from inside the editor's `Browser` tab:

1. Browse a checked-in weapon preset or a live/runtime match pack.
2. Read the selected entry preview and the `current vs incoming` diff.
3. Click `Load Into Current Project`.
4. Use `Quick Export + Copy Apply Command`.
5. Paste the copied `exp_cfg_apply ...` or `exp_matchcfg_apply ...` command into the running server.

## Weapon Sandbox Mode

Weapon sandbox mode is a server-side shortcut for repeatable live tuning. It does not add client UI or a new gameplay mode; it wires together the existing cfg/match-pack apply path, deterministic lab loadouts, saved target spots, target dummy profiles, and weapon telemetry.

Minimal loop:

```text
exp_sandbox_start
exp_sandbox_weapon mp5
exp_sandbox_pack hldm_mp5_burst
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

`exp_sandbox_reset` enables weapon telemetry, applies the selected cfg or pack if one is set, grants the selected weapon with practical ammo to the first live player, applies the selected dummy profile, selects the saved target spot when present, and respawns the dummy. Use `exp_sandbox_status` to see whether the weapon is owned/active and whether the dummy is alive. Use `exp_sandbox_verify` for the next useful manual or `exp_dummy_hit_test` command.

The sandbox keeps old workflows intact. `exp_cfg_apply`, `exp_matchcfg_apply`, `exp_lab_apply`, and the target commands still work directly outside sandbox mode.

One real client-attached session on `2026-04-22` verified the full pack loop on `crossfire`: `exp_matchcfg_list` enumerated six starter packs, `exp_matchcfg_apply duel_glock` switched the server into a round-based Glock duel, `exp_matchcfg_apply team_mp5_buy` switched the same live server into a team/buy pack, and a later `exp_cfg_apply editor_buy_armor.cfg` proved the older direct cfg path still remained usable afterward.

Integration provenance for this recommended path is recorded in [docs/stable-live-lab-state.md](/D:/DEV/CPP/HL-Server/docs/stable-live-lab-state.md).

Live lab console commands:

- `exp_cfg_apply <cfg_name_or_path>` applies a cfg from the live mod root or `cfg_profiles\` fallback.
- `exp_cfg_reload` re-executes the currently tracked cfg without restarting the session.
- `exp_cfg_status` prints whether cfg-driven mode is active, which cfg is tracked, and when it was last applied.
- `exp_matchcfg_list` prints the available match packs under `<HalfLifeRoot>\hlserver_testbed\match_packs\`.
- `exp_matchcfg_apply <name>` resolves the pack metadata, applies the referenced cfg, and records the active pack metadata for later status/debugging.
- `exp_matchcfg_status` prints the current active pack, pack metadata, and the match-pack directory scan state.
- `exp_sandbox_start`, `exp_sandbox_weapon <glock|mp5|357|shotgun>`, `exp_sandbox_cfg <cfg>`, `exp_sandbox_pack <name>`, `exp_sandbox_target <profile>`, `exp_sandbox_spot <name|none>`, `exp_sandbox_reset`, `exp_sandbox_status`, and `exp_sandbox_verify` provide a repeatable weapon-plus-dummy verification loop without replacing direct cfg or pack commands.
- `exp_match_start` starts or rearms match progression when team round mode is enabled.
- `exp_match_stop` disables match progression and returns the server to plain round flow.
- `exp_match_restart` resets score, halftime, and side mapping, then starts a fresh match.
- `exp_match_status` prints current score, halftime state, side mapping, and match-end state.
- `exp_match_swap` manually swaps sides for local verification.
- `exp_lab_apply <cfg_name_or_path>` applies the cfg and then respawns the current target with a single summary.
- `exp_target_spawn` enables and spawns the standing dummy.
- `exp_target_clear` removes the standing dummy and disables automatic respawn.
- `exp_target_mark [name]` saves or updates a named target spot for the current map. If no name is provided, it writes `default`.
- `exp_target_unmark [name]` removes a named target spot for the current map. If no name is provided, it removes `default`.
- `exp_target_list` prints all saved target spots for the current map, including the active selection, storage source, and last update time.
- `exp_target_use_saved <name>` selects the active named target spot to use on the next `exp_target_respawn`.
- `exp_target_respawn` rebuilds the dummy, preferring the active saved spot first, then the `default` spot when no active spot is selected, then the current anchor search, then the last known good transform.
- `exp_target_status` prints the current dummy profile, anchor state, saved-spot file/load status, active saved spot, all saved spot names, last known good transform, last spawn failure, and whether respawn is currently possible.
- `exp_target_tp_front` moves or respawns the dummy in front of the current live player anchor.
- `exp_target_profile <name>` switches between `unarmored`, `vest`, and `vest_headprotected`, then refreshes the target when possible.
- `exp_round_start` enables round mode and starts the freeze-time to live loop when at least one player is present.
- `exp_round_restart` forces a clean round reset, respawns players, reapplies the configured loadout, and starts freeze time again.
- `exp_round_status` prints current round state, player counts, timers, loadout mode, start health/armor, and the last winner/reason.
- `exp_round_stop` disables round mode and restores normal deathmatch respawn flow.
- `exp_round_slay [all|team1|team2]` is a small server-side round test helper that can eliminate one live player, every live player, or every live player on one configured team.
- `exp_team_join <player> <team>`, `exp_team_autoassign`, and `exp_team_status` provide lightweight server-side team assignment and debugging for small live round tests.
- `exp_team_fake_add <team> [name]` and `exp_team_fake_clear` are optional local verification helpers when only one real client is available and you still need to exercise last-team-alive round flow.
- `exp_team_spawn_mark <team> [name]`, `exp_team_spawn_unmark <team> <name>`, `exp_team_spawn_list`, `exp_team_spawn_use <team> <name>`, and `exp_team_spawn_status` manage persistent per-team round spawns under `<HalfLifeRoot>\hlserver_testbed\team_spawns\<map>.json`.
- `exp_buy_list`, `exp_buy_status`, `exp_buy <item> [player]`, `exp_buy_clear [player]`, `exp_buy_grant [player]`, `exp_buy_setmoney <player> <amount>`, and `exp_armor_status [player]` provide the first server-side round buy and equipment prototype for freeze-time weapon, armor, helmet, and handgrenade purchases without a client buy menu.

The live dummy now forces `mp_allowmonsters 1` before spawning, because the underlying server-side `monster_generic` entity is otherwise removed immediately on deathmatch maps.

Persistent named target spots are stored under `<HalfLifeRoot>\hlserver_testbed\target_spots\<map>.json`. Each map file is human-readable JSON with an `active_spot` field and one or more named saved spots. See [docs/target-spots.md](/D:/DEV/CPP/HL-Server/docs/target-spots.md) for the exact file layout and command flow.

## Round Modes

The repo now has a first server-side round loop for live duel testing plus a simple team-oriented extension for 1v1 / 2v2 style live checks. It is intentionally narrow:

- round start with freeze time
- deterministic health, armor, and loadout reset
- no respawn during the live round
- elimination-based round end
- automatic next-round restart after a short delay
- optional two-team assignment with last-team-alive win logic

It is not a full Counter-Strike ruleset yet. There is now a small server-side buy and equipment prototype for round mode, but there is still no client buy menu, no full economy tree, no full utility suite, and no polished join-in-progress flow in this pass. The armor and helmet logic is a deterministic server-side approximation for live testing, not a claim of exact CS armor parity.

The same rules layer now also includes first-pass match progression for team rounds:

- logical score tracking per team
- rounds-to-win and max-rounds match end conditions
- halftime
- side swap
- explicit match start, restart, stop, status, and manual swap commands

The current side-swap model is explicit and server-side: halftime reassigns players into the opposite physical `alpha` / `bravo` buckets, so the existing per-team spawn slots and per-team loadout slots keep following those buckets after the swap.

Main round cvars:

- `sv_exp_round_mode 0|1`
- `sv_exp_round_freeze_time`
- `sv_exp_round_restart_delay`
- `sv_exp_round_start_health`
- `sv_exp_round_start_armor`
- `sv_exp_round_no_respawn`
- `sv_exp_round_friendlyfire`
- `sv_exp_round_weapon_profile`
- `sv_exp_round_loadout_mode none|glock|mp5|357|shotgun`

Additional team-round cvars:

- `sv_exp_team_round_mode 0|1`
- `sv_exp_team_round_teamplay 0|1`
- `sv_exp_team_round_spawn_mode dm_spawns|manual_spots`
  `manual_spots` now prefers persisted per-team saved spawns first, then falls back to normal map deathmatch spawns with explicit status and telemetry when a saved team spawn is missing or blocked.
- `sv_exp_team_round_team1_name`
- `sv_exp_team_round_team2_name`
- `sv_exp_team_round_team1_loadout`
- `sv_exp_team_round_team2_loadout`
- `sv_exp_team_round_team1_health`
- `sv_exp_team_round_team2_health`
- `sv_exp_team_round_team1_armor`
- `sv_exp_team_round_team2_armor`
- `sv_exp_buy_mode 0|1`
- `sv_exp_buy_freeze_only 0|1`
- `sv_exp_buy_team_shared_catalog 0|1`
- `sv_exp_buy_start_money`
- `sv_exp_buy_round_win_reward`
- `sv_exp_buy_round_loss_reward`
- `sv_exp_buy_max_money`
- `sv_exp_buy_allow_glock`
- `sv_exp_buy_allow_mp5`
- `sv_exp_buy_allow_357`
- `sv_exp_buy_allow_shotgun`
- `sv_exp_buy_allow_armor`
- `sv_exp_buy_allow_helmet`
- `sv_exp_buy_allow_handgrenade`
- `sv_exp_buy_cost_glock`
- `sv_exp_buy_cost_mp5`
- `sv_exp_buy_cost_357`
- `sv_exp_buy_cost_shotgun`
- `sv_exp_buy_cost_armor`
- `sv_exp_buy_cost_helmet`
- `sv_exp_buy_cost_handgrenade`
- `sv_exp_armor_mode`
- `sv_exp_armor_start_value`
- `sv_exp_armor_max_value`
- `sv_exp_armor_health_fraction`
- `sv_exp_armor_drain_scale`
- `sv_exp_helmet_mode`
- `sv_exp_helmet_start_enabled`
- `sv_exp_helmet_headshot_protection`
- `sv_exp_match_mode 0|1`
- `sv_exp_match_rounds_to_win`
- `sv_exp_match_max_rounds`
- `sv_exp_match_enable_halftime 0|1`
- `sv_exp_match_halftime_after_round`
- `sv_exp_match_side_swap 0|1`
- `sv_exp_match_reset_money_on_halftime 0|1`
- `sv_exp_match_reset_loadout_on_halftime 0|1`
- `sv_exp_match_auto_restart_after_end 0|1`
- `sv_exp_match_end_delay`

Recommended duel loop:

1. Export and apply the weapon cfg you want to test, for example `exp_cfg_apply editor_357_test.cfg`.
2. Configure round mode in the cfg or console, for example:
   `sv_exp_round_mode 1`
   `sv_exp_round_loadout_mode 357`
   `sv_exp_round_start_health 100`
   `sv_exp_round_start_armor 0`
3. Run `exp_round_start`.
4. Use `exp_round_status` to inspect the current state.
5. Play the round. Dead players stay out until the automatic restart.
6. Use `exp_round_restart` for a forced clean reset or `exp_round_stop` to go back to normal deathmatch respawn behavior.

Recommended small-team loop:

1. Apply the weapon cfg you want to test, for example `exp_cfg_apply editor_mp5_simple.cfg`.
2. Configure team round mode, for example:
   `sv_exp_round_mode 1`
   `sv_exp_team_round_mode 1`
   `sv_exp_team_round_team1_loadout mp5`
   `sv_exp_team_round_team2_loadout 357`
3. Run `exp_team_autoassign` or `exp_team_join <player> <team>`.
4. Use `exp_team_status` and `exp_round_status` to confirm assignments and alive counts.
5. Run `exp_round_start`.
6. The round ends when one configured team has no living players left, then restarts after the configured delay.
7. If you only have one real client available, `exp_team_fake_add <team>` can stand in as a small local verification helper while you validate team round transitions.

Recommended persisted team-spawn setup for a map:

1. Enable team round mode and saved spawn usage, for example:
   `sv_exp_round_mode 1`
   `sv_exp_team_round_mode 1`
   `sv_exp_team_round_spawn_mode manual_spots`
2. Place one live player on alpha and mark the spawn:
   `exp_team_join Poni alpha`
   `exp_team_spawn_mark alpha default`
3. Place one live player on bravo and mark the spawn:
   `exp_team_join Poni bravo`
   `exp_team_spawn_mark bravo default`
4. Select the active saved spawns:
   `exp_team_spawn_use alpha default`
   `exp_team_spawn_use bravo default`
5. Restart the round with `exp_round_restart`.
6. Use `exp_team_spawn_status` to confirm that round start used the saved team spawns and that the file under `<HalfLifeRoot>\hlserver_testbed\team_spawns\<map>.json` is loaded.

Recommended match-progression loop:

1. Apply a team cfg or match pack, for example `exp_matchcfg_apply team_mp5_buy`.
2. Enable team round mode and short local timings when needed:
   `sv_exp_round_freeze_time 1`
   `sv_exp_round_restart_delay 1`
   `sv_exp_team_round_spawn_mode manual_spots`
3. Enable match progression:
   `sv_exp_match_mode 1`
   `sv_exp_match_rounds_to_win 2`
   `sv_exp_match_max_rounds 2`
   `sv_exp_match_enable_halftime 1`
   `sv_exp_match_halftime_after_round 1`
   `sv_exp_match_side_swap 1`
4. Optionally reset halftime state aggressively for local testing:
   `sv_exp_match_reset_money_on_halftime 1`
   `sv_exp_match_reset_loadout_on_halftime 1`
5. If you only have one human player, add the existing fake helper:
   `exp_team_fake_add bravo`
6. Use `exp_match_status` and `exp_team_status` to inspect the current score and side mapping.
7. Run `exp_match_start`.
8. After halftime, remember that players have been reassigned into the opposite physical `alpha` / `bravo` buckets, so saved team spawns and team loadouts follow those buckets automatically.
9. Use `exp_match_restart` for a clean new scoreline or `exp_match_stop` to go back to plain rounds.

One real client-attached session on `2026-04-22` verified the first full progression loop on `crossfire`: the real client `Poni` plus `bravo_fake` produced a round-1 score update to `alpha 1 - 0 bravo`, halftime triggered immediately after round 1, side swap reassigned the two players into the opposite physical `alpha` / `bravo` buckets, round 2 ended the match at logical score `alpha 2 - 0 bravo`, `exp_match_restart` reset the score into match 2, and a later `exp_match_stop` plus `sv_exp_team_round_mode 0` returned the server to plain round mode.

Recommended simple buy-prototype loop:

1. Apply the cfg you want to test, for example `exp_cfg_apply editor_357_test.cfg`.
2. Enable round mode and buy mode, for example:
   `sv_exp_round_mode 1`
   `sv_exp_team_round_mode 1`
   `sv_exp_buy_mode 1`
   `sv_exp_buy_freeze_only 1`
   `sv_exp_buy_start_money 2500`
   `sv_exp_team_round_team1_loadout none`
   `sv_exp_team_round_team2_loadout none`
3. Use `exp_buy_list` to inspect the allowed shared catalog and costs.
4. Start the round with `exp_round_start`.
5. During freeze, buy with `exp_buy 357 Poni`, `exp_buy armor Poni`, `exp_buy helmet Poni`, or `exp_buy handgrenade Poni`.
6. Use `exp_buy_status` and `exp_armor_status Poni` to confirm current money, selected loadout override, armor value, helmet state, and the current handgrenade grant.
7. When the round goes live, freeze-only buy mode closes automatically and later `exp_buy` attempts fail with an explicit reason.
8. After round end and restart, the current round's selected items reset for the next freeze while the player's money carries forward with the configured win/loss reward.

For the full state model, command reference, and current limitations, see [docs/round-mode.md](/D:/DEV/CPP/HL-Server/docs/round-mode.md).

## Shared Weapon Tuning Core

Glock, MP5, 357, and shotgun now share a lightweight server-side tuning core for the common tuning dimensions that were already present in the repo:

- base spread
- ground movement penalty
- air movement penalty
- duck penalty scale
- first-shot accuracy and its speed threshold
- spread recovery inputs
- max spread
- base damage
- headshot scale
- headshot lethal handling

Weapon-specific behavior stays in the weapon wrappers:

- Glock still owns its tap-fire press/hold semantics.
- MP5 still owns burst-growth and burst-specific spread accumulation.
- 357 stays a simple single-shot wrapper on top of the shared spread and damage helpers plus its own lab loadout wiring.
- Shotgun uses the shared spread and per-pellet damage profile helpers for primary fire, while keeping pellet-count, per-pellet traces, and aggregation-specific behavior in the shotgun wrapper and telemetry layer.
- Lab loadout and target workflow stay unchanged.

The editor and cfg surface is intentionally unchanged. Existing `sv_exp_glock_*`, `sv_exp_mp5_*`, `sv_exp_357_*`, `sv_exp_shotgun_*`, and exported cfg files still load through the same live-lab workflow. See [docs/shared-weapon-tuning.md](/D:/DEV/CPP/HL-Server/docs/shared-weapon-tuning.md) for the implementation note and verification summary.

Launch live with a Glock cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile my_glock.cfg
```

Launch live with an MP5 cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile my_mp5.cfg
```

Launch live with a 357 cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile editor_357_test.cfg
```

Launch live with a shotgun cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile editor_shotgun_test.cfg
```

Use the shorthand cfg alias when you only want to supply a profile plus extra launcher flags:

```bat
scripts\run-testbed.bat play-direct-cfg my_glock.cfg
scripts\run-testbed.bat play-direct-cfg my_mp5.cfg
scripts\run-testbed.bat play-direct-cfg editor_357_test.cfg
scripts\run-testbed.bat play-direct-cfg editor_shotgun_test.cfg
```

Legacy `cfg_profiles` paths are still accepted:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_glock.cfg
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_mp5.cfg
```

Launch from a cfg stored outside the live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgPath "D:\some-folder\editor_glock_simple.cfg"
```

Manual HLDS console fallback:

```text
exp_cfg_apply my_glock.cfg
exp_target_respawn

exp_cfg_apply my_mp5.cfg
exp_target_profile vest

exp_cfg_apply editor_357_test.cfg
exp_target_profile unarmored
exp_target_respawn

exp_cfg_apply editor_shotgun_test.cfg
exp_target_profile unarmored
exp_target_respawn

exp_lab_apply cfg_profiles/my_legacy_glock.cfg
```

Cfg-driven observability:

- the launcher prints whether the session is `demo` or `cfg-driven`
- cfg-driven sessions print the requested cfg, the active live-mod path, and the `exec` profile
- launch metadata under `testbed\logs\hlds-*-launch.txt` records `Cfg mode`, `Cfg profile`, `Cfg source`, and `Cfg active`
- `exp_cfg_status` prints the tracked active cfg path, last successful apply, last failure, and current cfg metadata from the running server
- weapon debug logs now include cfg-mode session fields plus compact `type=cfg` events when cfg apply or reload commands run successfully or fail
- root-level exported `*.cfg` files, the deployed `HlConfigEditorCpp.exe`, and legacy `cfg_profiles\...` files are preserved across same-root live-mod refreshes so the simplified workflow remains usable
- the editor deployment now writes a live-mod marker when the folder only contains known editor/live-mod content so later cfg-driven launcher refreshes can safely take ownership of the folder

## Experimental 357 Workflow

357 now plugs into the same shared server-side tuning core and live-lab loop as Glock and MP5. The implementation is still experimental and is meant for server-side iteration, not a claim of stock Counter-Strike parity.

Key 357 cvars:

- `sv_exp_357_primary_enabled`
- `sv_exp_357_profile_name`
- `sv_exp_357_primary_base_spread`
- `sv_exp_357_primary_ground_move_penalty`
- `sv_exp_357_primary_air_move_penalty`
- `sv_exp_357_primary_duck_penalty_scale`
- `sv_exp_357_primary_first_shot_accuracy`
- `sv_exp_357_primary_first_shot_speed_threshold`
- `sv_exp_357_primary_spread_recovery`
- `sv_exp_357_primary_max_spread`
- `sv_exp_357_primary_cadence_mode`
- `sv_exp_357_primary_cycle_time`
- `sv_exp_357_primary_click_penalty`
- `sv_exp_357_primary_click_penalty_scale`
- `sv_exp_357_primary_click_reset_time`
- `sv_exp_357_primary_hold_penalty_scale`
- `sv_exp_357_pattern_mode`
- `sv_exp_357_pattern_scale_x`
- `sv_exp_357_pattern_scale_y`
- `sv_exp_357_pattern_reset_time`
- `sv_exp_357_pattern_max_index`
- `sv_exp_357_primary_damage`
- `sv_exp_357_primary_headshot_scale`
- `sv_exp_357_primary_headshot_lethal`
- `sv_exp_357_lab_loadout`
- `sv_exp_357_lab_ammo`
- `sv_exp_357_lab_autoswitch`

Checked-in 357 preset JSON files live under `configs/357-presets/`:

- `default.json`
- `precision_test.json`
- `headshot_test.json`
- `357_precision_duel.json`
- `357_cadence_headshot.json`
- `357_pattern_soft.json`
- `357_pattern_tight.json`

Recommended 357 loop:

1. Build and run the deployed editor from `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
2. Configure the `357` tab and export `editor_357_test.cfg` with `Quick Export to Live Mod`.
3. Launch or reconnect to the live server.
4. Run `exp_cfg_apply editor_357_test.cfg`.
5. If this is the first setup on the map, run `exp_target_mark default`.
6. Run `exp_target_use_saved default` and `exp_target_respawn`.
7. Use `exp_target_profile unarmored` when you want clean reset and precision checks, or `exp_target_profile vest_headprotected` when you want armored/headshot confirmation.
8. Fire a few deliberate clicks, then a quicker follow-up string, then wait long enough for the cadence and pattern state to reset before firing again.
9. Review the weapon log with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon 357`.

The editor, launcher, and cfg command path stay unchanged. 357 is simply another shared-core weapon that can be exported to cfg, applied live, and exercised against the same target dummy workflow.

## Experimental Shotgun Workflow

Shotgun now plugs into the same shared server-side tuning core and live-lab loop as Glock, MP5, and 357. This pass is focused on experimental primary fire only. It is meant for server-side iteration, not a claim of stock Counter-Strike parity or a completed alt-fire conversion.

Key shotgun cvars:

- `sv_exp_shotgun_primary_enabled`
- `sv_exp_shotgun_profile_name`
- `sv_exp_shotgun_primary_base_spread`
- `sv_exp_shotgun_primary_ground_move_penalty`
- `sv_exp_shotgun_primary_air_move_penalty`
- `sv_exp_shotgun_primary_duck_penalty_scale`
- `sv_exp_shotgun_primary_first_shot_accuracy`
- `sv_exp_shotgun_primary_first_shot_speed_threshold`
- `sv_exp_shotgun_primary_spread_recovery`
- `sv_exp_shotgun_primary_max_spread`
- `sv_exp_shotgun_primary_shot_growth`
- `sv_exp_shotgun_pattern_mode`
- `sv_exp_shotgun_pattern_scale_x`
- `sv_exp_shotgun_pattern_scale_y`
- `sv_exp_shotgun_pattern_reset_time`
- `sv_exp_shotgun_pattern_max_index`
- `sv_exp_shotgun_primary_pellet_spread_mode`
- `sv_exp_shotgun_primary_damage_per_pellet`
- `sv_exp_shotgun_primary_pellet_count`
- `sv_exp_shotgun_primary_headshot_scale`
- `sv_exp_shotgun_primary_headshot_lethal`
- `sv_exp_shotgun_lab_loadout`
- `sv_exp_shotgun_lab_ammo`
- `sv_exp_shotgun_lab_autoswitch`

Checked-in shotgun preset JSON files live under `configs/shotgun-presets/`:

- `default.json`
- `close_quickkill.json`
- `precision_test.json`
- `shotgun_pattern_soft.json`
- `shotgun_pattern_tight.json`
- `shotgun_close_quickkill.json`
- `shotgun_precision_test.json`

Recommended shotgun loop:

1. Build and run the deployed editor from `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
2. Configure the `Shotgun` tab and export `editor_shotgun_test.cfg` with `Quick Export to Live Mod`.
3. Use `Deterministic pellet pattern`, `Pellet pattern X scale`, `Pellet pattern Y scale`, `Pattern reset time`, and `Per-shot spread growth` when you want a learnable close-range spread shape instead of the older random-like cone.
4. Launch or reconnect to the live server.
5. Run `exp_cfg_apply editor_shotgun_test.cfg`.
6. If this is the first setup on the map, run `exp_target_mark default`.
7. Run `exp_target_use_saved default` and `exp_target_respawn`.
8. Use `exp_target_profile unarmored` and `exp_target_tp_front` when you want deterministic close-range dummy kill checks under the new pellet pattern.
9. Review the weapon log with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon shotgun`.

Shotgun telemetry model:

- Shotgun logs one aggregated hit or kill event per shell per victim, not one noisy line per pellet.
- `health_before`, `health_after`, `armor_before`, `armor_after`, `applied_damage`, `damage_to_health`, `damage_absorbed`, and `armor_drain` are derived from the server-side pellet aggregate for that shell and are the authoritative values to trust while tuning.
- `pellets_hit` and `headshot_pellets` show how many pellets contributed to that aggregated result.
- `hitgroup="mixed"` means the shell combined multiple pellet hitgroups on the same victim; `headshot=1` still means at least one pellet hit the head.
- For repeatable live checks, `exp_verify_shotgun_hit [player]` fires one real server-side shotgun `PrimaryAttack()` from the resolved live player, and `sv_exp_shotgun_verify_autofire 1` plus `sv_exp_shotgun_verify_autofire_count N` can arm a short automatic verification sequence after the live player joins.

The editor, launcher, and cfg command path stay unchanged. Shotgun is another shared-core weapon that can be exported to cfg, applied live, and exercised against the same target dummy workflow.

## Live BAT launchers

These BAT files are the ready-to-run entry points for live server testing from Explorer or `cmd.exe`. They run the doctor with repair before launching, create or refresh the managed `hlserver_testbed` mod directory directly under the stock Half-Life root, copy the built `hl.dll` there, and then launch both `hlds.exe` and `hl.exe` from the same `D:\Steam\steamapps\common\Half-Life` root on `-game hlserver_testbed`.

Launch the default Glock live lab:

```bat
scripts\play-glock-live.bat
```

Launch the default MP5 live lab:

```bat
scripts\play-mp5-live.bat
```

Open the menu launcher:

```bat
scripts\play-live-test.bat
```

Show the latest all-weapon analyzer summary:

```bat
scripts\show-latest-log-analysis.bat
```

Show the latest Glock-focused analyzer summary:

```bat
scripts\show-latest-glock-analysis.bat
```

Show the latest MP5-focused analyzer summary:

```bat
scripts\show-latest-mp5-analysis.bat
```

Use the new aliases from the existing BAT dispatcher:

```bat
scripts\run-testbed.bat play-glock
scripts\run-testbed.bat play-mp5
scripts\run-testbed.bat play-target
scripts\run-testbed.bat play-direct -CfgProfile my_glock.cfg
scripts\run-testbed.bat play-direct-cfg my_mp5.cfg
scripts\run-testbed.bat play-glock-clientmatched
scripts\run-testbed.bat play-mp5-clientmatched
scripts\run-testbed.bat play-menu
```

Defaults:

- `scripts\play-glock-live.bat` launches the Glock lab with preset `cs_tight` and target profile `vest_headprotected`.
- `scripts\play-mp5-live.bat` launches the MP5 lab with preset `cs_burst` and target profile `vest`.
- `scripts\play-target-test-live.bat` launches the same direct `hlserver_testbed` live path in Glock mode and prints the target-control command reminder before the session hand-off.
- `scripts\play-hlserver-testbed-direct.bat -CfgPath ...` or `-CfgProfile ...` launches cfg-driven live play and disables the built-in demo preset pass for that session.
- You can override the defaults, for example:

```bat
scripts\play-glock-live.bat -Preset cs_mobile -TargetProfile unarmored
scripts\play-mp5-live.bat -Preset cs_mobile -TargetProfile vest_headprotected
```

What to expect from the live launchers:

- the managed `Half-Life\hlserver_testbed` mod directory is repaired or refreshed first
- the built `hl.dll` is copied into `Half-Life\hlserver_testbed\dlls\`
- HLDS starts from the real Half-Life root on `-game hlserver_testbed`
- the stock client launch is requested from the same Half-Life root on `-game hlserver_testbed`
- stock maps and assets still come from standard `Half-Life\valve\` through `fallback_dir "valve"`
- the one-player lab dummy appears for the live session flow
- logs are written under `testbed/logs/`
- analyzer output remains under `testbed/logs/reports/`
- the console prints the chosen client root, live mod root, actual launched `hlds.exe`, actual launched `hl.exe`, `Same-root launch`, `content_match`, chosen map, and the log locations

If the fix is working, the live launcher and the doctor will show:

- the stock client root under `Client root`
- `Game dir            : hlserver_testbed`
- the managed live mod under `Live mod root`
- `Same-root launch    : yes`
- `content_match       : yes`
- `Launch hlds` and `Launch hl` under `D:\Steam\steamapps\common\Half-Life\`
- `Live content verdict: server and client launch from the same Half-Life root through the managed live mod`
- matching `Runtime crossfire SHA256` and `Client crossfire SHA256` values
- the client connects without `Your map [maps/crossfire.bsp] differs from the server's.`

If `hl.exe` is not found, the live BAT wrappers stop before the session hand-off and print the remediation path. Set `HL_EXE` in `.env` or pass `-HlExe D:\Steam\steamapps\common\Half-Life\hl.exe`.

## Live target dummy commands

The live target is a server-side standing dummy named `Damage Dummy`. It uses a stock human model and stock assets, stays compatible with the standard Steam Half-Life client, and is not a real networked player or bot.

Quick launch:

```bat
scripts\play-target-test-live.bat
scripts\run-testbed.bat play-target
```

Useful server console commands:

```text
exp_target_spawn
exp_target_clear
exp_target_mark default
exp_target_unmark default
exp_target_list
exp_target_use_saved default
exp_target_respawn
exp_target_status
exp_target_tp_front
exp_target_profile unarmored
exp_target_profile vest
exp_target_profile vest_headprotected
```

What the commands do:

- `exp_target_spawn` enables the target and spawns it now if a live player anchor or saved target position is available.
- `exp_target_clear` removes the current target and disables automatic target spawning until you enable it again. It does not erase saved target spots.
- `exp_target_mark [name]` stores a persistent named target spot for the current map and makes it the active saved spot. Omitting the name writes `default`.
- `exp_target_unmark [name]` removes a named target spot. Omitting the name removes `default`.
- `exp_target_list` prints all named spots for the current map and shows which one is active.
- `exp_target_use_saved <name>` selects the active named target spot that `exp_target_respawn` should prefer.
- `exp_target_respawn` recreates the target immediately, preferring the active saved spot first, then the `default` spot when no active spot is selected, then a fresh anchor search, then the last known good transform.
- `exp_target_status` prints the current target state, profile, placement settings, target-spots file/load state, active saved spot, saved spot names, last known good transform, active anchor, and the latest placement failure.
- `exp_target_tp_front` moves the current target, or spawns a fresh one, into a predictable position in front of the live player.
- `exp_target_profile <name>` switches the built-in live profile and refreshes the target immediately when practical.

Implementation note:
The server-side dummy forces `mp_allowmonsters 1` when it needs to spawn `monster_generic`, so the target loop still works on live deathmatch maps such as `crossfire`.

Built-in live profiles:

- `unarmored`
  Baseline unarmored target.
- `vest`
  Armored torso target.
- `vest_headprotected`
  Armored target with protected head enabled.

Reliable live target workflow:

1. Launch `scripts\play-target-test-live.bat`.
2. Join the live session on `-game hlserver_testbed`.
3. Stand in a good firing lane and run `exp_target_mark default` once.
4. Run `exp_target_status` or `exp_target_list` to confirm the active spot, disk file, and next respawn source.
5. Use `exp_target_respawn` after a kill when you want a clean full-health target immediately.
6. Use `exp_target_profile unarmored`, `vest`, or `vest_headprotected` before comparing body shots, headshots, and protected-head behavior.
7. For later sessions, run `exp_cfg_apply editor_glock_simple.cfg`, `exp_target_use_saved default`, and then `exp_target_respawn`.
8. Use `exp_lab_apply editor_glock_simple.cfg` when you want to refresh the current cfg and rebuild the dummy in one step.
9. If respawn fails, rerun `exp_target_status` and inspect the explicit saved-spot, anchor, and last-failure lines before remarking with `exp_target_mark default`.

After the session, analyze the latest telemetry:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
```

The analyzer now summarizes target lifecycle counts for spawn, clear, respawn, reposition, hit, kill, headshot hit, and headshot kill evidence alongside the existing Glock and MP5 telemetry.

## Runtime doctor

Inspect the currently selected runtime source and the disposable runtime:

```powershell
.\scripts\doctor-testbed.ps1
```

Inspect the current live client/root pairing and the representative `crossfire.bsp` hashes:

```powershell
.\scripts\check-live-map-match.ps1
.\scripts\check-live-map-match.ps1 -PreferClientMatchedRuntime
```

Inspect or repair the runtime specifically for a client-attached live session:

```powershell
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair
```

Force a disposable-runtime refresh and let the repo provision a dedicated HLDS cache under `testbed/cache/` when needed:

```powershell
.\scripts\doctor-testbed.ps1 -Repair
```

Use the thin BAT alias from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat doctor
```

If you only want to refresh the disposable runtime without the full repair flow:

```powershell
.\scripts\install-testbed.ps1 -Configuration Debug
```

`doctor-testbed.ps1` prints:

- the selected runtime source root
- the selected `hlds.exe`
- the selected `hl.exe` when one is available
- the actual disposable runtime root
- the launched `testbed/runtime\hlds.exe`
- the launched client executable path that will be used for the live session
- the resolved stock client root used for live play
- the effective content source root used for the live mod and standard `valve` fallback
- `Same-root launch` and `content_match` as explicit `yes` / `no` preflight verdicts
- the representative `valve\maps\crossfire.bsp` paths and SHA256 hashes for the runtime, the selected source, and the client root
- the disposable runtime root and runtime manifest
- key executable-side files or directories that are present or missing
- the current blocker classification and remediation guidance

`-Repair` is intentionally stronger than a plain install:

- it refreshes `testbed/runtime/`
- it can refresh the cached SteamCMD-provisioned HLDS template
- it may create `testbed/runtime/steam_appid.txt` when the selected source does not carry one but the source type makes the AppID unambiguous

Runtime source selection now prefers, in order:

1. explicit `-TemplateRoot` or `HL_RUNTIME_TEMPLATE`
2. explicit `-HldsExe` or `HLDS_EXE`
3. explicit `-HlExe` or `HL_EXE` when that root also contains `hlds.exe`
4. the cached dedicated HLDS template under `testbed/cache/hlds-template`
5. an installed `Half-Life Dedicated Server` runtime
6. a regular Half-Life client install only as a fallback when it already contains `hlds.exe`

That preference order is still correct for no-client or dedicated-only flows. Live client-attached play is different: `play-glock-live.bat`, `play-mp5-live.bat`, and `.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime` now create or refresh the managed `hlserver_testbed` mod directly under the real Half-Life root, copy the built `hl.dll` into `dlls\`, and rely on standard `valve` fallback content so the client and server launch from the same root without copying the whole game into `testbed/runtime/`.

To build Release instead:

```powershell
.\scripts\build.ps1 -Configuration Release
.\scripts\install-testbed.ps1 -Configuration Release -AllowSteamCmdDownload
```

## Experimental Glock mode

The current gameplay pass is server-only and limited to Glock primary fire. It keeps the disposable runtime on `-game valve` and stays compatible with the stock Steam Half-Life client, but the altered tap-fire cadence and spread logic can feel slightly different from local client prediction because no client DLL code is changed.

Build and install:

```powershell
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\doctor-testbed.ps1 -Repair
```

Launch the disposable HLDS runtime in vanilla mode with PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached
```

Launch the disposable HLDS runtime in experimental Glock mode with PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached -EnableExperimentalGlock
```

List the available versioned Glock tuning presets:

```powershell
.\scripts\list-glock-profiles.ps1
```

Launch a detached server with an experimental Glock preset:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached -GlockProfile cs_tight
```

Launch the same disposable runtime from `cmd.exe` or Explorer with the BAT wrapper:

```bat
scripts\run-testbed.bat
```

Enable the experimental Glock mode from the BAT wrapper:

```bat
scripts\run-testbed.bat experimental
```

Enable the experimental Glock mode with debug telemetry and dedicated weapon logging:

```bat
scripts\run-testbed.bat experimental-debug
```

List the available presets from the BAT wrapper:

```bat
scripts\run-testbed.bat glock-profiles
```

Launch detached experimental-debug HLDS with a preset from the BAT wrapper:

```bat
scripts\run-testbed.bat glock-profile cs_tight
```

Launch detached experimental-debug HLDS with the same named preset from PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -EnableExperimentalGlock -EnableExperimentalGlockDebug -GlockProfile cs_tight -Detached
```

Launch the one-click manual Glock session:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug
```

Launch the one-click manual Glock session with a preset:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -GlockProfile cs_tight
```

Launch the same named-profile one-click session from `cmd.exe` or Explorer with the thin BAT alias:

```bat
scripts\run-testbed.bat glock-session-profile cs_tight
```

Launch the one-player Glock lab session that enables the stock-asset server-side dummy:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -LabDummy
```

List the checked-in lab target profiles:

```powershell
.\scripts\list-glock-lab-targets.ps1
```

Launch the same lab flow with a checked-in armored target profile:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -LabDummy -LabTargetProfile vest_headprotected
```

Launch the same lab flow with a checked-in preset:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -LabDummy -GlockProfile cs_tight
```

Launch the lab flow with both a weapon preset and a dummy target profile:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -LabDummy -GlockProfile cs_tight -LabTargetProfile vest_headprotected
```

Launch the same one-player lab flow from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-lab
```

List the available target profiles from the BAT wrapper:

```bat
scripts\run-testbed.bat glock-lab-targets
```

Launch a one-player lab session with a named target profile from the BAT wrapper:

```bat
scripts\run-testbed.bat glock-lab-target vest_headprotected
```

Launch the preset-capable lab alias from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-lab-profile cs_tight
```

Launch the same preset-capable lab alias and forward a named target profile:

```bat
scripts\run-testbed.bat glock-lab-profile cs_tight -LabTargetProfile vest
```

Launch the same session and analyze the latest Glock telemetry log after you finish the manual firing pass and press Enter in the original console:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -AnalyzeLatestOnExit
```

Launch the same one-click session from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-session
```

Forward extra launch arguments through the BAT wrapper when needed:

```bat
scripts\run-testbed.bat experimental -Port 27016 -Map crossfire
```

The one-click Glock session always:

- reinstalls the selected disposable runtime into `testbed/runtime/`
- launches HLDS in experimental-debug mode on `-game valve`
- waits for either the session-ready weapon log header or the traditional `Started map "<map>"` marker before printing the checklist
- opens `scripts/tail-weapon-log.ps1` in a second PowerShell window when the session weapon log is ready, if possible
- launches a stock `hl.exe` and auto-connects to `127.0.0.1:<port>` unless `-NoClient` is set
- prints a compact in-terminal checklist for the manual firing pass

Preset merge order stays predictable:

1. built-in experimental Glock defaults
2. selected preset from `configs/glock-presets/`
3. selected target profile from `configs/glock-lab-targets/`
4. explicit `-SetCvar` or `-Cvars` overrides

These presets are experimental tuning helpers for server-side iteration. They are not claims of exact Counter-Strike values or exact Counter-Strike feel.

For no-client or disposable-runtime sessions, the helper still prefers `testbed/runtime/hl.exe` when that mirrored runtime contains a stock client executable. For live same-root sessions, it instead launches `D:\Steam\steamapps\common\Half-Life\hl.exe -game hlserver_testbed` so the client and server share the same Half-Life root. If no stock client is found, the script leaves a server-only session running and prints a clear message instead of failing.

Run a server-only one-click session when you only want readiness and telemetry validation:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -NoClient
```

Analyze the newest dummy session log after a manual lab pass:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
```

Assert that a fixture or live log contains the minimum armored-dummy evidence:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\scripts\fixtures\sample-weapon-debug.log -RequireArmoredDummyHits -RequireProtectedDummyHeadshotHits -RequireProtectedDummyHeadshotKills -RequireDummyLethalHeadshotEvidence
```

The one-player Glock lab uses a stock `monster_generic` with an allowed stock human model (`models/barney.mdl` by default, `models/scientist.mdl` also allowed) so the disposable runtime stays on `-game valve` and the stock Steam client can still connect. Checked-in target profiles live under `configs/glock-lab-targets/` as small JSON files:

- `unarmored`
  No dummy armor and no head protection.
- `vest`
  Experimental torso armor only. Chest and stomach hits use the dummy-only armor model, but headshots still bypass that armor.
- `vest_headprotected`
  Experimental armored target with head protection enabled. Headshots also use the dummy-only armor model.

Example armored dummy lifecycle and hit telemetry:

```text
[weaponlog] type=dummy_spawn ts=... map=crossfire dummy="Glock Lab Dummy" entindex=24 dummy_class=glock_lab_dummy dummy_model="models/barney.mdl" health=100.0 autorespawn=1 respawn_delay=1.00 spawn_distance=256.0 anchor="Player" anchor_entindex=1 anchor_userid=3 origin="256.0 0.0 0.0" yaw=180.0 profile="cs_tight" target_profile="vest_headprotected" spawn_health=100.0 spawn_armor=100.0 head_protected=1 armor_health_fraction=0.500 armor_drain_scale=1.000
[weaponlog] type=hit ts=... map=crossfire attacker="Player" attacker_entindex=1 attacker_userid=3 victim="Glock Lab Dummy" victim_entindex=24 victim_userid=-1 victim_kind=dummy victim_class=glock_lab_dummy victim_model="models/barney.mdl" weapon=glock fire=primary hitgroup=chest hitgroup_id=2 headshot=0 experimental=1 profile="cs_tight" target_profile="vest_headprotected" trace_damage=10.0000 applied_damage=5.0000 armor_before=100.0 armor_after=95.0 damage_raw=10.0000 damage_to_health=5.0000 damage_absorbed=5.0000 armor_drain=5.0000 dummy_armor_before=100.0 dummy_armor_after=95.0 armor_applied=1 head_protected=1 headshot_lethal_active=1 headshot_lethal_applied=0
[weaponlog] type=kill ts=... map=crossfire attacker="Player" attacker_entindex=1 attacker_userid=3 victim="Glock Lab Dummy" victim_entindex=24 victim_userid=-1 victim_kind=dummy victim_class=glock_lab_dummy victim_model="models/barney.mdl" weapon=glock fire=primary hitgroup=head hitgroup_id=1 headshot=1 experimental=1 profile="cs_tight" target_profile="vest_headprotected" trace_damage=150.0000 applied_damage=75.0000 health_before=75.0 health_after=0.0 armor_before=75.0 armor_after=0.0 damage_raw=150.0000 damage_to_health=75.0000 damage_absorbed=75.0000 armor_drain=75.0000 dummy_armor_after=0.0 armor_applied=1 head_protected=1 headshot_lethal_active=1 headshot_lethal_applied=1
```

The Glock lab dummy is a one-player testing aid, not a perfect substitute for real player-vs-player testing. It gives real server-side hitgroup, headshot, kill, respawn, and armor-model telemetry against a stationary stock model, but it does not validate stock-client feel, movement behavior, or exact PvP pacing.

The lab dummy armor model is experimental and dummy-only. It is useful for comparing unarmored versus armored or head-protected target states, but it is not guaranteed exact parity with real player armor. Head-protected lethal-headshot evidence against the dummy proves only that the logged dummy armor model ran for that target state.

## Experimental MP5 mode

The repo now carries a parallel server-side MP5 primary experiment that stays on `-game valve`, keeps the stock client compatible, and reuses the same disposable testbed plus analyzer pipeline as the Glock work.

The current MP5 recommendation is still not "full CS." It is a stock-client-compatible server-side approximation built around two cooperating layers:

- deterministic early-burst pattern progression so the first few follow-up shots are learnable
- burst-growth plus recovery so 2-5 shot bursts stay useful while long held spray blooms much harder

The focused spray-control knobs for this pass are:

- `sv_exp_mp5_primary_burst_growth`
- `sv_exp_mp5_primary_burst_max_additional_spread`
- `sv_exp_mp5_primary_spread_recovery`
- `sv_exp_mp5_primary_burst_reset_time`
- `sv_exp_mp5_primary_hold_penalty_scale`
- `sv_exp_mp5_pattern_scale_x`
- `sv_exp_mp5_pattern_scale_y`
- `sv_exp_mp5_pattern_reset_time`

List the checked-in MP5 presets:

```powershell
.\scripts\list-mp5-profiles.ps1
```

Launch a detached MP5 profile run directly with PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached -EnableExperimentalMp5 -EnableExperimentalWeaponDebug -Mp5Profile cs_burst
```

Launch the same detached profile-oriented session flow from the BAT wrapper:

```bat
scripts\run-testbed.bat mp5-profile cs_burst -NoClient -NoTail
```

Launch the one-click MP5 session flow:

```powershell
.\scripts\run-mp5-test-session.ps1 -Configuration Debug
```

Launch the one-player MP5 lab flow with a dummy target:

```powershell
.\scripts\run-mp5-test-session.ps1 -Configuration Debug -LabDummy -Mp5Profile cs_mobile -LabTargetProfile vest_headprotected
```

Use the MP5 BAT aliases from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat mp5-profiles
scripts\run-testbed.bat mp5-session
scripts\run-testbed.bat mp5-lab
scripts\run-testbed.bat mp5-lab-profile cs_mobile -NoClient -NoTail -LabTargetProfile vest_headprotected
```

Analyze the newest MP5-focused log:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon mp5
```

Recommended live verification loop for the current MP5 pass:

1. Export or apply a burst-focused cfg such as `editor_mp5_simple.cfg` or preset `mp5_controlled_burst`.
2. Run `exp_target_use_saved default`, `exp_target_respawn`, and `exp_target_tp_front`.
3. Fire one short 2-5 shot burst, then one longer held spray, then wait long enough for reset.
4. Check the live log or analyzer for `pattern_mode`, `pattern_index`, `burst_growth`, `hold_penalty`, `next_additional_spread`, and `burst_reset`.

Run MP5-focused analyzer assertions against a known synthetic fixture:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\scripts\fixtures\sample-mp5-weapon-debug.log -Weapon mp5 -RequireWeaponAccepted -RequireWeaponHits -RequireWeaponKills -RequireWeaponHeadshotKills -RequireBurstGrowthEvidence -RequireMovementPenaltyEvidence
```

Example MP5 accepted and hit telemetry:

```text
[weaponlog] type=accepted ts=2026-04-16T13:30:01.090 map=fixture_range player="Synthetic Fixture" entindex=1 userid=1 weapon=mp5 fire=primary experimental=1 profile="cs_burst" firstshot=0 spread=0.0600 base=0.0400 move_penalty=0.0120 burst_additional_spread=0.0080 burst_index=2 speed2d=180.0 maxspeed=270.0 grounded=1 ducking=0 delta_prev=0.090 clip=48
[weaponlog] type=kill ts=2026-04-16T13:30:01.181 map=fixture_range attacker="Synthetic Fixture" attacker_entindex=1 attacker_userid=1 victim="Glock Lab Dummy" victim_entindex=24 victim_userid=-1 victim_kind=dummy victim_class=glock_lab_dummy victim_model="models/barney.mdl" weapon=mp5 fire=primary hitgroup=head hitgroup_id=1 headshot=1 experimental=1 profile="cs_burst" target_profile="vest_headprotected" trace_damage=137.0000 applied_damage=68.5000 health_before=68.5 health_after=0.0 armor_before=68.5 armor_after=0.0 damage_raw=137.0000 damage_to_health=68.5000 damage_absorbed=68.5000 armor_drain=68.5000 dummy_armor_after=0.0 armor_applied=1 head_protected=1 headshot_lethal_active=1 headshot_lethal_applied=1
```

MP5 session logs are written under `testbed/logs/weapon-debug-*.log`, and MP5 analyzer JSON or CSV exports still land under `testbed/logs/reports/`.

This is still server-authoritative experimentation for stock clients. It proves only what the server logged for spread, burst-growth, movement-penalty, dummy-hit, and headshot-damage paths. It does not prove client-side recoil feel, prediction quality, or exact Counter-Strike parity.

## Glock comparison matrices

Checked-in manual comparison matrices live under `configs/glock-comparison-matrices/`.

List the available matrices:

```powershell
.\scripts\list-glock-comparison-matrices.ps1
```

Run a named matrix in the guided lab flow:

```powershell
.\scripts\run-glock-comparison-matrix.ps1 -Matrix quick_smoke -Configuration Debug
```

Run the same matrix in no-client auto-advance mode for tooling verification only:

```powershell
.\scripts\run-glock-comparison-matrix.ps1 -Matrix quick_smoke -Configuration Debug -NoClient -NoTail -AutoAdvance -MaxSteps 1
```

Compare analyzer JSON outputs directly:

```powershell
.\scripts\compare-weapon-reports.ps1 -ReportDir .\scripts\fixtures\comparison -ExportMarkdown -ExportCsv -ExportJson
```

Use the BAT aliases from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-matrices
scripts\run-testbed.bat glock-matrix quick_smoke
scripts\run-testbed.bat glock-matrix quick_smoke -NoClient -NoTail -AutoAdvance -MaxSteps 1
scripts\run-testbed.bat glock-compare -ReportDir .\scripts\fixtures\comparison
```

`scripts\run-testbed.bat glock-compare` with no extra arguments will compare the latest generated matrix report directory if one exists. Otherwise it tells you to pass `-ReportDir` or `-ReportPaths`.

The matrix runner writes all generated artifacts under the disposable reports area:

- per-step analyzer JSON and CSV:
  `testbed/logs/reports/glock-comparison-matrices/<matrix>-<timestamp>/steps/<step>/`
- consolidated matrix comparison JSON, CSV, and Markdown:
  `testbed/logs/reports/glock-comparison-matrices/<matrix>-<timestamp>/`

Example comparison summary produced from the synthetic fixtures:

```text
MatrixStep         Profile  Target             Acc Rej DHits DKills ProtHS AvgDmg Signals
unarmored          baseline unarmored            4   1     3      1      0 23.00  tap:Y move:Y hs:Y armor:N prot:N lethal:Y
vest               baseline vest                 5   1     4      1      0 14.00  tap:Y move:Y hs:Y armor:Y prot:N lethal:N
vest_headprotected baseline vest_headprotected   6   2     5      1      2 20.50  tap:Y move:Y hs:Y armor:Y prot:Y lethal:Y
```

The matrix runner structures a reproducible manual checklist, per-step tagging, and report aggregation. It does not generate gameplay evidence by itself. In `-AutoAdvance` mode it is only validating the tooling path, not live Glock behavior.

## Mixed weapon comparison matrices

Checked-in mixed comparison matrices now live under `configs/weapon-comparison-matrices/`.

List the available mixed matrices:

```powershell
.\scripts\list-weapon-comparison-matrices.ps1
```

Run a named mixed matrix in the guided lab flow:

```powershell
.\scripts\run-weapon-comparison-matrix.ps1 -Matrix mixed_quick_smoke -Configuration Debug
```

Run the same matrix in no-client auto-advance mode for tooling verification only:

```powershell
.\scripts\run-weapon-comparison-matrix.ps1 -Matrix mixed_quick_smoke -Configuration Debug -NoClient -NoTail -AutoAdvance -MaxSteps 2
```

Compare mixed analyzer JSON outputs directly:

```powershell
.\scripts\compare-weapon-reports.ps1 -ReportDir .\scripts\fixtures\comparison-mixed -ExportMarkdown -ExportCsv -ExportJson
```

Use the BAT aliases from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat weapon-matrices
scripts\run-testbed.bat weapon-matrix mixed_quick_smoke
scripts\run-testbed.bat weapon-matrix mixed_quick_smoke -NoClient -NoTail -AutoAdvance -MaxSteps 2
scripts\run-testbed.bat weapon-compare -ReportDir .\scripts\fixtures\comparison-mixed
```

`scripts\run-testbed.bat weapon-compare` with no extra arguments will compare the latest generated mixed matrix report directory if one exists. Otherwise pass `-ReportDir` or `-ReportPaths`.

The mixed matrix runner writes all generated artifacts under the disposable reports area:

- per-step analyzer JSON and CSV:
  `testbed/logs/reports/weapon-comparison-matrices/<matrix>-<timestamp>/steps/<step>/`
- consolidated mixed comparison JSON, CSV, and Markdown:
  `testbed/logs/reports/weapon-comparison-matrices/<matrix>-<timestamp>/`

Example mixed comparison summary produced from the synthetic fixtures:

```text
Step                  Wpn   Prof      Tgt                Acc Rej Hit Kill DHit DKill Leth Sig
glock_smoke           glock baseline  unarmored            4   1   3    1    3     1    1 tap:Y move:Y dummy:Y armor:N prot:N lethal:Y
mp5_smoke             mp5   cs_burst  unarmored            6   0   4    1    4     1    0 burst:Y move:Y dummy:Y armor:N prot:N lethal:N
mp5_vest_headprotected mp5  cs_mobile vest_headprotected   7   0   5    1    5     1    1 burst:Y move:Y dummy:Y armor:Y prot:Y lethal:Y
```

The mixed matrix runner tags every step with `weaponUnderTest`, weapon-profile metadata, target-profile metadata, `sessionTag`, `matrixName`, and `matrixStep`, then aggregates the resulting analyzer JSON into one mixed report set. It still structures manual comparison testing only. Without human firing, the generated artifacts prove launch, tagging, and aggregation, not gameplay behavior.

Tail the newest disposable weapon debug log in PowerShell:

```powershell
.\scripts\tail-weapon-log.ps1
```

Read the newest disposable weapon debug log once without following it:

```powershell
.\scripts\tail-weapon-log.ps1 -NoFollow
```

Run the non-interactive smoke test with the same experimental path:

```powershell
.\scripts\smoke-test.ps1 -Configuration Debug -AllowSteamCmdDownload -EnableExperimentalGlock
```

Telemetry stays fully quiet by default. `scripts\run-testbed.bat experimental-debug` enables:

- `sv_exp_debug_weaponlog 1`
- `sv_exp_debug_weaponlog_rejections 1`

Accepted Glock primary shots log one grep-friendly server-side line to both the server console and `testbed/logs/weapon-debug-<timestamp>.log`, for example:

```text
[weaponlog] type=accepted ts=2026-04-16T21:24:55.314 map=crossfire player="Player" entindex=1 userid=2 weapon=glock fire=primary experimental=1 tapfire=1 firstshot=1 spread=0.0000 base=0.0100 move_penalty=0.0000 speed2d=18.4 maxspeed=270.0 grounded=1 ducking=0 delta_prev=0.421 clip=16
```

Blocked tap-fire hold attempts log only when rejection logging is also enabled, for example:

```text
[weaponlog] type=rejected ts=2026-04-16T21:24:56.002 map=crossfire player="Player" entindex=1 userid=2 weapon=glock fire=primary reason=tapfire_hold_blocked speed2d=18.4 grounded=1 ducking=0
```

Each session also starts with a single header line:

```text
[weaponlog] type=session ts=2026-04-16T21:24:54.901 map=crossfire event=weapon_debug_session status=ready game=valve file="d:/dev/cpp/hl-server/testbed/logs/weapon-debug-20260416-212454.log" profile="cs_tight" tapfire=1 move_scale=1.0000 firstshot_enabled=1 recovery=0.350 base=0.0080 ground_move_penalty=0.0950 air_move_penalty=0.1400 duck_penalty_scale=0.7000 firstshot_speed=30.0 max_spread=0.1600 sv_exp_glock_primary_damage=10.0000 sv_exp_glock_primary_headshot_scale=4.0000 sv_exp_glock_primary_headshot_lethal=1
```

Hit and kill telemetry stay on the same single-line `key=value` shape, for example:

```text
[weaponlog] type=hit ts=2026-04-16T21:25:02.114 map=crossfire attacker="Player" attacker_entindex=1 attacker_userid=2 victim="Target Bravo" victim_entindex=3 victim_userid=5 weapon=glock fire=primary hitgroup=head hitgroup_id=1 headshot=1 experimental=1 profile="cs_tight" base_damage=10.0000 hitgroup_scale=4.0000 trace_damage=200.0000 applied_damage=100.0000 health_before=100.0 health_after=0.0 armor_before=50.0 armor_after=0.0 armor_damage=50.0 headshot_lethal_active=1 headshot_lethal_applied=1
```

```text
[weaponlog] type=kill ts=2026-04-16T21:25:02.114 map=crossfire attacker="Player" attacker_entindex=1 attacker_userid=2 victim="Target Bravo" victim_entindex=3 victim_userid=5 weapon=glock fire=primary hitgroup=head hitgroup_id=1 headshot=1 experimental=1 profile="cs_tight" trace_damage=200.0000 applied_damage=100.0000 health_before=100.0 health_after=0.0 armor_before=50.0 armor_after=0.0 headshot_lethal_active=1 headshot_lethal_applied=1
```

In this implementation, "lethal headshot evidence" means the server explicitly logged `headshot_lethal_applied=1` on a Glock hit or kill after the dedicated headshot-lethal path raised pre-armor damage for that specific hit. It is not inferred from a kill line alone, and it is not a claim of exact Counter-Strike parity or validated subjective feel.

## Analyze weapon telemetry

Analyze the newest disposable weapon log:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
```

The same analyzer can now be run from the native editor's `Telemetry` tab:

1. Use the editor to quick-export/apply a cfg or match pack.
2. Use the `Live Server` tab or HLDS console to run sandbox setup/reset if desired.
3. Shoot in the live client until the server writes `weapon-debug-*.log` lines.
4. Open `Telemetry`, click `Find Latest Log`, choose a weapon filter, then click `Analyze Latest`.
5. If the editor cannot find PowerShell, the analyzer script, or the selected log, it shows the failure and the manual fallback remains the PowerShell command below.

Manual weapon-filter examples:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon glock
.\scripts\analyze-weapon-log.ps1 -Path .\testbed\logs\weapon-debug-20260416-114837.log -Weapon mp5
```

Analyze a specific log file:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\testbed\logs\weapon-debug-20260416-114837.log
```

Export the summary JSON and per-event CSV to `testbed/logs/reports/`:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -ExportJson -ExportCsv
```

Analyze the newest disposable log and surface the active preset metadata:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
```

Filter the summary to 357 only:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon 357
```

Use the BAT alias from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-report
scripts\run-testbed.bat glock-report -ExportJson -ExportCsv
```

Assert that a manual run exercised the expected signals:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\scripts\fixtures\sample-weapon-debug.log -RequireAccepted -RequireRejections -RequireFirstShot -RequireMovePenalty -RequireCrouchMoveEvidence
```

Assert that a log contains hit, kill, headshot-kill, and explicit lethal-headshot evidence:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\scripts\fixtures\sample-weapon-debug.log -RequireHits -RequireKills -RequireHeadshotKills -RequireLethalHeadshotEvidence
```

Example summary:

```text
Weapon log analysis
  log path                 : D:\DEV\CPP\HL-Server\scripts\fixtures\sample-weapon-debug.log
  session                  : ts=2026-04-16T12:00:00.000 map=fixture_range game=valve status=ready
  profile                  : cs_tight
  tuning                   : base=0.0080 ground=0.0950 air=0.1400 duck=0.7000 firstshot_speed=30.0 max_spread=0.1600
  damage tuning            : damage=10.0000 headshot_scale=4.0000 headshot_lethal=1
  feature flags            : tapfire=1 move_scale=1.0000 firstshot=1 recovery=0.350
  accepted shots           : 5
  rejected shots           : 1
  hit events               : 2
  kill events              : 1
  headshot hits            : 1
  headshot kills           : 1
  lethal hs evidence       : 1
  first-shot accepted      : 2
  accepted move_penalty>0  : 3
  applied dmg min/avg/max  : 10.0000 / 55.0000 / 100.0000
  hitgroups                : head=1, chest=1
  accepted grounded        : 4
  accepted airborne        : 1
  accepted ducking         : 1
  spread min/avg/max       : 0.0000 / 0.0350 / 0.0700
```

The analyzer summarizes server-authoritative telemetry evidence only. It helps confirm that accepted shots, tap-fire hold rejections, movement penalties, recovery, crouch-move candidates, and shared-core damage evidence were logged, but it does not prove subjective stock-client feel or prediction quality.

## Pointing the scripts at existing installs

Copy `.env.example` to `.env` and fill in any paths you want to pin:

```powershell
Copy-Item .env.example .env
```

Supported variables:

- `HLDS_EXE` - full path to `hlds.exe`
- `HL_EXE` - full path to `hl.exe`
- `STEAMCMD_EXE` - full path to `steamcmd.exe`
- `HL_RUNTIME_TEMPLATE` - root folder to mirror into `testbed/runtime/`

You can also pass explicit script parameters instead of using `.env`.

Examples:

```powershell
.\scripts\doctor-testbed.ps1 -TemplateRoot D:\Games\Half-Life Dedicated Server
.\scripts\install-testbed.ps1 -Configuration Debug -HldsExe D:\Steam\steamapps\common\Half-Life\hlds.exe
.\scripts\run-server.ps1 -Configuration Debug -HlExe D:\Steam\steamapps\common\Half-Life\hl.exe -Detached
```

The scripts auto-detect common Steam install locations and the Steam install path from the Windows registry when possible. If auto-detection fails, they stop with a clear message instead of guessing unsafe locations.

## Build outputs

The CMake preset is the source of truth.

- Configure preset: `vs2022-win32`
- Build directory: `build/vs2022-win32/`
- Generated VS solution after configure: `build/vs2022-win32/hl_server.sln`
- DLL output:
  - `artifacts/Debug/hl.dll`
  - `artifacts/Release/hl.dll`

## Disposable testbed layout

- `testbed/runtime/` - disposable HLDS runtime mirror for no-client and dedicated flows
- `testbed/logs/` - `hlds-*.log` server launch logs and `weapon-debug-*.log` Glock telemetry logs
- `testbed/cache/` - downloaded SteamCMD and optional HLDS template cache
- `testbed/runtime/.hl-server-runtime.json` - runtime manifest recording which source root was mirrored
- `Half-Life\hlserver_testbed\.hl-server-live-mod.json` - manifest recording the managed live mod and client root

`install-testbed.ps1` now mirrors the selected source root into `testbed/runtime/`, validates the executable-side dependencies HLDS needs to start, repairs missing key sidecars when the source provides them, and then overwrites only:

- `testbed/runtime/valve/dlls/hl.dll`
- `testbed/runtime/valve/dlls/hl.pdb` when available

Key runtime-side entries checked by the doctor include:

- `hlds.exe`
- `hl.exe` when available from the selected source
- `steam_appid.txt`
- `SDL2.dll` or `SDL3.dll` when present in the selected source
- `steam_api.dll`
- `tier0.dll`
- `vstdlib.dll`
- `platform/`
- `bin/` when the selected source uses it

Live client-attached mode creates or refreshes a managed mod folder at `Half-Life\hlserver_testbed`, copies the built `hl.dll` there, and keeps stock content in the standard `Half-Life\valve` tree through `fallback_dir "valve"`.

## Script reference

- `scripts/bootstrap.ps1` verifies prerequisites, ensures the vendored SDK and docs are present, and writes `.env.example`.
- `scripts/configure.ps1` configures the VS2022 Win32 CMake preset.
- `scripts/build.ps1` builds Debug or Release and prints the resulting artifact paths.
- `scripts/install-testbed.ps1` prepares either the disposable runtime for no-client flows or the managed `hlserver_testbed` live mod folder for client-attached flows, then installs the built `hl.dll`.
- `scripts/doctor-testbed.ps1` diagnoses the selected runtime source, the disposable runtime, and common Windows startup blockers such as Steam initialization or missing executable-side DLL issues, and `-Repair` can refresh the runtime plus provision a dedicated HLDS cache.
- `scripts/run-server.ps1` launches HLDS against `testbed/runtime` on `-game valve` for no-client flows or from the stock Half-Life root on `-game hlserver_testbed` for live client-attached flows, defaults to `crossfire`, accepts `-EnableExperimentalGlock`, `-EnableExperimentalGlockDebug`, `-GlockProfile <name>`, and `-LabTargetProfile <name>` for server-only Glock tuning plus dummy-target selection, and still supports launch-time overrides through `-SetCvar @('name=value', ...)` or `-Cvars @{ name = 'value' }`.
- `scripts/play-live-session.ps1` is the shared live BAT compatibility helper. It preserves the old `play-glock-live.bat` and `play-mp5-live.bat` parameter surface, normalizes presets and target profiles, accepts `-CfgPath` or `-CfgProfile` for cfg-driven live sessions, and then delegates to the direct same-root launcher so every Explorer-friendly live BAT uses the same working `hlserver_testbed` path.
- `scripts/run-glock-test-session.ps1` reinstalls the disposable runtime, launches the experimental-debug Glock server, waits for readiness, optionally opens a tail window for the exact weapon log, optionally launches a stock client, prints the manual checklist with the connect address and log paths, accepts `-GlockProfile <name>` and `-LabTargetProfile <name>`, and can hand off to the analyzer with `-AnalyzeLatestOnExit`.
- `scripts/run-mp5-test-session.ps1` mirrors the same detached live-session flow for the MP5 experiment, including client launch, dummy-target support, checklist output, and optional analyzer hand-off.
- `scripts/list-glock-profiles.ps1` lists the checked-in versioned Glock presets from `configs/glock-presets/`.
- `scripts/list-glock-lab-targets.ps1` lists the checked-in lab target profiles from `configs/glock-lab-targets/`.
- `scripts/list-weapon-comparison-matrices.ps1` lists the checked-in mixed Glock-plus-MP5 comparison matrices from `configs/weapon-comparison-matrices/`.
- `scripts/run-weapon-comparison-matrix.ps1` dispatches mixed matrix steps to the existing Glock or MP5 session scripts, stamps normalized session metadata, exports per-step analyzer artifacts, and writes one consolidated mixed comparison report set under `testbed/logs/reports/weapon-comparison-matrices/`.
- `scripts/compare-weapon-reports.ps1` aggregates analyzer JSON from Glock-only or mixed Glock-plus-MP5 sessions, prints a concise comparison table, and can export normalized JSON, CSV, and Markdown summaries.
- `scripts/analyze-weapon-log.ps1` analyzes the newest or a specific `weapon-debug-*.log`, prints a concise evidence summary including session profile and target-profile metadata when present, supports `-Weapon glock`, `-Weapon mp5`, `-Weapon 357`, or `-Weapon all`, optionally exports JSON and CSV under `testbed/logs/reports/`, and can fail non-zero when required armored-dummy telemetry signals are missing.
- `scripts/show-latest-analysis.ps1` is the shared analyzer BAT helper that loads the newest disposable `weapon-debug-*.log`, prints the log and reports locations, and reruns the existing analyzer with `all`, `glock`, or `mp5` filtering.
- `scripts/play-glock-live.bat`, `scripts/play-mp5-live.bat`, and `scripts/play-live-test.bat` are Explorer-friendly live launchers for stock-client-attached same-root `hlserver_testbed` sessions.
- `scripts/show-latest-log-analysis.bat`, `scripts/show-latest-glock-analysis.bat`, and `scripts/show-latest-mp5-analysis.bat` are thin BAT entry points for the latest analyzer summaries.
- `scripts/run-testbed.bat` is a thin convenience wrapper over the PowerShell scripts that defaults to a detached Debug disposable launch, maps `experimental` and `experimental-debug` to the corresponding server switches, maps `glock-session` to the one-click manual session helper, maps `glock-report` to `scripts/analyze-weapon-log.ps1`, maps `glock-profiles`, `glock-lab-targets`, and `weapon-matrices` to the listing helpers, maps `glock-profile <name>` to a detached experimental-debug launch with that preset, maps `glock-lab-target <name>` to a one-player dummy session with that target profile, maps `weapon-matrix <name>` and `weapon-compare` to the mixed comparison helpers, adds `play-glock`, `play-mp5`, `play-menu`, `play-direct`, and `play-direct-cfg` aliases for the live BAT launchers, and keeps argument forwarding thin.
- `scripts/tail-weapon-log.ps1` finds the newest `testbed/logs/weapon-debug-*.log` file, or follows a specific log passed through `-Path`, prints a helpful message if none exists, and can either follow the log or dump it once with `-NoFollow`.
- `scripts/run-client.ps1` optionally launches a stock Half-Life client on the requested `-game` target and connects to `127.0.0.1`.
- `scripts/smoke-test.ps1` validates the end-to-end bootstrap non-interactively and can verify experimental cvar values from launch-time overrides.
- `scripts/clean-testbed.ps1` removes generated build and disposable runtime artifacts.

## What is intentionally not committed

- Steam files and registries
- Half-Life or HLDS binaries
- proprietary game assets
- generated build outputs
- disposable testbed runtime and logs
- local `.env` overrides

## Future direction

This repository is aimed at iterative server-side gameplay experiments that keep the stock Steam Half-Life client compatible. The current checked-in manual flows cover Glock, MP5, and 357, and future work can extend the same seam to additional weapons without turning the project into a custom client or full conversion.

See:

- `docs/upstream.md`
- `docs/testbed.md`
- `docs/testbed-runtime-troubleshooting.md`
- `docs/future-gameplay-hooks.md`
