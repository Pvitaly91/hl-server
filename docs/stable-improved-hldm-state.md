# Stable Improved HLDM State

`integration/stable-improved-hldm` is the recommended base branch for future work after the April 24, 2026 integration pass.

This branch is intentionally a stabilization point. It does not add a new gameplay mechanic, rename cvars, or redesign the editor. It collects the current verified server-side weapon-feel work, telemetry fidelity fixes, Improved HLDM packs, weapon sandbox mode, and native editor workflow into one branch.

## Integration Strategy

The integration branch starts from `feature/editor-test-report-history` at `e7bac3c7c28557306da6bdd52c5805008aee7153`.

That branch was chosen because local ancestry showed it is a linear descendant of the current server and editor work listed below. No additional merge or cherry-pick was required for the known subsystem heads; the integration pass only adds this stable-state documentation and updates the README recommended workflow.

## Source Subsystems

| Subsystem | Source branch | Source commit |
| --- | --- | --- |
| Weapon feel core, including Glock/MP5 deterministic patterns, Glock/357 cadence, MP5 recoil-growth polish, 357 precision pattern, and shotgun pattern lineage | `feature/357-pattern-precision` plus ancestors | `123a3acac7ec026caa3327dd9f4c57d66324c86f` |
| MP5 recoil-growth polish | `feature/mp5-recoil-growth-polish` | `02840b74e76848bed4c057fa59b60901a8cf25b0` |
| Shotgun deterministic pellet mode | `feature/shotgun-deterministic-pellets` | `9d1a8badec0a44b4754cca686e6903ea18c6ea01` |
| Shotgun telemetry fidelity | `fix/shotgun-telemetry-fidelity` | `47fcf01dd07463b636a826783a5f288eeefba09b` |
| Dummy armor/protected-head fidelity | `fix/dummy-armor-fidelity` | `278ae163caf1fab6e5e16e7c8e64bb10775c92f1` |
| Improved HLDM packs | `feature/improved-hldm-packs` | `50ba174eec96571e959cdd6f124a15019163a7fd` |
| Weapon sandbox mode | `feature/weapon-sandbox-mode` | `431c807f9b8c8f840a02f0975d3295ed9a851392` |
| Editor live apply / RCON helper | `feature/editor-live-apply-helper` | `ad6554e63e69a878bde53d634688747c600e7f26` |
| Editor telemetry analysis | `feature/editor-telemetry-analysis` | `2b19258a304d4aedd1fddde1eee3e82cf57c8c48` |
| Guided weapon tests | `feature/editor-guided-weapon-tests` | `15a2cea1c3e9b4cb55e4ceda1b1e2df78205830d` |
| Guided report history / compare | `feature/editor-test-report-history` | `e7bac3c7c28557306da6bdd52c5805008aee7153` |

## Included Runtime State

- Direct live mod workflow through `<HalfLifeRoot>\hlserver_testbed\`.
- CFG-driven server commands for applying configs and lab state.
- Shared weapon tuning core for Glock, MP5, 357, and shotgun.
- Deterministic/pattern/cadence weapon-feel controls for Glock, MP5, 357, and shotgun.
- MP5 burst-vs-spray tuning support.
- 357 precision/pattern tuning support.
- Shotgun deterministic pellet pattern and corrected shotgun hit telemetry.
- Real-player, fake-player, and dummy hit/headshot/helmet/armor telemetry paths.
- Dummy armor and protected-head telemetry consistency fix.
- Persistent target spots and target dummy workflow.
- Improved HLDM match packs, including `hldm_skill_default`.
- Weapon sandbox mode for repeatable weapon/dummy setup.

## Included Editor State

- Native C++ config editor.
- Quick export into the live mod root.
- Preset browser and match-pack browser.
- Live Server tab with GoldSrc RCON command sender and manual fallback.
- Telemetry tab that runs the existing PowerShell analyzer.
- Guided Tests tab for repeatable manual shooting tests.
- Guided test report saving with `.txt` reports and `.json` sidecars.
- Reports tab for report history, comparison, open/copy helpers, and comparison export.

## Recommended Everyday Workflow

1. Build the server DLL with `.\scripts\build.ps1 -Configuration Debug`.
2. Build `tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj` in `Release|Win32` or `Debug|Win32`.
3. Launch the live mod through the existing BAT/PowerShell launcher or `.\scripts\run-server.ps1`.
4. Open `<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe`.
5. Use the preset browser or match-pack browser; `hldm_skill_default` is the default Improved HLDM starting point.
6. Quick-export a cfg or apply a match pack through the Live Server tab.
7. Use Weapon Sandbox or Guided Tests to set weapon, cfg/pack, target profile, and saved spot.
8. Shoot manually in the stock Half-Life client.
9. Use Telemetry or Guided Tests `Finish & Analyze` to inspect the latest weapon log.
10. Save a guided test report.
11. Use Reports to compare runs across configs, presets, and match packs.

## Still Manual

- Subjective weapon feel remains manual. The editor can guide setup and summarize telemetry, but it does not replace human shooting.
- Gameplay balance is not final.
- Stock-client prediction and visual recoil remain server-side approximations because no custom client DLL is shipped.
- Round, team, buy, armor, and match systems remain available as experimental layers, but the recommended Improved HLDM path stays deathmatch-oriented by default.
