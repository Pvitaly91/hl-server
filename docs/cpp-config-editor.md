# C++ Config Editor

This repository includes a standalone Win32 config editor for tuning the experimental weapon and target-dummy cvars without hand-editing GoldSrc cfg files. The editor saves editable project files as `.hlcfg.json` and exports server-ready `.cfg` files that HLDS can load with `exec`.

## Exact paths

Use these paths relative to the repository root:

- project folder: `<repo-root>\tools\HlConfigEditorCpp\`
- solution file: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.sln`
- project file: `<repo-root>\tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj`
- `Debug|Win32` executable: `<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe`
- `Release|Win32` executable: `<repo-root>\tools\HlConfigEditorCpp\bin\Release\Win32\HlConfigEditorCpp.exe`
- self-test summary file: `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`
- self-test export folder: `<repo-root>\artifacts\HlConfigEditorCppSelfTest\hlserver_testbed\cfg_profiles\`
- recommended live export folder: `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`
- staged fallback export folder: `<repo-root>\testbed\mods\hlserver_testbed\cfg_profiles\`

The project file sets `OutDir` to `$(ProjectDir)bin\$(Configuration)\$(Platform)\`, so the two executable paths above are the exact build outputs for the only solution configurations: `Debug|Win32` and `Release|Win32`.

## Open and build in Visual Studio 2022

1. Open `tools\HlConfigEditorCpp\HlConfigEditorCpp.sln` in Visual Studio 2022.
2. If you only want the one project, `tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj` also opens directly in Visual Studio 2022.
3. In the Visual Studio toolbar, select either `Debug` or `Release`, and select `Win32` as the platform.
4. Build the solution with `Build -> Build Solution`.
5. Run the editor with `Debug -> Start Without Debugging` or by launching the built executable directly from `bin\Debug\Win32\` or `bin\Release\Win32\`.

## Run the editor

After the build completes, start one of these executables:

```text
<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe
<repo-root>\tools\HlConfigEditorCpp\bin\Release\Win32\HlConfigEditorCpp.exe
```

The editor exposes the current cvar surface through these tabs:

- `General`
- `Glock`
- `MP5`
- `Target Dummy`
- `Export`

## JSON vs CFG

- `.hlcfg.json` is the editor project file. Save this when you want to reopen the same tuning setup and keep editing it later.
- `.cfg` is the GoldSrc server config file. HLDS loads this with `exec`, and the live launchers consume it through `-CfgProfile` or `-CfgPath`.

## Save and export workflow

1. Change values on the `General`, `Glock`, `MP5`, and `Target Dummy` tabs.
2. Save the editable project with `File -> Save` or `File -> Save As`.
3. Give the editor project a `.hlcfg.json` name such as `my_glock.hlcfg.json` or `my_mp5.hlcfg.json`.
4. Open the `Export` tab.
5. Set the export folder to `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\` when you want the cfg to be immediately usable by the live same-root launcher.
6. If the editor cannot resolve the real Half-Life root, export to `<repo-root>\testbed\mods\hlserver_testbed\cfg_profiles\` or another folder you control, then use `-CfgPath`.
7. Set the exported file name to a `.cfg` file such as `my_glock.cfg` or `my_mp5.cfg`.
8. Click `Export CFG`.
9. Use `Copy exec` to copy the manual HLDS command.
10. Use `Copy launcher` when the export target resolves inside `<HalfLifeRoot>\hlserver_testbed\`.

## Example workflow: Glock

1. Build and launch `HlConfigEditorCpp.exe`.
2. On `General`, set your session-wide values such as the session tag and weapon logging options.
3. On `Glock`, apply the preset or manual values you want to test.
4. On `Target Dummy`, choose the dummy profile you want to use during live play.
5. Save the editable project as `my_glock.hlcfg.json`.
6. On `Export`, set the folder to `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`.
7. Set the export file name to `my_glock.cfg`.
8. Click `Export CFG`.
9. Start a cfg-driven live session:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_glock.cfg
```

