# Future gameplay hooks

## Current seams

`src/future_gameplay_hooks.cpp` remains the main server-only seam for future gameplay work.

- `third_party/valve-halflife-sdk/dlls/game.cpp` calls it during `GameDLLInit` to register typed experimental cvars.
- `third_party/valve-halflife-sdk/dlls/client.cpp` now calls it from `StartFrame` so debug-only runtime helpers can arm themselves after launch-time cvars are applied.
- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp` is still the only weapon-specific gameplay patch site for the current Glock experiment.
- `third_party/valve-halflife-sdk/dlls/combat.cpp` and `third_party/valve-halflife-sdk/dlls/player.cpp` carry the smallest safe server-side damage and post-hit telemetry hooks for this Glock-specific pass.
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
- `sv_exp_glock_profile_name`
  String-like server cvar used as active preset metadata. `default` means no checked-in preset was selected.
- `sv_exp_glock_primary_base_spread`
  Glock primary base spread coefficient for the experimental server path.
- `sv_exp_glock_primary_ground_move_penalty`
  Ground movement penalty coefficient before the generic `sv_exp_move_spread_scale` multiplier is applied.
- `sv_exp_glock_primary_air_move_penalty`
  Air movement penalty coefficient before the generic `sv_exp_move_spread_scale` multiplier is applied.
- `sv_exp_glock_primary_duck_penalty_scale`
  Multiplier applied to the movement penalty when ducking.
- `sv_exp_glock_primary_first_shot_speed_threshold`
  Maximum horizontal speed for first-shot accuracy qualification.
- `sv_exp_glock_primary_max_spread`
  Clamp ceiling for experimental Glock primary spread.
- `sv_exp_glock_primary_damage`
  Experimental Glock primary base bullet damage used only by the server-side experimental primary path.
- `sv_exp_glock_primary_headshot_scale`
  Head hitgroup multiplier for the experimental Glock primary path. Other hitgroups still use the stock HL skill-data multipliers in this pass.
- `sv_exp_glock_primary_headshot_lethal`
  `0` keeps the experimental path on explicit damage and hitgroup scaling only.
  `1` allows the server to raise a Glock primary headshot's pre-armor damage high enough to force a lethal result for the current target snapshot, and the telemetry explicitly marks when that path actually ran.
- `sv_exp_debug_weaponlog`
  Default `0` keeps telemetry fully quiet.
  `1` logs accepted Glock primary shots for the current server-authoritative experiment.
- `sv_exp_debug_weaponlog_rejections`
  Default `0` logs accepted shots only.
  `1` also logs tap-fire hold rejections when primary fire is blocked because the player never released attack for a fresh press.

The Glock tuning coefficients now live behind typed accessors in `src/future_gameplay_hooks.cpp`, so changing presets or launch-time overrides no longer requires recompiling the weapon logic.

## Preset files

Checked-in Glock tuning presets live under `configs/glock-presets/` as small JSON files.

Current examples:

- `baseline.json`
  Stays close to the current experimental Glock spread tuning and stock Half-Life Glock damage multipliers.
- `cs_tight.json`
  Experimental tighter, more deliberate tuning with harsher movement penalties plus optional lethal-headshot server logic for deliberate headshot trials.
- `cs_mobile.json`
  Experimental more mobile tuning with lighter movement penalties and a milder headshot damage profile than the tighter preset.

Each preset file contains:

- `name`
- `description`
- `cvars`

The `cvars` block now version-controls both spread/recovery tuning and lethality tuning, including:

- `sv_exp_glock_primary_damage`
- `sv_exp_glock_primary_headshot_scale`
- `sv_exp_glock_primary_headshot_lethal`

The launcher merge order is:

1. built-in experimental Glock defaults
2. selected preset file
3. explicit command-line overrides

This keeps tuning iteration versioned, reviewable, and server-side while staying stock-client-compatible on `-game valve`.

## What the Glock telemetry proves

The current telemetry is server-authoritative and stock-client-compatible:

- it proves which authoritative Glock primary shots were accepted
- it proves when the tap-fire hold gate rejected a primary attempt
- it records the spread/recovery inputs that produced those shot-attempt decisions
- it records which authoritative Glock primary hits and kills were observed after damage resolution
- it records whether the explicit headshot-lethal path was merely configured or actually applied on a logged hit
- it proves the disposable runtime is still running on the stock `valve` game content with no custom client DLL requirement

The telemetry still has important limits:

- it does not prove subjective stock-client feel, pacing, or readability
- it does not prove exact Counter-Strike parity
- it does not prove that every headshot is universally one-shot outside the logged target state
- it does not bypass Half-Life armor rules; lethal-headshot behavior is still evaluated against the server's health and armor snapshot for the specific victim
- it only proves what the server observed for that hitgroup and that damage path, not the player's intent

Launch and smoke verification are automated. Actual weapon feel still requires manual in-game testing against the stock Steam Half-Life client.

## Stable telemetry shape

The telemetry now uses a stable, single-line, parser-friendly prefix plus `key=value` fields:

- session header
  `[weaponlog] type=session ts=... map=... event=weapon_debug_session status=ready game=valve file="..." profile="..." tapfire=... move_scale=... firstshot_enabled=... recovery=... base=... ground_move_penalty=... air_move_penalty=... duck_penalty_scale=... firstshot_speed=... max_spread=... sv_exp_glock_primary_damage=... sv_exp_glock_primary_headshot_scale=... sv_exp_glock_primary_headshot_lethal=...`
- accepted primary shot
  `[weaponlog] type=accepted ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary ...`
- rejected tap-fire hold
  `[weaponlog] type=rejected ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary reason=tapfire_hold_blocked ...`
- hit result
  `[weaponlog] type=hit ts=... map=... attacker="..." victim="..." weapon=glock fire=primary hitgroup=... applied_damage=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`
- kill result
  `[weaponlog] type=kill ts=... map=... attacker="..." victim="..." weapon=glock fire=primary hitgroup=... applied_damage=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`

The line remains human-readable, but the stable prefix and `type=` field make it fast to parse. The current analyzer is also backward-compatible with older unprefixed `key=value` Glock logs so earlier manual sessions are still usable.

The session header now exposes the active profile name and the actual tuning values that the server used for that run. That makes it possible to compare telemetry evidence against the real launch profile instead of relying on memory or handwritten notes.

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

It summarizes:

- which Glock profile metadata and tuning values were present in the session header
- whether accepted shots were logged
- whether tap-fire hold rejections were logged
- whether hit and kill lines were logged
- whether any hit lines were headshots
- whether any kill lines were headshot kills
- whether any hit line explicitly recorded `headshot_lethal_applied=1`
- whether first-shot accepted events occurred
- whether accepted shots with `move_penalty > 0` occurred
- whether a later accepted event returned to `firstshot=1` after earlier non-firstshot accepted shots
- whether grounded crouch-moving accepted shots look like lower movement-penalty candidates than comparable grounded standing movement
- summary statistics for spread, movement penalty, horizontal speed, and applied damage
- per-hitgroup counts when hit telemetry is present

It can assert non-zero on missing evidence through:

- `-RequireHits`
- `-RequireKills`
- `-RequireHeadshotKills`
- `-RequireLethalHeadshotEvidence`

It cannot infer:

- subjective stock-client feel
- prediction smoothness or viewmodel timing
- whether the player intentionally executed the exact movement pattern you wanted unless the telemetry clearly reflects it
- whether a single crouch-moving sample is enough to prove balance quality
- whether a kill alone means the lethal-headshot path ran; that requires explicit telemetry such as `headshot_lethal_applied=1`
- whether a logged lethal headshot proves every target state is one-shot; armor and current health still matter

That distinction matters. The analyzer summarizes evidence from logs; it does not replace a human in-game firing pass.

## Telemetry layers

Keep these layers separate when reading the reports:

- shot-attempt telemetry
  Accepted and rejected lines explain whether the authoritative Glock primary path fired at all, plus the spread and gating inputs behind that decision.
- hit and kill telemetry
  Hit and kill lines explain what the authoritative server observed after trace and damage resolution, including hitgroup, applied damage, and whether the explicit lethal-headshot path ran.
- subjective feel validation
  A human still has to fire the stock client and judge cadence, readability, responsiveness, and prediction feel.

Tuning and telemetry evidence are still separate from feel testing:

- tuning and telemetry evidence
  proves which preset and coefficients were active, plus what the server-authoritative firing path did with them
- subjective in-game feel testing
  still requires a human to fire the weapon with the stock client and judge cadence, readability, responsiveness, and prediction feel

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

The Glock telemetry itself lives in three places:

- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp`
  The accepted-shot and tap-fire rejection decisions are logged exactly where the server decides whether Glock primary fire proceeds, and this file now brackets experimental Glock primary shots with a narrow hit-telemetry context.
