# C++ Config Editor

`tools\HlConfigEditorCpp\` is the standalone native Win32 editor for the existing `sv_exp_*` cvar surface. It saves editable projects as `.hlcfg.json` and exports GoldSrc-ready `.cfg` files for HLDS without introducing a second config system.

## Exact paths

Use these paths relative to the repository root:

- project folder: `<repo-root>\tools\HlConfigEditorCpp\`
- solution file: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.sln`
- project file: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj`
- `Debug|Win32` build output: `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe`
- `Release|Win32` build output: `<repo-root>\tools\HlConfigEditorCpp\bin\Release\Win32\HlConfigEditorCpp.exe`
- deployed live-mod EXE: `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`
- default live export folder: `<HalfLifeRoot>\hlserver_testbed\`
- staged fallback export folder: `<repo-root>\testbed\mods\hlserver_testbed\`
- self-test summary: `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`

The project file keeps the normal Visual Studio output under `bin\$(Configuration)\$(Platform)\`. Deployment into `hlserver_testbed` is an extra post-build convenience, not a replacement for the standard build output.

## Build in Visual Studio 2022

1. Open `tools\HlConfigEditorCpp\HlConfigEditorCpp.sln` in Visual Studio 2022.
2. Select `Debug` or `Release` and `Win32`.
3. Build the solution.
4. The build keeps the normal EXE in `bin\Debug\Win32\` or `bin\Release\Win32\`.
5. After the build succeeds, the project also tries to copy the EXE into `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
6. If the Half-Life root cannot be resolved from `HL_EXE`, `HLDS_EXE`, `.env`, or a common install path, the build prints a warning and still succeeds.

## Simple workflow

This is now the default path:

1. Build `HlConfigEditorCpp` in Visual Studio 2022.
2. Run `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
3. Edit values on the `General`, `Glock`, `MP5`, `357`, `Shotgun`, `Target Dummy`, `Round Mode`, `Team Round`, and `Buy & Equipment` tabs.
4. Save the editable project as `.hlcfg.json`.
5. Open the `Export` tab.
6. Confirm the resolved-path fields show the expected Half-Life root, live mod root, and quick-export target.
7. Leave the default live-mod target in place.
8. Use the suggested cfg filename, or change it to something like `my_glock.cfg`.
9. Click `Quick Export to Live Mod`.
10. If the target file already exists, confirm the overwrite.
11. On success, the editor verifies the file exists, updates the `Last Action` status, and shows the exact written cfg path plus the `exec` command.
12. Click `Copy exec command`.
13. In the running HLDS console, run `exp_cfg_apply my_glock.cfg` to refresh the cfg or `exp_lab_apply my_glock.cfg` to refresh the cfg and rebuild the dummy in one command.
14. `exec my_glock.cfg` remains available as a legacy fallback.

The editor project now covers more than weapon tuning:

- weapon tuning for Glock, MP5, 357, and shotgun
- target dummy settings
- round-mode rules
- team-round rules
- buy prototype rules
- armor, helmet, and first utility settings

The `General` page also shows a small "What This Config Will Affect" summary so you can see at a glance whether the current project enables round mode, team mode, buy mode, armor, helmet, and which main loadout it implies.

For the current Glock and MP5 feel pass, the editor now uses more direct tuning labels on the weapon tabs:

- `Shot growth (cadence bloom per shot)` or `Burst growth (cadence bloom per shot)`
- `Recovery seconds to clear cadence bloom`
- `Burst reset time`
- `Spray hold penalty scale`
- `Crouch stability scale`
- `Max spread clamp`
- `Deterministic pattern mode`
- `Horizontal pattern scale`
- `Vertical pattern scale`
- `Pattern reset time`
- `Pattern max index`

The recommended preset buttons and names for this pass are:

- Glock: `glock_cadence_soft`, `glock_cadence_tight`
- 357: `357_precision_duel`, `357_cadence_headshot`, `357_pattern_soft`, `357_pattern_tight`
- MP5: `mp5_controlled_burst`, `mp5_mobile_soft`, `mp5_pattern_burst`, `mp5_spray_harsh`
- Shotgun: `shotgun_pattern_soft`, `shotgun_pattern_tight`, `shotgun_close_quickkill`, `shotgun_precision_test`

The intent is to make it obvious that the editor is tuning cadence growth plus recovery, with an optional deterministic follow-up pattern layered on top, instead of forcing a hard server-only tap-fire gate as the only way to get a skillful pistol or SMG feel.

For the MP5 specifically, the current tab wording is meant to separate "good burst" from "bad spray":

- `Burst growth` controls how quickly repeated accepted shots bloom
- `Burst reset time` controls how long you need to pause before the burst state restarts cleanly
- `Spray hold penalty scale` controls how much extra punishment long held fire adds on top of normal burst growth
- `Pattern scale` and `Pattern reset time` control how learnable the early burst remains before the spray layer takes over

For the current pistol cadence and precision-pattern pass, the Glock and `357` tabs now also expose:

- `Cadence-sensitive mode`
- `Ideal shot cycle time`
- `Fast-click penalty`
- `Fast-click penalty scale`
- `Cadence reset time`
- `Hold/spam penalty`
- `Deterministic pattern mode`
- `Horizontal pattern scale`
- `Vertical pattern scale`
- `Pattern reset time`
- `Pattern max index`

The same `General` page now also carries optional match-pack metadata:

- pack name
- description
- tags

When the pack name is filled and the cfg is exported into the live mod root, the editor writes a sidecar JSON file into `<HalfLifeRoot>\hlserver_testbed\match_packs\` so the runtime can apply that config through `exp_matchcfg_apply <pack-name>`.

## Browser tab

The editor now includes a dedicated `Browser` tab for the preset and match-pack layer that already exists in the repository and live mod.

Weapon presets:

- listed from `configs\glock-presets\`, `configs\mp5-presets\`, `configs\357-presets\`, and `configs\shotgun-presets\`
- filterable by weapon family
- previewed with source path, description, and cvar count

Match packs:

- listed from `<HalfLifeRoot>\hlserver_testbed\match_packs\`
- previewed with description, tags, notes, and referenced cfg path
- loaded through the referenced cfg so the preview shows the real server cvar set, not only the JSON metadata
- includes the Improved HLDM packs such as `hldm_skill_default`, `hldm_precision_duel`, `hldm_mp5_burst`, `hldm_shotgun_control`, and `hldm_headshot_lab` once starter packs are synced into the live mod

Diff / merge behavior:

- `Selected Entry Preview` shows the entry metadata and the leading cvars
- `Current vs Incoming Diff` shows a simple per-cvar comparison of current and incoming values
- `Load Into Current Project` merges the selected entry into the open project
- if the current project is still the generic untitled/default project, loading a preset or pack adopts that entry name as the starting project and cfg name
- if the project already has unsaved changes, the editor warns before overwriting matching fields

Live-apply helper behavior:

- `Copy Apply Command` copies the exact live command for the currently loaded project
- `Quick Export + Copy Apply Command` writes the cfg into `<HalfLifeRoot>\hlserver_testbed\` and then copies the exact apply command
- when the loaded project includes match-pack metadata and exports into the live mod root, the copied command becomes `exp_matchcfg_apply <pack-name>`
- otherwise the copied command is `exp_cfg_apply <cfg-name>`

The editor now also includes a `Live Server` tab for a small direct-apply workflow over GoldSrc RCON. It is intentionally thin: it sends the same server console commands you could paste manually.

Live Server fields:

- host, default `127.0.0.1`
- port, default `27015`
- RCON password, entered by the user
- cfg filename
- match-pack name
- sandbox weapon, target profile, and target spot
- custom command text

Live Server actions:

- `Test Connection` sends `status`
- `Apply Current CFG` quick-exports the open project and sends `exp_cfg_apply <cfg>`
- `Apply Selected Match Pack` sends `exp_matchcfg_apply <pack>`
- `Sandbox Reset` sends `exp_sandbox_reset`
- `Apply CFG + Sandbox Reset` quick-exports the open project, applies it, then resets the sandbox
- `Apply Pack + Sandbox Reset` applies the selected pack, then resets the sandbox
- `Apply Sandbox Setup` sends the generated `exp_sandbox_*` command sequence
- `Send Custom Command` sends the command text currently shown in the live tab

If RCON is unavailable, the password is missing, or HLDS rejects the command, the editor shows the failure and copies the exact fallback commands for manual paste into the HLDS console. This is not full automatic server orchestration; it is a practical command sender over the existing cfg, match-pack, and sandbox command paths.

## Guided Tests tab

The `Guided Tests` tab is a thin workflow helper for manual weapon-feel testing. It does not shoot for you, validate balance, or add a new server mode; it builds and optionally sends the same sandbox, cfg, and match-pack commands you can already run from the HLDS console.

Guided test fields:

- `Weapon` selects `glock`, `mp5`, `357`, or `shotgun`.
- `Source` selects the setup source: the current editor cfg, an exported preset cfg name, a match pack, or a manual cfg filename.
- `Source value` is used for preset, match-pack, and manual cfg sources; for the current editor cfg path, the editor quick-exports first.
- `Target profile` selects `unarmored`, `vest`, or `vest_headprotected`.
- `Target spot` names the saved dummy spot, with `default` as the normal quick path.
- `Scenario` selects concise shooting instructions for single-shot precision, burst/spam control, movement, headshot/armor, or shotgun pellet checks.

`Start Test` records the current latest weapon log, builds the setup sequence, and sends it through the Live Server tab's RCON settings. Typical generated commands look like:

```text
exp_sandbox_start
exp_sandbox_weapon mp5
exp_sandbox_cfg editor_mp5_simple.cfg
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

