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
6. Leave the default live-mod target in place.
7. Use the suggested cfg filename, or change it to something like `my_glock.cfg`.
8. Click `Quick Export to Live Mod`.
9. If the target file already exists, confirm the overwrite.
10. Click `Copy exec command`.
11. In HLDS, run `exec my_glock.cfg`.

In the simple path, the editor writes directly into:

```text
<HalfLifeRoot>\hlserver_testbed\my_glock.cfg
```

The copied command is:

```text
exec my_glock.cfg
```

## Export actions

The `Export` tab now emphasizes the simple live-mod workflow first:

- `Quick Export to Live Mod` exports directly into `<HalfLifeRoot>\hlserver_testbed\`.
- `Copy exec command` copies `exec <filename>.cfg` for root-level live-mod exports.
- `Export to chosen folder` is still available for advanced/custom destinations.
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
