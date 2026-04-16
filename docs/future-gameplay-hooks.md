# Future gameplay hooks

## Current seams

`src/future_gameplay_hooks.cpp` remains the main server-only seam for future gameplay work.

- `third_party/valve-halflife-sdk/dlls/game.cpp` calls it during `GameDLLInit` to register typed experimental cvars.
- `third_party/valve-halflife-sdk/dlls/client.cpp` now calls it from `StartFrame` so debug-only runtime helpers can arm themselves after launch-time cvars are applied.
- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp` is still the only weapon-specific gameplay patch site for the current Glock experiment.
- `src/weapon_debug_logger.cpp` is the local, non-vendored logger that mirrors telemetry to the server console and a disposable `testbed/logs/weapon-debug-<timestamp>.log` file.

This keeps the stock runtime on `-game valve`, keeps all debug writes inside the disposable testbed, and avoids any client DLL dependency.

## Current Glock cvars

- `sv_exp_pistol_tapfire`
  `0` keeps vanilla Glock primary cadence.
  `1` makes primary fire tap-fire only. Holding `IN_ATTACK` does not keep firing. Another primary shot requires a release and a fresh press.
- `sv_exp_move_spread_scale`
  Default `0.0` preserves vanilla spread. Values above `0.0` scale the server-side movement penalty applied to Glock primary spread.
- `sv_exp_first_shot_accuracy`
  `0` disables the feature.
  `1` allows a fully accurate accepted primary shot when the player is grounded, moving slowly enough, and has recovered long enough since the previous accepted shot.
- `sv_exp_spread_recovery`
  Quiet time in seconds before first-shot accuracy can return. Values less than or equal to `0` mean immediate recovery once the movement conditions are met.
- `sv_exp_debug_weaponlog`
  Default `0` keeps telemetry fully quiet.
  `1` logs accepted Glock primary shots for the current server-authoritative experiment.
- `sv_exp_debug_weaponlog_rejections`
  Default `0` logs accepted shots only.
  `1` also logs tap-fire hold rejections when primary fire is blocked because the player never released attack for a fresh press.

## What the Glock telemetry proves

The new telemetry is server-authoritative and stock-client-compatible:

- it proves which authoritative Glock primary shots were accepted
- it proves when the tap-fire hold gate rejected a primary attempt
- it records the server-side spread inputs used for that decision path
- it proves the disposable runtime is still running on the stock `valve` game content with no custom client DLL requirement

The telemetry does not prove player feel:

- it does not prove that the stock client prediction visually matches the server timing
- it does not prove subjective weapon feel, pacing, or readability
- it does not replace manual in-game firing passes for cadence and feel validation

Launch and smoke verification are automated. Actual weapon feel still requires manual in-game testing against the stock Steam Half-Life client.

## Stable telemetry shape

The telemetry now uses a stable, single-line, parser-friendly prefix plus `key=value` fields:

- session header
  `[weaponlog] type=session ts=... map=... event=weapon_debug_session status=ready game=valve file="..."`
- accepted primary shot
  `[weaponlog] type=accepted ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary ...`
- rejected tap-fire hold
  `[weaponlog] type=rejected ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary reason=tapfire_hold_blocked ...`

The line remains human-readable, but the stable prefix and `type=` field make it fast to parse. The current analyzer is also backward-compatible with older unprefixed `key=value` Glock logs so earlier manual sessions are still usable.

## One-click manual Glock session

`scripts/run-glock-test-session.ps1` exists to collapse the manual validation path into one disposable flow:

- reinstall the selected testbed runtime
- launch the experimental-debug Glock server on `-game valve`
- wait for the server to report that the map actually started
- surface the server log path and the current `weapon-debug-*.log` path
- optionally open a tail window and launch a stock client that auto-connects
- print the compact checklist that a human still has to execute in-game
- optionally pause for a manual stop point and run `scripts/analyze-weapon-log.ps1` against the latest weapon log when `-AnalyzeLatestOnExit` is requested

The corresponding BAT alias is `scripts\run-testbed.bat glock-session`.

## Telemetry analysis

`scripts/analyze-weapon-log.ps1` turns the manual Glock telemetry into a reviewable report.

It infers:

- whether accepted shots were logged
- whether tap-fire hold rejections were logged
- whether first-shot accepted events occurred
- whether accepted shots with `move_penalty > 0` occurred
- whether a later accepted event returned to `firstshot=1` after earlier non-firstshot accepted shots
- whether grounded crouch-moving accepted shots look like lower movement-penalty candidates than comparable grounded standing movement
- summary statistics for spread, movement penalty, and horizontal speed

It cannot infer:

- subjective stock-client feel
- prediction smoothness or viewmodel timing
- whether the player intentionally executed the exact movement pattern you wanted unless the telemetry clearly reflects it
- whether a single crouch-moving sample is enough to prove balance quality

That distinction matters. The analyzer summarizes evidence from logs; it does not replace a human in-game firing pass.

## Manual checklist summary

- connect the stock client to the disposable server on the printed port
- stand still, wait briefly, and fire one single primary shot
  expected: one accepted Glock line and `firstshot=1` once the recovery gate is satisfied
- hold primary without releasing
  expected: no repeated accepted shots and `tapfire_hold_blocked` rejection lines when rejection logging is enabled
- move continuously and fire primary
  expected: accepted lines with `move_penalty > 0`
- stop, wait past recovery, and fire again
  expected: the first-shot bonus can return
- crouch-move and compare against uncrouched movement at a similar speed
  expected: lower movement penalty than standing movement

This checklist is intentionally honest. It validates the authoritative telemetry and a human-observed firing pass, but it does not claim that stock-client prediction or weapon feel is already solved.

## Glock telemetry hook location

The Glock telemetry itself lives in two places:

- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp`
  The accepted-shot and tap-fire rejection decisions are logged exactly where the server decides whether Glock primary fire proceeds.
