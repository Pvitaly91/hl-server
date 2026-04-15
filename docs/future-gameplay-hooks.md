# Future gameplay hooks

## Current hook point

`src/future_gameplay_hooks.cpp` is the current server-only bootstrap seam. It is called from `third_party/valve-halflife-sdk/dlls/game.cpp` during `GameDLLInit`, which makes it a safe place to register experimental server cvars and confirm the custom GameDLL is active.

## Where future server-side weapon logic should go

- `third_party/valve-halflife-sdk/dlls/weapons.cpp` for shared weapon state and attack timing glue
- individual weapon files in `third_party/valve-halflife-sdk/dlls/` for weapon-specific fire behavior
- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp` for the Glock-specific path already used by the official project
- `src/` for local glue that should stay clearly separated from upstream code

## Suggested cvars to grow from the placeholder module

The placeholder registration area already exposes server cvars that can become the configuration surface for future work:

- `sv_exp_pistol_tapfire`
- `sv_exp_move_spread_scale`
- `sv_exp_first_shot_accuracy`
- `sv_exp_spread_recovery`

Recommended use:

- `sv_exp_pistol_tapfire` for reduced repeat-fire cadence or special tap-fire handling
- `sv_exp_move_spread_scale` for movement-dependent spread penalties
- `sv_exp_first_shot_accuracy` for first-shot bonus logic
- `sv_exp_spread_recovery` for post-shot recovery timing

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
