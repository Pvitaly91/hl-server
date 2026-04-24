# Stable Improved HLDM Playtest Checklist

This checklist is for manual playtesting of the stable Improved HLDM package. It verifies that the package is ready for human shooting tests; it does not prove final balance or full Counter-Strike parity.

## Setup Checklist

- Checkout `release/improved-hldm-playtest-pass`.
- Run `.\scripts\setup-improved-hldm.ps1`.
- Run `.\scripts\check-improved-hldm.ps1`.
- Confirm the live mod exists at `<HalfLifeRoot>\hlserver_testbed\`.
- Confirm `HlConfigEditorCpp.exe` exists under the live mod.
- Confirm `hldm_skill_default` exists under `<HalfLifeRoot>\hlserver_testbed\match_packs\`.

Ready enough:

- setup and check scripts pass
- editor opens
- match packs are listed

Needs fixing:

- missing `dlls\hl.dll`
- missing editor executable
- missing match packs
- check script has any `[FAIL]` line

## Launch Checklist

Start the recommended mode:

```powershell
.\scripts\play-improved-hldm.bat
```

Server-only variant:

```powershell
.\scripts\play-improved-hldm.ps1 -NoClient -Port 27016
```

In HLDS, the recommended baseline is:

```text
exp_matchcfg_apply hldm_skill_default
```

Ready enough:

- HLDS starts with `-game hlserver_testbed`
- `hldm_skill_default.cfg` is copied into the live mod root
- stock Half-Life client can connect

Needs fixing:

- HLDS does not start
- map mismatch or missing file errors
- stock client cannot connect

For a guided real-client run, use:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>"
```

For a fast single-weapon validation:

```powershell
.\scripts\run-stock-client-playtest.ps1 -Weapon glock -RconPassword "<password>"
```

For a hard validation run that should fail on missing client, missing RCON, or stale telemetry:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>" -Strict
```

Use a non-destructive command preview first:

```powershell
.\scripts\run-stock-client-playtest.ps1 -DryRun -NoPause -Weapon glock
```

The guided script writes `summary.txt`, `per-weapon-summary.txt`, RCON validation output, client-status evidence, and per-weapon analyzer reports under `testbed\logs\reports\stock-client-playtests\<timestamp>\`. It prints `Client connected: yes`, `Client connected: no`, or `Client status: unknown`, and prints `connect 127.0.0.1:<port>` when the client is not verified.

Ready enough:

- client status is `yes`, or the manual connect command is clear
- RCON says `yes`, or fallback commands are visible when RCON is not required
- each weapon step says `Fresh telemetry detected: yes` after manual shooting

Needs fixing:

- strict mode fails before manual shooting because client/RCON prerequisites are missing
- fresh telemetry is `no` after shooting
- per-weapon summary points to stale or missing analyzer output

## Target Dummy Checklist

Use the sandbox flow:

```text
exp_sandbox_start
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
exp_sandbox_status
exp_sandbox_verify
```

Ready enough:

- sandbox reset grants the selected weapon
- dummy respawns or repositions
- telemetry appears after shooting

Needs fixing:

- dummy fails to spawn without a clear reason
- selected target profile is ignored
- hit/kill lines are inconsistent

## Glock Test Checklist

Commands:

```text
exp_matchcfg_apply hldm_skill_default
exp_sandbox_start
exp_sandbox_weapon glock
exp_sandbox_pack hldm_skill_default
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

Manual test:

- stand still and fire one careful click
- wait for recovery
- fire another careful click
- fire several rapid clicks
- compare telemetry with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon glock`

Ready enough:

- careful single clicks are logged as accepted shots
- rapid spam shows cadence or pattern growth evidence
- the recommended feel does not require hard tapfire gating

Needs fixing:

- no accepted-shot telemetry
- no cadence/pattern evidence under the intended cfg
- client-visible behavior feels severely desynced

## MP5 Test Checklist

Commands:

```text
exp_matchcfg_apply hldm_skill_default
exp_sandbox_start
exp_sandbox_weapon mp5
exp_sandbox_pack hldm_skill_default
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

Manual test:

- fire a 3-5 shot burst
- pause for recovery
- fire a longer spray
- analyze with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon mp5`

Ready enough:

- short burst telemetry shows lower growth than long spray
- pattern mode and burst-growth evidence appear
- reset/recovery is visible after a pause

Needs fixing:

- long spray does not bloom
- pattern index never changes
- analyzer cannot parse MP5 evidence

## 357 Test Checklist

Commands:

```text
exp_matchcfg_apply hldm_skill_default
exp_sandbox_start
exp_sandbox_weapon 357
exp_sandbox_pack hldm_skill_default
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

Manual test:

- fire one careful first shot
- fire fast follow-ups
- wait for cadence/pattern reset
- fire again
- analyze with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon 357`

Ready enough:

- cadence and pattern fields are present
- fast follow-ups degrade accuracy
- reset is visible after enough idle time
- headshot damage remains readable

Needs fixing:

- cadence fields are absent
- pattern fields are absent when enabled
- protected-head dummy telemetry is inconsistent

## Shotgun Test Checklist

Commands:

```text
exp_matchcfg_apply hldm_skill_default
exp_sandbox_start
exp_sandbox_weapon shotgun
exp_sandbox_pack hldm_skill_default
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

Manual test:

- stand close to the dummy
- fire one shell
- wait for pattern reset
- fire another shell
- test at least one kill case if practical
- analyze with `.\scripts\analyze-weapon-log.ps1 -Latest -Weapon shotgun`

Ready enough:

- deterministic pellet pattern telemetry is present
- pellet hit count and damage summaries are internally consistent
- kill lines match actual health changes

Needs fixing:

- applied damage disagrees with health delta
- analyzer reports inconsistent dummy hits or kills
- pattern reset cannot be observed

## Analyzer And Report Checklist

Run:

```powershell
.\scripts\analyze-improved-hldm-latest.ps1
.\scripts\collect-improved-hldm-diagnostics.ps1
```

Ready enough:

- analyzer output is created for `all`, `glock`, `mp5`, `357`, and `shotgun`
- diagnostics folder contains git info, paths, pack list, latest logs, latest qconsole evidence when present, analyzer summaries, and the latest stock-client playtest summary when present
- editor `Telemetry`, `Guided Tests`, and `Reports` remain usable

Needs fixing:

- diagnostics cannot find any relevant paths
- analyzer output is missing despite existing logs
- report comparison cannot list saved reports

## Final Readiness Call

Ready for manual playtesting:

- package setup/check passes
- recommended launcher starts
- editor opens
- sandbox reset prepares at least one weapon and dummy setup
- analyzer and diagnostics produce files

Not ready:

- any package check fails
- launch path is blocked
- telemetry cannot be collected
- hit/damage consistency warnings appear in fresh logs

Subjective gameplay still requires human shooting in the stock client. Metrics help compare configs, but they do not decide final balance automatically.
