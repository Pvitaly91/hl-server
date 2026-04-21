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
3. Edit values on the `General`, `Glock`, `MP5`, and `Target Dummy` tabs.
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

The default cfg filename is derived from the current project/config name so the common path does not start from a generic placeholder. Example defaults include `editor_glock_simple.cfg` and `editor_mp5_simple.cfg`.

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
- exports example cfgs such as `editor_glock_simple.cfg` and `editor_mp5_simple.cfg`
- writes the summary file to `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`
- records the expected simple exec commands, for example `exec editor_glock_simple.cfg`

When the real Half-Life root is available, the self-test exports directly into `<HalfLifeRoot>\hlserver_testbed\`. If the live root is not available, it falls back to `<repo-root>\testbed\mods\hlserver_testbed\`.

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
