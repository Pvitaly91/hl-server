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

## Disposable launch entry points

- `scripts/run-testbed.bat`
  Vanilla disposable HLDS launch.
- `scripts/run-testbed.bat experimental`
  Experimental Glock launch with the existing movement, tap-fire, and first-shot cvar bundle.
- `scripts/run-testbed.bat experimental-debug`
  Experimental Glock launch plus `sv_exp_debug_weaponlog 1` and `sv_exp_debug_weaponlog_rejections 1`.
- `scripts/tail-weapon-log.ps1`
  Follows the newest disposable Glock telemetry log, or dumps it once with `-NoFollow`.

The BAT wrapper remains thin and delegates the real launch behavior to `scripts/run-server.ps1`.

## Reuse pattern for later weapons

MP5 and shotgun work should reuse the same pattern instead of inventing a new one each time:

1. register future cvars in `src/future_gameplay_hooks.cpp`
2. keep reusable logging or helper code in `src/`
3. patch only the weapon-specific authoritative decision point in the vendored server weapon file
4. log single-line, server-authoritative accepted and rejected events that are easy to grep in `testbed/logs/`
5. keep the experiment compatible with the stock `-game valve` client until client prediction or presentation changes are genuinely required

For MP5, that likely means primary-fire cadence, movement spread, and recoil-oriented telemetry at the exact server fire decision point. For shotgun, it means the same for buckshot spread, recovery timing, and pellet-count related diagnostics, again without moving speculative logic into the client.