For match-pack sources, the sequence starts with `exp_matchcfg_apply <pack>` and then uses `exp_sandbox_pack <pack>` before reset. If RCON is unavailable, the command sequence is copied for manual paste and shown in the tab.

After the user shoots in-game, `Finish & Analyze` locates the newest weapon log and runs the same analyzer used by the `Telemetry` tab with the selected weapon filter. The top of the output shows a compact evidence summary, followed by the full analyzer output. If precise log-session matching is not possible, the editor explicitly says it used the newest log.

`Save Test Report` writes a UTF-8 text report to:

```text
<repo-root>\testbed\logs\reports\guided-tests\
```

The report includes timestamp, weapon, source, target profile, target spot, scenario, generated command sequence, shooting instructions, analyzed log path, and analyzer output.

## Reports tab

The `Reports` tab lists guided weapon test reports saved under:

```text
<repo-root>\testbed\logs\reports\guided-tests\
```

New reports keep the readable `.txt` file and add a same-name `.json` sidecar. The sidecar stores normalized metadata and metrics so future comparisons do not need to scrape every value from the full analyzer text. The fields include timestamp, weapon, source type/value, target profile, target spot, scenario, analyzed log path, accepted shots, hits, kills, headshot hits/kills, pattern evidence, cadence evidence, burst-growth evidence, armor evidence, spread summary, damage summary, and consistency warnings.

Older text-only reports remain supported. The editor lists them, marks them as `text-only`, and infers whatever metadata and metrics it can from the plain report headers and analyzer output. Missing metrics show as `n/a` instead of crashing the UI.

Report actions:

- `Refresh` reloads the report folder.
- `Compare Selected` compares two or more selected reports in a tab-separated table.
- `Open Report` and `Open Log` use the Windows shell to open the selected report or source weapon log.
- `Copy Report Path`, `Copy Log Path`, and `Copy Comparison` keep the data easy to paste elsewhere.
- `Export Comparison` writes a text comparison report under:

```text
<repo-root>\testbed\logs\reports\guided-tests\comparisons\
```

The comparison is evidence for tuning, not an automatic balance verdict. Use the metrics to spot regressions or useful differences, then still judge stock-client feel by shooting manually.

## Telemetry tab

The editor now includes a `Telemetry` tab for the simple post-shooting analysis loop. It does not replace the PowerShell analyzer or make balance decisions; it only finds logs and runs the existing analyzer from inside the editor.

Telemetry fields and actions:

- `Latest log` shows the newest `weapon-debug-*.log` discovered by `Find Latest Log`.
- `Selected log` can be filled manually or through `Browse...`.
- `Weapon filter` passes `all`, `glock`, `mp5`, `357`, or `shotgun` to the analyzer.
- `Analyze Latest` finds the newest log and runs `scripts\analyze-weapon-log.ps1 -Path <log> -Weapon <filter>`.
- `Analyze Selected` runs the analyzer against the selected path.
- `Open Log Folder`, `Copy Log Path`, and `Copy Analysis` keep the manual workflow available.

The log search order is:

- `<HalfLifeRoot>\logs\weapon-debug-*.log`
- `<HalfLifeRoot>\hlserver_testbed\logs\weapon-debug-*.log`
- `<repo-root>\testbed\logs\weapon-debug-*.log`

When the editor is launched from `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`, it resolves the repo root by walking parents first, then checking `HL_SERVER_REPO_ROOT` / `HL_SERVER_REPO`, the live-mod marker `.hl-server-live-mod.txt`, and known local repo fallback paths. If the analyzer script or selected log cannot be resolved, the tab shows the failure instead of silently doing nothing.

