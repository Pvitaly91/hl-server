# Shared Weapon Tuning Core

This repository now uses one small server-side tuning core for the common Glock, MP5, 357, and shotgun tuning math instead of keeping separate ad-hoc implementations per weapon.

## What moved into the shared core

The shared layer covers the tuning dimensions that were already duplicated across the weapon implementations:

- base spread
- ground movement penalty
- air movement penalty
- duck penalty scale
- first-shot accuracy
- first-shot speed threshold
- spread recovery inputs
- max spread
- base damage
- headshot scale
- headshot lethal flag

The core exposes small spread and damage profiles plus helper functions that compute:

- normalized movement state from the firing player
- final spread after movement, duck, first-shot, and recovery inputs
- shared damage and headshot handling for dummy and non-dummy traces

## What stayed weapon-specific

The refactor does not force fake symmetry where the weapons behave differently by design:

- Glock still owns optional legacy tap-fire semantics and its primary-attack wrapper.
- MP5 still owns burst growth, burst spread accumulation, and MP5-specific lab loadout behavior.
- 357 still owns its single-shot fire wrapper, cadence, and 357-specific lab loadout behavior.
- Shotgun keeps primary-fire pellet count, per-pellet traces, and pellet-hit aggregation in the shotgun wrapper plus telemetry layer. Only the common spread and per-pellet damage profile math moved into the shared core.
- Target dummy, cfg command, launcher, and editor workflows are unchanged.

## Compatibility

The user-facing tuning surface was kept stable:

- existing `sv_exp_glock_*`, `sv_exp_mp5_*`, `sv_exp_357_*`, `sv_exp_shotgun_*`, and general `sv_exp_*` cvars remain valid
- existing editor-exported cfg files still load without format changes
- the C++ config editor still exports the same GoldSrc cfg surface
- the first round-mode loadout selector can now reuse the same supported weapon set with `sv_exp_round_loadout_mode glock|mp5|357|shotgun`

## Current Feel Philosophy

This repository is explicitly improving stock-client-compatible HLDM gameplay, not chasing a full Counter-Strike clone.

- Glock now favors accurate single shots through first-shot qualification plus cadence-grown inaccuracy that recovers over time.
- The old hard tap-fire gate can still exist as a legacy switch, but it is no longer the recommended path for skillful pistol feel.
- MP5 now favors controlled bursts: repeated accepted shots add burst spread, waiting lets that extra spread decay, and long held fire is meant to bloom more than short bursts.
- Movement, air state, crouch stability, and readable headshot damage remain part of the shared model.
- Because the client DLL is still stock Half-Life, the server can improve authoritative spread and damage behavior but cannot promise exact client-side recoil or prediction parity.

## Current Recommended Presets

- Glock: `glock_cs_like_soft` for a softer mobile pistol feel, `glock_cs_like_tight` for a stricter single-shot-focused feel.
- MP5: `mp5_controlled_burst` for the main "short burst beats spray" path, `mp5_mobile_soft` for a lighter movement-oriented variant.

That keeps the live-lab workflow unchanged:

```text
exp_cfg_apply editor_glock_simple.cfg
exp_target_use_saved default
exp_target_respawn
```

and

```text
exp_cfg_apply editor_mp5_simple.cfg
exp_target_use_saved default
exp_target_respawn
```

and

```text
exp_cfg_apply editor_357_test.cfg
exp_target_use_saved default
exp_target_respawn
```

and

```text
exp_cfg_apply editor_shotgun_test.cfg
exp_target_use_saved default
exp_target_respawn
```

357 preset JSON files now live under `configs/357-presets/` with checked-in `default`, `precision_test`, and `headshot_test` examples for live-lab iteration.

Shotgun preset JSON files now live under `configs/shotgun-presets/` with checked-in `default`, `close_quickkill`, and `precision_test` examples for live-lab iteration.

Round-mode cfgs can layer on top of the same weapon configs. A typical live test can now combine:

```text
exp_cfg_apply editor_357_test.cfg
sv_exp_round_mode 1
sv_exp_round_loadout_mode 357
exp_round_start
```

That keeps weapon tuning, deterministic loadout grant, and the no-respawn duel loop on the same server-side surface.

## Why this helps future weapons

357 and shotgun show that the shared seam is usable for new weapons, not just a refactor of the original Glock and MP5 paths. The next server-side weapon can plug into the same shared spread and damage helpers instead of re-implementing movement penalty, first-shot qualification, spread clamping, and trace-damage/headshot handling from scratch. The intent is not a large framework; it is a small shared core with weapon-specific wrappers at the edges.

## Verified state

On `2026-04-21`, the shared-core path was verified in live play on `crossfire` with the existing live-lab workflow:

- `editor_glock_simple.cfg` applied successfully through `exp_cfg_apply`
- `editor_mp5_simple.cfg` applied successfully through `exp_cfg_apply`
- `editor_357_test.cfg` applied successfully through `exp_cfg_apply`
- persisted target-spot selection and `exp_target_respawn` still worked
- accepted Glock shots were logged after the refactor
- accepted MP5 shots were logged after the refactor
- accepted 357 shots, dummy hits, and dummy kills were logged in a real live session

On `2026-04-22`, the Glock and MP5 feel pass was verified again in fresh client-attached sessions on `crossfire`:

- Glock telemetry in `weapon-debug-20260422-231348.log` showed an accurate first shot followed by cadence and recovery evidence on the next accepted shot, with the new `shot_growth`, `additional_spread`, and `recovery_applied` fields populated.
- MP5 telemetry in `weapon-debug-20260422-233056.log` showed nonzero `burst_additional_spread`, partial `recovery_applied`, and higher spread on the next accepted shot under the `editor_mp5_simple` controlled-burst cfg.
- In both sessions, the target dummy still spawned from the persisted saved spot, so the live-lab workflow remained intact while the weapon-feel telemetry changed.

Shotgun integration is present in the same shared-core path, including editor export, cfg apply, lab loadout wiring, telemetry, and analyzer support. On `2026-04-21`, the remaining blocker was not the server-side shotgun code path itself but unstable live client launches around Steam initialization. The strongest live shotgun evidence from that date was:

- `editor_shotgun_test.cfg` applied successfully through `exp_cfg_apply`
- the target dummy workflow still rebuilt and repositioned correctly under shotgun cfg control
- shotgun accepted-shot telemetry and real dummy deaths were captured in live logs before the shotgun hit/kill aggregation fix
- the hit/kill aggregation lifetime bug was fixed in source by keeping the active shotgun shot context alive until after `FinalizeActiveShotgunPrimaryHitTelemetry()`

That means the shotgun shared-core implementation is in place, but a fresh post-fix end-to-end live proof of shotgun hit and kill telemetry still depends on a clean client-attached run.

357 remains experimental. The live proof shows that the shared core, editor export path, cfg commands, telemetry, and analyzer now cover a third weapon. It does not claim final gameplay balance or exact Counter-Strike parity.

Gameplay tuning and balance are still manual. This shared core now supports a better server-side feel pass, but it does not claim full CS parity or perfect client-side feel.
