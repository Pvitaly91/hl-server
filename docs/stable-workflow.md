# Stable Improved HLDM Workflow

This workflow packages the current improved-HLDM prototype into one repeatable path. It is still a stock-client-compatible server-side prototype, not a full Counter-Strike clone and not final gameplay balance.

## Branch

Use the packaged release workflow branch:

```powershell
git checkout release/improved-hldm-stable-package
```

This branch is based on `integration/stable-improved-hldm`, which remains the integrated runtime base for future feature work.

## One-Time Setup

From the repository root:

```powershell
.\scripts\setup-improved-hldm.ps1
```

The setup script:

- builds the server `hl.dll` in `Debug` by default
- installs the managed live mod under `<HalfLifeRoot>\hlserver_testbed\`
- builds and deploys `HlConfigEditorCpp.exe`
- syncs checked-in match packs into the live mod
- prepares live log and guided-test report directories
- verifies the key files exist

If your Half-Life install is not auto-detected, set `HL_EXE` in `.env` or pass it explicitly:

```powershell
.\scripts\setup-improved-hldm.ps1 -HlExe "D:\Steam\steamapps\common\Half-Life\hl.exe"
```

## Sanity Check

Run:

```powershell
.\scripts\check-improved-hldm.ps1
```

It checks:

- built server DLL
- live mod folder
- live `dlls\hl.dll`
- deployed editor executable
- synced match packs, including `hldm_skill_default`
- live log folder
- guided-test report and comparison folders

If it fails, rerun setup first.

## Launch

Start the recommended Improved HLDM live session:

```powershell
.\scripts\play-improved-hldm.bat
```

The helper delegates to the existing live launcher, uses `-game hlserver_testbed`, and launches with the checked-in `hldm_skill_default.cfg` as the recommended baseline. It also prints the key live commands:

```text
exp_matchcfg_apply hldm_skill_default
exp_sandbox_start
exp_sandbox_reset
```

For server-only verification:

```powershell
.\scripts\play-improved-hldm.ps1 -NoClient
```

## Open The Editor

Open the deployed native editor:

```powershell
.\scripts\open-hldm-editor.bat
```

Expected editor path:

```text
<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe
```

If it is missing, run setup again.

## Everyday Loop

1. Launch the live mod with `scripts\play-improved-hldm.bat`.
2. Open the editor with `scripts\open-hldm-editor.bat`.
3. In the editor, open `Browser`.
4. Choose a weapon preset or the `hldm_skill_default` match pack.
5. Use `Quick Export` or the `Live Server` tab to apply through RCON.
6. Use `Guided Tests` to select a weapon, target profile, target spot, and scenario.
7. Click `Start Test`; if RCON is unavailable, paste the shown commands into HLDS manually.
8. Shoot manually in the stock Half-Life client.
9. Click `Finish & Analyze`.
10. Save a report.
11. Use `Reports` to compare saved runs.

## Useful Paths

- Live mod: `<HalfLifeRoot>\hlserver_testbed\`
- Live DLL: `<HalfLifeRoot>\hlserver_testbed\dlls\hl.dll`
- Editor: `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`
- Checked-in match packs: `<repo>\configs\match-packs\`
- Live match packs: `<HalfLifeRoot>\hlserver_testbed\match_packs\`
- Repo logs: `<repo>\testbed\logs\`
- Live logs: `<HalfLifeRoot>\hlserver_testbed\logs\`
- Guided reports: `<repo>\testbed\logs\reports\guided-tests\`
- Comparison exports: `<repo>\testbed\logs\reports\guided-tests\comparisons\`

## RCON Fallback

The editor can send live commands only when HLDS has a matching `rcon_password`. If RCON is missing, wrong, or blocked:

1. Use the editor button that copies the fallback command sequence.
2. Paste the commands directly into the HLDS console.
3. Continue with `Finish & Analyze` after shooting.

This is expected; the editor is a workflow helper, not a replacement for the HLDS console.

## Log Discovery Fallback

If the editor cannot find logs:

1. Confirm the server has written weapon telemetry by running a sandbox or guided test.
2. Check `<repo>\testbed\logs\`.
3. Check `<HalfLifeRoot>\hlserver_testbed\logs\`.
4. Run the manual analyzer:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest
```

Weapon-specific examples:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon glock
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon mp5
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon 357
.\scripts\analyze-weapon-log.ps1 -Latest -Weapon shotgun
```

## What Is Ready

- Improved HLDM weapon-feel packs are packaged and easy to apply.
- The managed live mod and native editor deploy path are scripted.
- Sandbox and guided tests reduce console friction.
- Telemetry and report comparison are available from the editor.

## What Is Still Experimental

- Weapon balance still requires subjective playtesting.
- Round, team, buy, armor, and match systems remain optional experimental layers.
- No client-side recoil, HUD, inventory, or buy UI is included.
- The project improves HLDM gameplay while preserving stock Half-Life client compatibility; it is not a full CS clone.