- `src/weapon_debug_logger.cpp`
  Formats single-line telemetry, resolves the disposable `testbed/logs/` path from the loaded `hl.dll`, writes the one-time session header, and keeps console logging alive even if the file cannot be opened.

The accepted-shot line includes timestamp, map, player identity, fire mode, experiment/tap-fire/first-shot flags, spread values, movement inputs, grounded or ducking state, time since the previous accepted primary shot when known, and clip ammo after the shot.

The rejection line includes timestamp, player identity, the `tapfire_hold_blocked` reason, and the movement and stance inputs that explain why the blocked evaluation happened.

## Stock client compatibility boundary

This experiment stays server-side:

- authoritative hit registration and spread live on the server
- launch still uses `-game valve`
- the standard Steam Half-Life client can still connect
- no `cl_dll/`, HUD, VGUI, or client event code is involved

That boundary is deliberate. Once a change must alter client-side prediction, presentation, or HUD behavior, it is no longer a stock-client-only experiment.

## Verification boundary

Keep these four layers separate when evaluating the Glock work:

- launch verification
  the disposable runtime started, stayed on `-game valve`, chose the expected map and port, and exposed the expected log paths
- telemetry generation
  the server produced the session-ready header plus accepted and rejected Glock lines that reflect the server-authoritative decision path
- telemetry analysis
  `scripts/analyze-weapon-log.ps1` parsed the log, summarized the observed signals, and reported which expected cases were or were not present in that telemetry
- manual in-game validation
  a human joined with a stock client and actually fired the Glock to compare standing, moving, recovery, and crouch-moving cases

Only the fourth layer speaks to real in-game behavior. The first three layers prove setup, telemetry generation, and evidence review, not prediction feel.

## Disposable launch entry points

- `scripts/run-testbed.bat`
  Vanilla disposable HLDS launch.
- `scripts/run-testbed.bat experimental`
  Experimental Glock launch with the existing movement, tap-fire, and first-shot cvar bundle.
- `scripts/run-testbed.bat experimental-debug`
  Experimental Glock launch plus `sv_exp_debug_weaponlog 1` and `sv_exp_debug_weaponlog_rejections 1`.
- `scripts/run-testbed.bat glock-session`
  One-click manual Glock session: disposable reinstall, experimental-debug launch, readiness wait, optional tail window, optional stock-client auto-connect, and printed checklist.
- `scripts/run-testbed.bat glock-report`
  Analyze the newest disposable Glock telemetry log, with optional forwarded export or assertion switches.
- `scripts/run-glock-test-session.ps1`
  PowerShell entry point for the same one-click manual session flow, plus an optional `-AnalyzeLatestOnExit` handoff after a manual stop point.
- `scripts/analyze-weapon-log.ps1`
  PowerShell analyzer for the newest or a specific `weapon-debug-*.log`, including concise summary output, JSON/CSV exports, and optional signal assertions.
- `scripts/tail-weapon-log.ps1`
  Follows the newest disposable Glock telemetry log, or a specific path passed through `-Path`, or dumps it once with `-NoFollow`.

The BAT wrapper remains thin and delegates the real launch behavior to `scripts/run-server.ps1`.

## Reuse pattern for later weapons

MP5 and shotgun work should reuse the same pattern instead of inventing a new one each time:

1. register future cvars in `src/future_gameplay_hooks.cpp`
2. keep reusable logging or helper code in `src/`
3. patch only the weapon-specific authoritative decision point in the vendored server weapon file
4. log single-line, server-authoritative accepted and rejected events that are easy to grep in `testbed/logs/`
5. keep the experiment compatible with the stock `-game valve` client until client prediction or presentation changes are genuinely required

For MP5, that likely means primary-fire cadence, movement spread, recoil-oriented telemetry, and the same analyzer/report/export pattern so manual sessions stay measurable. For shotgun, it means the same for buckshot spread, recovery timing, pellet-count diagnostics, and rejection/acceptance evidence, again without moving speculative logic into the client.
