# Future gameplay hooks

## Current hook point

`src/future_gameplay_hooks.cpp` is the current server-only bootstrap seam. It is called from `third_party/valve-halflife-sdk/dlls/game.cpp` during `GameDLLInit`, which makes it a safe place to register experimental server cvars and confirm the custom GameDLL is active.

## Current Glock pass

The first experimental pass is limited to Glock primary fire in `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp`.

- `sv_exp_pistol_tapfire = 0`
  Vanilla Glock primary fire cadence.
- `sv_exp_pistol_tapfire = 1`
  Primary fire becomes tap-fire only. Holding `IN_ATTACK` does not keep firing. Another primary shot requires releasing the button and pressing again. Secondary fire is unchanged.
- `sv_exp_move_spread_scale`
  Scales the movement penalty applied to Glock primary spread. The default is `0.0`, which preserves vanilla spread. When set above `0.0`, the server computes:
  - base primary spread `0.01`
  - grounded movement penalty `0.08 * clamp(speed2D / max(maxspeed, 1), 0, 1) * sv_exp_move_spread_scale`
  - airborne movement penalty `0.12 * sv_exp_move_spread_scale`
  - ducking multiplies only the movement penalty by `0.75`
  - final spread is clamped to `0.0 .. 0.2`
- `sv_exp_first_shot_accuracy = 0`
  Disabled.
- `sv_exp_first_shot_accuracy = 1`
  If the player is grounded, moving at `40` units per second or slower, and has recovered long enough since the last accepted Glock primary shot, the next accepted primary shot uses `0.0` spread.
- `sv_exp_spread_recovery`
  Quiet time in seconds required before first-shot accuracy can return. Values less than or equal to `0` mean immediate recovery once the movement conditions are met.

## Stock client compatibility caveat

This pass stays server-only:

- authoritative hit registration and spread change on the server
- the disposable runtime still launches with `-game valve`
- the standard Steam Half-Life client remains compatible

Because the client DLL is unchanged, the local predicted feel of tap-fire and spread timing may not perfectly match the server-authoritative result. That caveat is intentional for this pass.

## Where server-side weapon logic should go

- `third_party/valve-halflife-sdk/dlls/weapons.cpp` for shared weapon state and attack timing glue
- individual weapon files in `third_party/valve-halflife-sdk/dlls/` for weapon-specific fire behavior
- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp` for the Glock-specific path already used by the official project
- `src/` for local glue that should stay clearly separated from upstream code

## Current server cvars

- `sv_exp_pistol_tapfire`
- `sv_exp_move_spread_scale`
- `sv_exp_first_shot_accuracy`
- `sv_exp_spread_recovery`

## What can stay server-only

- damage values
- fire rate limits
- spread calculations used by hit registration
- server-side cvar gating for experimentation
- movement-state checks that influence authoritative weapon logic

## What would still require a custom client DLL

- client HUD changes
- custom crosshair behavior
- prediction-sensitive weapon feel that must match a visibly altered client-side fire model
- new client-only animations or viewmodel behavior
- custom weapon selection UI or VGUI changes

As long as the experiments stay within authoritative server-side weapon logic and the standard `valve` content, the stock Steam Half-Life client can remain untouched.

## Next logical extension points

- MP5 primary fire can reuse the same movement-spread helper pattern while preserving stock client compatibility.
- Shotgun buckshot spread can reuse the same cvar seam for movement penalties and per-shot recovery timing.
- If more weapons need first-shot or tap-fire state, move only the smallest reusable pieces into `src/` or shared weapon glue instead of broad gameplay rewrites.