Manual fallback commands remain:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon glock
.\scripts\analyze-weapon-log.ps1 -Path <log> -Weapon mp5
```

For repeatable weapon-lab sessions, either send the generated sandbox setup from the `Live Server` tab or paste the equivalent commands manually:

```text
exp_sandbox_start
exp_sandbox_weapon shotgun
exp_sandbox_pack hldm_shotgun_control
exp_sandbox_target unarmored
exp_sandbox_reset
```

If the browser copied a plain cfg command, use the cfg name with `exp_sandbox_cfg <cfg-name>` instead. The live tab reuses `exp_sandbox_*` on the server side; it does not introduce a separate sandbox config system.

For the recommended stock-client-compatible Improved HLDM workflow, select `hldm_skill_default` in the match-pack browser, preview the cvar diff, click `Load Into Current Project`, and use `Copy Apply Command` or `Quick Export + Copy Apply Command`. The copied live command should be:

```text
exp_matchcfg_apply hldm_skill_default
```

That pack is deathmatch-oriented: it improves Glock, MP5, 357, and shotgun weapon feel while keeping round mode, team round mode, buy mode, economy, and match progression off. The other `hldm_*` packs are focused variants for precision, MP5 bursts, shotgun control, and headshot/dummy verification.

In the simple path, the editor writes directly into:

```text
<HalfLifeRoot>\hlserver_testbed\my_glock.cfg
```

The copied command is:

```text
exec my_glock.cfg
```

The faster live-lab command path in the running server is:

```text
exp_cfg_apply my_glock.cfg
exp_target_respawn
```

or the one-step variant:

```text
exp_lab_apply my_glock.cfg
```

## Export actions

The `Export` tab now emphasizes the simple live-mod workflow first:

- `Quick Export to Live Mod` exports directly into `<HalfLifeRoot>\hlserver_testbed\`.
- The tab shows the resolved Half-Life root, live mod root, quick-export target, and running editor EXE before export.
- `Copy exec command` copies `exec <filename>.cfg` for root-level live-mod exports.
- `Export to chosen folder` is still available for advanced/custom destinations.
- `Copy launcher`, `Copy raw cfg`, `Open export folder`, and `Open live mod` all provide visible success or error feedback instead of failing silently.
- `Copy launcher` still works when the exported cfg resolves to a live-mod-relative path.
- Exported cfgs now include the current round, team-round, buy, armor, and helmet sections when those settings are present in the project, so one exported file can represent a full live match setup.
- Exported Glock and MP5 cfgs now also include the deterministic pattern cvars when pattern mode is enabled.
- Exported Glock and `357` cfgs now include the cadence cvars when cadence mode is enabled.
- Exported `357` cfgs now also include the deterministic pattern cvars when the pattern controls are enabled.
- Exported shotgun cfgs now also include the deterministic pellet-pattern cvars when the shotgun pattern controls are enabled.

The default cfg filename is derived from the current project/config name so the common path does not start from a generic placeholder. Example defaults include `editor_glock_simple.cfg`, `editor_mp5_simple.cfg`, `editor_357_test.cfg`, and `editor_shotgun_test.cfg`.

If the project includes match-pack metadata and the export target is the live mod root, the editor also writes:

```text
<HalfLifeRoot>\hlserver_testbed\match_packs\<pack-name>.json
```

That sidecar JSON references the exported cfg instead of inventing a second gameplay config format. The match-pack layer therefore stays compatible with `exec`, `exp_cfg_apply`, and the current cfg-driven live launch path.

## Advanced and backward-compatible workflow

The simplified path is now the default, but the older layout is still supported:

- You can still export into `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`.
- You can still use explicit legacy launcher paths such as `cfg_profiles\my_glock.cfg`.
- You can still export to another custom folder entirely.

Practical rules:

- If the cfg lives directly under `<HalfLifeRoot>\hlserver_testbed\`, the simple command is `exec my_glock.cfg`.
- If the cfg lives under `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`, the legacy command remains `exec cfg_profiles/my_glock.cfg`.
- If the cfg lives outside the live mod, launch it with `-CfgPath`.

## Launcher usage

Cfg-driven live play still uses the existing launcher surface:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile my_glock.cfg
scripts\run-testbed.bat play-direct-cfg my_glock.cfg
```

When `-CfgProfile` receives a bare filename with no folder separators, resolution now happens in this order:

1. `<HalfLifeRoot>\hlserver_testbed\<filename>.cfg`
2. `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\<filename>.cfg`

The launcher prints the exact resolved path before starting HLDS.

Legacy `cfg_profiles` commands still work:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_glock.cfg
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_mp5.cfg
```

External cfg files still work through `-CfgPath`:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgPath "D:\some-folder\editor_glock_simple.cfg"
```

## Recommended stable live-lab workflow

Use this as the preferred integrated path. Legacy demo and backward-compatible export paths remain available, but the direct live-mod workflow below is the recommended base for future tuning work:

1. Edit the values in `HlConfigEditorCpp`.
2. Quick-export the cfg directly into `<HalfLifeRoot>\hlserver_testbed\`.
3. Join the live server and stand in the firing lane you want to reuse.
4. Run `exp_target_mark default` once to save a persistent named spot for the current map.
5. In HLDS, run `exp_cfg_apply editor_glock_simple.cfg`, or use `exp_lab_apply editor_glock_simple.cfg` to apply the cfg and rebuild the dummy in one command.
6. For later sessions, run `exp_target_use_saved default`, then `exp_target_respawn`. Use `exp_target_tp_front` only when you want a temporary anchor-front placement instead of the persisted spot.
7. Switch target presets with `exp_target_profile unarmored`, `exp_target_profile vest`, or `exp_target_profile vest_headprotected`.
8. Inspect the current live state with `exp_cfg_status`, `exp_target_list`, and `exp_target_status`.
9. Test in-game and then review the weapon log or analyzer output.

For 357 specifically, the same editor path applies:

1. Open the `357` tab.
2. Choose a checked-in preset such as `default`, `precision_test`, or `headshot_test`.
3. Export `editor_357_test.cfg` to the live mod root.
4. In HLDS, run `exp_cfg_apply editor_357_test.cfg`.
5. Run `exp_target_use_saved default` and `exp_target_respawn`.
6. Review 357-only telemetry with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon 357`.

