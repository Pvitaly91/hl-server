# Guided Stock-Client Playtest

This workflow guides a real stock Half-Life client playtest for the stable Improved HLDM package. It automates setup, command printing, optional RCON execution, analyzer output, and report collection. The actual shooting and subjective feel judgment remain manual.

## Quick Dry Run

Use this first to verify the instructions and report generation without launching anything:

```powershell
.\scripts\run-stock-client-playtest.ps1 -DryRun -NoPause -Port 27016
```

The report is written under:

```text
testbed\logs\reports\stock-client-playtests\<timestamp>\
```

## Real Client Run

Run setup and package checks first:

```powershell
.\scripts\setup-improved-hldm.ps1
.\scripts\check-improved-hldm.ps1
```

Then start the guided playtest:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -Port 27015
```

The script launches the managed `hlserver_testbed` session and stock client unless you pass `-NoClient`.

If the client does not auto-connect, open the client console and run:

```text
connect 127.0.0.1:27015
```

## RCON

For RCON-assisted setup, pass a password:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>" -Port 27015
```

When `-RconPassword` is provided, the script tests the running server with `status` and uses RCON for `hldm_skill_default` plus sandbox setup if HLDS accepts the password.

The script does not persist or write `rcon_password` into generated cfg/report files. Set `rcon_password` in the HLDS console or a private local cfg before relying on RCON. If RCON fails, the script prints the exact fallback commands to paste into the HLDS console. Passwords are not written into the summary as plain text.

## Weapon Steps

For each weapon, the script prepares or prints these commands:

```text
exp_sandbox_weapon <weapon>
exp_sandbox_pack hldm_skill_default
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
exp_sandbox_status
exp_sandbox_verify
```

The manual checks are:

- Glock: careful single clicks, recovery wait, rapid clicks, cadence/pattern evidence.
- MP5: 3-5 shot burst, recovery pause, longer spray, movement spray, burst-growth/pattern evidence.
- 357: careful first shot, fast follow-ups, reset wait, cadence/pattern evidence.
- Shotgun: close shell, second shell after short delay, reset wait, pellet/pattern consistency.

The script pauses before analysis unless `-NoPause` or `-DryRun` is used.

## Report Contents

Each stock-client playtest report includes:

- `summary.txt`
- `package-check.txt`
- `<weapon>-analysis.txt`
- optional `<weapon>-rcon.txt`
- copied latest weapon log after each weapon step when a log exists
- branch and commit information

Use the editor afterward:

- `Telemetry` tab to inspect the latest log.
- `Guided Tests` tab for editor-side setup.
- `Reports` tab to compare saved guided reports.

## PASS / FAIL Guidance

PASS means:

- package check passes
- HLDS starts with `-game hlserver_testbed`
- stock client connects or a clear manual connect command is shown
- RCON works or fallback commands are clear
- analyzer output is collected for the selected weapons

FAIL means:

- package check fails
- HLDS does not start
- client cannot connect
- RCON silently fails without fallback
- analyzer output is missing even though weapon logs exist

This workflow does not claim final balance or full CS parity. It only makes stock-client manual testing repeatable and easier to diagnose.
