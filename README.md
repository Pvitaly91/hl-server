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
- Server-only experimental Glock and MP5 paths for manual stock-client-compatible gameplay iteration without changing the stock client DLL.

It is not yet a gameplay conversion and it does not ship any proprietary game assets, Steam files, or HLDS binaries.

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
4. On the `Export` tab, click `Quick Export to Live Mod`.
5. The editor writes `<HalfLifeRoot>\hlserver_testbed\<project-or-config-name>.cfg`.
6. Click `Copy exec command` and use the copied command in HLDS, for example `exec my_glock.cfg`.

How to use the editor:

1. Edit values on the `General`, `Glock`, `MP5`, and `Target Dummy` tabs.
2. Use `File -> Save` or `File -> Save As` to store the editable project as `.hlcfg.json`.
3. Open the `Export` tab. The default filename follows the project or config name, for example `editor_glock_simple.cfg`.
4. `Quick Export to Live Mod` is the primary action. It exports directly into `<HalfLifeRoot>\hlserver_testbed\` and prompts before overwriting an existing file.
5. `Copy exec command` now defaults to `exec my_glock.cfg` when the cfg is exported to the live mod root.
6. `Export to chosen folder` is still available for advanced/custom locations.
7. `Copy launcher` still works for cfgs inside the live mod, including legacy `cfg_profiles\...` exports.

JSON vs CFG:

- `.hlcfg.json` is the editor project file. Keep it if you want to reopen the same tuning session later and continue editing.
- `.cfg` is the GoldSrc server config file. HLDS loads this file with `exec`, and the live launchers use it through `-CfgProfile` or `-CfgPath`.

Export behavior:

- When the editor can resolve the real Half-Life root from `HL_EXE` or `HLDS_EXE`, it defaults the export folder to `<HalfLifeRoot>\hlserver_testbed\`.
- If the live root is not available, it falls back to `<repo-root>\testbed\mods\hlserver_testbed\`.
- If you choose a custom folder inside `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`, the legacy `cfg_profiles\...` launcher and `exec cfg_profiles/...` workflow remains supported.
- The editor does not hot-apply changes to a running server. Export the `.cfg`, then load it manually in HLDS or start a cfg-driven live session with the launcher commands below.

Built-in self-test:

- Run `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe --self-test`.
- When the editor resolves the repository root, it writes the summary to `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`.
- When the live Half-Life root is available, the self-test writes example Glock and MP5 `.hlcfg.json` projects plus exported `.cfg` files such as `<HalfLifeRoot>\hlserver_testbed\editor_glock_simple.cfg` and `<HalfLifeRoot>\hlserver_testbed\editor_mp5_simple.cfg`.
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

Launch live with a Glock cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile my_glock.cfg
```

Launch live with an MP5 cfg already exported into the active live mod root:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile my_mp5.cfg
```

Use the shorthand cfg alias when you only want to supply a profile plus extra launcher flags:

```bat
scripts\run-testbed.bat play-direct-cfg my_glock.cfg
scripts\run-testbed.bat play-direct-cfg my_mp5.cfg
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
exec my_glock.cfg
changelevel crossfire

exec my_mp5.cfg
changelevel crossfire

exec cfg_profiles/my_legacy_glock.cfg
changelevel crossfire
```

Cfg-driven observability:

- the launcher prints whether the session is `demo` or `cfg-driven`
- cfg-driven sessions print the requested cfg, the active live-mod path, and the `exec` profile
- launch metadata under `testbed\logs\hlds-*-launch.txt` records `Cfg mode`, `Cfg profile`, `Cfg source`, and `Cfg active`
- root-level exported `*.cfg` files, the deployed `HlConfigEditorCpp.exe`, and legacy `cfg_profiles\...` files are preserved across same-root live-mod refreshes so the simplified workflow remains usable

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
exp_target_respawn
exp_target_status
exp_target_tp_front
exp_target_profile unarmored
exp_target_profile vest
exp_target_profile vest_headprotected
```

What the commands do:

- `exp_target_spawn` enables the target and spawns it now if a live player anchor or saved target position is available.
- `exp_target_clear` removes the current target and disables automatic target spawning until you enable it again.
- `exp_target_respawn` recreates the target immediately, preferring the current player-facing position when a live anchor exists.
- `exp_target_status` prints the current target state, profile, placement settings, saved target spot, and active anchor to the server console.
- `exp_target_tp_front` moves the current target, or spawns a fresh one, into a predictable position in front of the live player.
- `exp_target_profile <name>` switches the built-in live profile and refreshes the target immediately when practical.

Built-in live profiles:

- `unarmored`
  Baseline unarmored target.
- `vest`
  Armored torso target.
- `vest_headprotected`
  Armored target with protected head enabled.

Practical live flow:

1. Launch `scripts\play-target-test-live.bat`.
2. Join the live session on `-game hlserver_testbed`.
3. Run `exp_target_status` once to confirm the anchor, current profile, and saved target spot.
4. Use `exp_target_tp_front` if you want the standing target reset directly in front of you.
5. Use `exp_target_profile unarmored`, `vest`, or `vest_headprotected` before comparing body shots, headshots, and protected-head behavior.
6. Use `exp_target_respawn` after a kill when you want a clean full-health target immediately.

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

## Analyze Glock telemetry

Analyze the newest disposable weapon log:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
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

The analyzer summarizes server-authoritative telemetry evidence only. It helps confirm that accepted shots, tap-fire hold rejections, movement penalties, recovery, and crouch-move candidates were logged, but it does not prove subjective stock-client feel or prediction quality.

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
- `scripts/analyze-weapon-log.ps1` analyzes the newest or a specific `weapon-debug-*.log`, prints a concise evidence summary including session profile and target-profile metadata when present, optionally exports JSON and CSV under `testbed/logs/reports/`, and can fail non-zero when required armored-dummy telemetry signals are missing.
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

This repository is aimed at iterative server-side gameplay experiments that keep the stock Steam Half-Life client compatible. The current checked-in manual flows cover Glock and MP5, and future work can extend the same seam to additional weapons without turning the project into a custom client or full conversion.

See:

- `docs/upstream.md`
- `docs/testbed.md`
- `docs/testbed-runtime-troubleshooting.md`
- `docs/future-gameplay-hooks.md`