For shotgun specifically, the same editor path applies:

1. Open the `Shotgun` tab.
2. Choose a checked-in preset such as `shotgun_pattern_soft`, `shotgun_pattern_tight`, `shotgun_close_quickkill`, or `shotgun_precision_test`.
3. Export `editor_shotgun_test.cfg` to the live mod root.
4. In HLDS, run `exp_cfg_apply editor_shotgun_test.cfg`.
5. Run `exp_target_use_saved default` and `exp_target_respawn`.
6. Use the `Deterministic pellet pattern`, `Pellet spread mode`, `Pellet pattern X scale`, `Pellet pattern Y scale`, `Pattern reset time`, and `Per-shot spread growth` controls when you want a learnable shotgun cone instead of the older random-like spread.
7. Use `exp_target_profile unarmored` and `exp_target_tp_front` when you want deterministic close-range dummy checks.
8. Review shotgun-only telemetry with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon shotgun`.

One real editor session on `2026-04-23` verified the browser workflow end to end:

- preset browser listed Glock presets including `glock_cadence_tight`
- preset diff preview showed 21 changed cvars before load
- loading that preset into the current project worked
- `Quick Export + Copy Apply Command` then wrote `<HalfLifeRoot>\hlserver_testbed\glock_cadence_tight.cfg` and copied `exp_cfg_apply glock_cadence_tight.cfg`
- match-pack browser listed `team_mp5_buy`
- match-pack diff preview showed the incoming round/team/buy changes before load
- loading that pack into the current project worked
- `Copy Apply Command` then copied `exp_matchcfg_apply team_mp5_buy`

## Match-rule configs

The editor can now create full match configs in addition to weapon-only test configs.

Recommended tab layout:

- `General`
- `Glock`
- `MP5`
- `357`
- `Shotgun`
- `Target Dummy`
- `Round Mode`
- `Team Round`
- `Buy & Equipment`
- `Browser`
- `Live Server`
- `Guided Tests`
- `Reports`
- `Telemetry`
- `Export`

Round-mode coverage now includes:

- `sv_exp_round_mode`
- `sv_exp_round_freeze_time`
- `sv_exp_round_restart_delay`
- `sv_exp_round_start_health`
- `sv_exp_round_start_armor`
- `sv_exp_round_no_respawn`
- `sv_exp_round_friendlyfire`
- `sv_exp_round_weapon_profile`
- `sv_exp_round_loadout_mode`

Team-round coverage now includes:

- `sv_exp_team_round_mode`
- `sv_exp_team_round_teamplay`
- `sv_exp_team_round_spawn_mode`
- `sv_exp_team_round_team1_name`
- `sv_exp_team_round_team2_name`
- `sv_exp_team_round_team1_loadout`
- `sv_exp_team_round_team2_loadout`
- `sv_exp_team_round_team1_health`
- `sv_exp_team_round_team2_health`
- `sv_exp_team_round_team1_armor`
- `sv_exp_team_round_team2_armor`

Buy and equipment coverage now includes:

- `sv_exp_buy_mode`
- `sv_exp_buy_freeze_only`
- `sv_exp_buy_team_shared_catalog`
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

Built-in match templates now provide quick starting points:

- `duel_glock`
- `duel_357`
- `team_mp5`
- `team_shotgun`
- `armor_test`
- `buy_test`

## Match-pack aware export

The runtime now supports named match packs under `<HalfLifeRoot>\hlserver_testbed\match_packs\`. The editor does not try to become a launcher, but it can produce pack-ready exports cleanly:

1. Fill `Pack name` on the `General` page.
2. Optionally fill `Description` and `Tags`.
3. Configure weapon, target, round, team, buy, and armor settings as usual.
4. `Quick Export to Live Mod`.
5. The editor writes the cfg and, when the export target is the live mod root, a sidecar JSON pack file.
6. In the running server, apply it with `exp_matchcfg_apply <pack-name>`.

That means one editor project can now drive both:

- the plain cfg path: `exp_cfg_apply my_match.cfg` or `exec my_match.cfg`
- the named-pack path: `exp_matchcfg_apply my_pack`

The runtime also ships starter packs such as `duel_glock`, `duel_357`, `team_mp5_buy`, `team_shotgun`, `armor_test`, and `aim_lab`. See [docs/match-config-packs.md](/D:/DEV/CPP/HL-Server/docs/match-config-packs.md) for the runtime-side command flow and file layout.

Practical usage examples:

1. Duel config:
   Set `Round Mode` on, keep `Team Round` off, choose `357` or `glock` as the round loadout, save `editor_duel_357.hlcfg.json`, then export `editor_duel_357.cfg`.
2. Team config:
   Enable both `Round Mode` and `Team Round`, set team names and loadouts, save `editor_team_mp5.hlcfg.json`, then export `editor_team_mp5.cfg`.
3. Buy/armor config:
   Enable `Buy & Equipment`, set `sv_exp_buy_mode`, armor, and helmet options, save `editor_buy_armor.hlcfg.json`, then export `editor_buy_armor.cfg`.

Old `.hlcfg.json` projects still load. Missing newer match sections simply fall back to defaults instead of corrupting the project.

Subsystem provenance for this recommended path is recorded in [docs/stable-live-lab-state.md](/D:/DEV/CPP/HL-Server/docs/stable-live-lab-state.md).

Live lab console commands:

- `exp_cfg_apply <cfg_name_or_path>` applies a cfg from the live mod root or `cfg_profiles\` fallback.
- `exp_cfg_reload` re-executes the currently tracked cfg.
- `exp_cfg_status` prints the tracked cfg path, mode, last apply time, and current cfg metadata.
- `exp_lab_apply <cfg_name_or_path>` applies the cfg and respawns the current dummy with one summary.
- `exp_target_spawn` enables and spawns the standing dummy.
- `exp_target_clear` removes the standing dummy and disables automatic respawn.
- `exp_target_mark [name]` stores a persistent named target spot for the current map. Omitting the name writes `default`.
- `exp_target_unmark [name]` removes a named target spot for the current map. Omitting the name removes `default`.
- `exp_target_list` prints all named target spots for the current map, the active selection, and the on-disk file path.
- `exp_target_use_saved <name>` selects the active named target spot that `exp_target_respawn` should use next.
- `exp_target_respawn` rebuilds the dummy using the active saved spot first, then the `default` spot when no active spot is selected, then current anchor search, then last known good transform fallback.
- `exp_target_status` prints the current dummy profile, target-spots file/load state, active saved spot, saved spot names, last known good transform, anchor, last failure, and respawn viability.
- `exp_target_tp_front` moves or respawns the dummy in front of the current live player anchor.
- `exp_target_profile <name>` switches between `unarmored`, `vest`, and `vest_headprotected`, then refreshes the dummy when possible.

The live dummy now forces `mp_allowmonsters 1` before spawning, because the backing `monster_generic` entity is removed immediately on deathmatch maps when monster spawning is disabled.

Persistent target spots are stored under `<HalfLifeRoot>\hlserver_testbed\target_spots\<map>.json`. Each file is human-readable JSON and persists both the active spot name and the saved spot transforms for that map. See [docs/target-spots.md](/D:/DEV/CPP/HL-Server/docs/target-spots.md) for the exact command flow and file format.

## Shared tuning compatibility

The current server build routes Glock, MP5, 357, and shotgun through one shared server-side tuning core for common spread and damage calculations, but the editor surface is unchanged on purpose:

- existing `sv_exp_glock_*`, `sv_exp_mp5_*`, `sv_exp_357_*`, and `sv_exp_shotgun_*` names still map to the same exported cfg fields
- existing `.hlcfg.json` projects still export normal GoldSrc `.cfg` files
- existing exported cfgs such as `editor_glock_simple.cfg`, `editor_mp5_simple.cfg`, `editor_357_test.cfg`, and `editor_shotgun_test.cfg` still apply through `exp_cfg_apply`, `exp_lab_apply`, `-CfgProfile`, and `-CfgPath`

That means the editor workflow in this document remains the recommended path even after the Glock/MP5/357/shotgun shared-core server refactor.

## JSON vs CFG

- `.hlcfg.json` is the editable project file for reopening the same tuning session later.
- `.cfg` is the runtime server config that HLDS loads with `exec`.

Keep the `.hlcfg.json` file when you want to iterate later. Export a `.cfg` whenever you want HLDS or the live launcher to apply the current values.

## Self-test

Run the built-in self-test with:

```text
<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe --self-test
```

The self-test:

- saves example `.hlcfg.json` projects under `<repo-root>\artifacts\HlConfigEditorCppSelfTest\`
- exports example cfgs such as `editor_glock_simple.cfg`, `editor_mp5_simple.cfg`, `editor_357_test.cfg`, `editor_shotgun_test.cfg`, `editor_duel_357.cfg`, `editor_team_mp5.cfg`, and `editor_buy_armor.cfg`
- writes sidecar match-pack JSON files such as `duel_357.json`, `team_mp5.json`, and `buy_test.json` when the project includes pack metadata and the live mod root is available
- writes the summary file to `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`
- records the expected simple exec commands, for example `exec editor_glock_simple.cfg`, `exec editor_357_test.cfg`, `exec editor_team_mp5.cfg`, and `exec editor_buy_armor.cfg`

When the real Half-Life root is available, the self-test exports directly into `<HalfLifeRoot>\hlserver_testbed\`. If the live root is not available, it falls back to `<repo-root>\testbed\mods\hlserver_testbed\`.

One exported full-match config was also verified in a real live session on `2026-04-22`: `exp_cfg_apply editor_buy_armor.cfg` on a running server with a connected client set `sv_exp_round_mode`, `sv_exp_buy_mode`, `sv_exp_armor_mode`, and `sv_exp_helmet_mode` to `1`, confirming that the expanded editor export path now reaches the live rules layer instead of only weapon tuning.

## Live-mod refresh behavior

The managed `hlserver_testbed` refresh path now preserves the files that matter for the simplified editor workflow:

- root-level exported `*.cfg` files
- root-level `.hlcfg.json` files if you choose to save them there
- the deployed `HlConfigEditorCpp.exe`
- the deployed `HlConfigEditorCpp.pdb` when present
- the legacy `cfg_profiles\...` tree

That means a live-mod refresh no longer erases the default root-level cfg export path.

The editor deployment step also writes a live-mod marker when the folder contains only known editor/live-mod content, so `play-hlserver-testbed-direct.bat -CfgProfile <name>.cfg` can later refresh the same folder instead of refusing it as unmanaged.

## Troubleshooting

- If `Quick Export to Live Mod` looks idle, the current editor should now always respond with either a success dialog or a clear error dialog and an updated `Last Action` status field.
- The default quick-export file should appear directly in `<HalfLifeRoot>\hlserver_testbed\`, for example `D:\Steam\steamapps\common\Half-Life\hlserver_testbed\editor_gui_simple.cfg`.
- To verify you are running the deployed build, launch `D:\Steam\steamapps\common\Half-Life\hlserver_testbed\HlConfigEditorCpp.exe` and compare its timestamp with `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe`.
- If Visual Studio reports that deployment failed because the destination EXE is in use, close the running deployed editor from the mod root and rebuild.