10. Or use the BAT dispatcher alias:

```bat
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_glock.cfg
```

11. If you are already at an HLDS console instead of launching a new session, load the cfg manually and restart the map:

```text
exec cfg_profiles/my_glock.cfg
changelevel crossfire
```

## Example workflow: MP5

1. Build and launch `HlConfigEditorCpp.exe`.
2. On `General`, set the session tag and any shared debug/logging values.
3. On `MP5`, apply the preset or manual values you want to test.
4. On `Target Dummy`, choose the target profile that matches the scenario you want to compare against.
5. Save the editable project as `my_mp5.hlcfg.json`.
6. On `Export`, set the folder to `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`.
7. Set the export file name to `my_mp5.cfg`.
8. Click `Export CFG`.
9. Start a cfg-driven live session:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_mp5.cfg
```

10. Or use the BAT dispatcher alias:

```bat
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_mp5.cfg
```

11. Manual HLDS console fallback:

```text
exec cfg_profiles/my_mp5.cfg
changelevel crossfire
```

## Launching a cfg from outside the live mod

If you exported to a repo folder or another absolute path instead of `<HalfLifeRoot>\hlserver_testbed\cfg_profiles\`, use `-CfgPath`:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgPath "<repo-root>\artifacts\HlConfigEditorCppSelfTest\hlserver_testbed\cfg_profiles\editor_glock_test.cfg"
```

The live launcher copies that cfg into the active live mod and then executes it as `cfg_profiles/editor_glock_test.cfg`.

## How the cfg is applied in game

The cfg-driven live launcher supports these practical entry points:

```bat
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_glock.cfg
scripts\play-hlserver-testbed-direct.bat -CfgProfile cfg_profiles\my_mp5.cfg
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_glock.cfg
scripts\run-testbed.bat play-direct-cfg cfg_profiles\my_mp5.cfg
```

Manual console fallback inside HLDS:

```text
exec cfg_profiles/my_glock.cfg
changelevel crossfire

exec cfg_profiles/my_mp5.cfg
changelevel crossfire
```

## Self-test

The editor includes a built-in self-test path:

```text
<repo-root>\tools\HlConfigEditorCpp\bin\Debug\Win32\HlConfigEditorCpp.exe --self-test
```

When the executable can resolve the repository root, it writes:

- `<repo-root>\artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt`
- `<repo-root>\artifacts\HlConfigEditorCppSelfTest\editor_glock_test.hlcfg.json`
- `<repo-root>\artifacts\HlConfigEditorCppSelfTest\editor_mp5_test.hlcfg.json`
- `<repo-root>\artifacts\HlConfigEditorCppSelfTest\hlserver_testbed\cfg_profiles\editor_glock_test.cfg`
- `<repo-root>\artifacts\HlConfigEditorCppSelfTest\hlserver_testbed\cfg_profiles\editor_mp5_test.cfg`

The summary file records the exact exported cfg paths plus the `exec cfg_profiles/...` commands that should work in HLDS.

## Verified in this checkout

The following checks were run against this repository state:

- `HlConfigEditorCpp.sln` built successfully with MSBuild from Visual Studio 2022 for `Debug|Win32`
- `HlConfigEditorCpp.sln` built successfully with MSBuild from Visual Studio 2022 for `Release|Win32`
- the documented `Debug|Win32` executable launched successfully
- the documented self-test output path existed after running the editor self-test
- the self-test generated both Glock and MP5 `.hlcfg.json` project files and exported `.cfg` files under `artifacts\HlConfigEditorCppSelfTest\`
- cfg-driven live launch succeeded with `scripts\play-hlserver-testbed-direct.bat -CfgPath ...\editor_glock_test.cfg -NoClient`
- cfg-driven live launch also succeeded with `scripts\run-testbed.bat play-direct-cfg cfg_profiles\editor_glock_test.cfg -NoClient`
