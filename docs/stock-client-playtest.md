# Guided Stock-Client Playtest

This workflow guides a real stock Half-Life client playtest for the stable Improved HLDM package. It automates setup, command printing, optional RCON execution, analyzer output, and report collection. The actual shooting and subjective feel judgment remain manual.

## Quick Dry Run

Use this first to verify the instructions and report generation without launching anything:

```powershell
.\scripts\run-stock-client-playtest.ps1 -DryRun -NoPause -Weapon glock -Port 27016
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

The script prints one of:

- `Client connected: yes`
- `Client connected: no`
- `Client status: unknown`

`yes` usually comes from RCON `status` or fresh qconsole evidence. If the result is `no` or `unknown`, the script prints the exact stock-client launch command and the in-game `connect 127.0.0.1:<port>` fallback.

## RCON

For RCON-assisted setup, pass a password:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>" -Port 27015
```

When `-RconPassword` is provided, the script tests the running server with `status` and `exp_cfg_status`, then uses RCON for `hldm_skill_default` plus sandbox setup if HLDS accepts the password.

The script does not persist or write `rcon_password` into generated cfg/report files. Set `rcon_password` in the HLDS console or a private local cfg before relying on RCON. If RCON fails, the script prints the exact fallback commands to paste into the HLDS console. Passwords are not written into the summary as plain text.

Use `-RequireRcon` when you want the script to fail if RCON cannot be verified. Use `-Strict` when you also want the run to fail on unconfirmed client connection or missing fresh telemetry:

```powershell
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>" -RequireRcon
.\scripts\run-stock-client-playtest.ps1 -StartServer -RconPassword "<password>" -Strict
```

## Weapon Steps

Use `-Weapon` to test one weapon quickly:

```powershell
.\scripts\run-stock-client-playtest.ps1 -Weapon mp5 -RconPassword "<password>"
```

Default is `-Weapon all`. For each selected weapon, the script prepares or prints these commands:

```text
exp_sandbox_weapon <weapon>
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
exp_sandbox_verify
exp_sandbox_status
```

The manual checks are:

- Glock: careful single clicks, recovery wait, rapid clicks, cadence/pattern evidence.
- MP5: 3-5 shot burst, recovery pause, longer spray, movement spray, burst-growth/pattern evidence.
- 357: careful first shot, fast follow-ups, reset wait, cadence/pattern evidence.
- Shotgun: close shell, second shell after short delay, reset wait, pellet/pattern consistency.

The script pauses before analysis unless `-NoPause` or `-DryRun` is used.

Before each weapon step, the script snapshots the newest `weapon-debug-*.log`. After manual shooting, it checks whether a new log appeared, the log changed, or accepted/hit/kill events for that weapon appeared after the step start time. The summary prints:

```text
Fresh telemetry detected: yes
Fresh telemetry detected: no
```

If this says `no`, the analyzer output may be stale and should not be treated as proof of the just-performed shooting step.

## Report Contents

Each stock-client playtest report includes:

- `summary.txt`
- `package-check.txt`
- `per-weapon-summary.txt`
- `client-status.txt`
- `rcon-validation.txt`
- `commands.txt`
- `<weapon>-analysis.txt`
- latest weapon log path used by each analysis when available
- branch and commit information

Use the editor afterward:

- `Telemetry` tab to inspect the latest log.
- `Guided Tests` tab for editor-side setup.
- `Reports` tab to compare saved guided reports.

## PASS / FAIL Guidance

PASS means:

- package check prerequisites are present
- HLDS starts with `-game hlserver_testbed`
- stock client connects or a clear manual connect command is shown
- RCON works, or fallback commands are clear when not required
- fresh telemetry is detected for the weapon step in strict validation
- analyzer output is collected for the selected weapons

FAIL means:

- strict mode cannot confirm client connection
- `-RequireRcon` or `-Strict` cannot verify RCON
- strict mode cannot detect fresh telemetry for a weapon step
- analyzer output is missing even though weapon logs exist
- fallback commands are not visible

This workflow does not claim final balance or full CS parity. It only makes stock-client manual testing repeatable and easier to diagnose.