- `src/weapon_debug_logger.cpp`
  Formats single-line telemetry, resolves the disposable `testbed/logs/` path from the loaded `hl.dll`, writes the one-time session header, and keeps console logging alive even if the file cannot be opened.
- `third_party/valve-halflife-sdk/dlls/combat.cpp` and `third_party/valve-halflife-sdk/dlls/player.cpp`
  The experimental Glock hit and kill telemetry finalizes after the stock server damage path resolves. Those are the smallest practical hook points for hitgroup-aware damage scaling and post-damage telemetry in this pass.

The accepted-shot line includes timestamp, map, player identity, fire mode, experiment/tap-fire/first-shot flags, spread values, movement inputs, grounded or ducking state, time since the previous accepted primary shot when known, and clip ammo after the shot.

The rejection line includes timestamp, player identity, the `tapfire_hold_blocked` reason, and the movement and stance inputs that explain why the blocked evaluation happened.

The hit and kill lines include timestamp, map, attacker and victim identity, hitgroup, applied damage, pre/post health when available, pre/post armor for player victims when available, active profile metadata, and whether the explicit lethal-headshot logic was configured or actually applied.

This is still an honest approximation layer:

- armor handling follows the standard Half-Life server damage rules
- hitgroups only reflect what the authoritative trace path reported for that hit
- the current implementation is Glock-primary-specific rather than a general weapon damage system
- "lethal headshot evidence" means the server raised that hit's pre-armor damage through the dedicated path and marked it explicitly, not that every future headshot will behave identically

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
  the server produced the session-ready header plus accepted, rejected, hit, and kill Glock lines that reflect the server-authoritative decision and damage paths
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
- `scripts/run-testbed.bat glock-session-profile <name>`
  Same one-click manual session flow, but injects `-GlockProfile <name>` without making the BAT wrapper responsible for the real launch logic.
- `scripts/run-testbed.bat glock-report`
  Analyze the newest disposable Glock telemetry log, with optional forwarded export or assertion switches.
- `scripts/run-glock-test-session.ps1`
  PowerShell entry point for the same one-click manual session flow, plus an optional `-AnalyzeLatestOnExit` handoff after a manual stop point.
- `scripts/list-glock-profiles.ps1`
  Lists the checked-in Glock preset files, their paths, and their short descriptions.
- `scripts/analyze-weapon-log.ps1`
  PowerShell analyzer for the newest or a specific `weapon-debug-*.log`, including concise summary output, session profile/tuning metadata, JSON/CSV exports, and optional signal assertions.
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
