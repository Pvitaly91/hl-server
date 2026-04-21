# Shared Weapon Tuning Core

This repository now uses one small server-side tuning core for the common Glock, MP5, and 357 tuning math instead of keeping separate ad-hoc implementations per weapon.

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

- Glock still owns tap-fire press/hold semantics and its primary-attack wrapper.
- MP5 still owns burst growth, burst spread accumulation, and MP5-specific lab loadout behavior.
- 357 still owns its single-shot fire wrapper, cadence, and 357-specific lab loadout behavior.
- Target dummy, cfg command, launcher, and editor workflows are unchanged.

## Compatibility

The user-facing tuning surface was kept stable:

- existing `sv_exp_glock_*`, `sv_exp_mp5_*`, `sv_exp_357_*`, and general `sv_exp_*` cvars remain valid
- existing editor-exported cfg files still load without format changes
- the C++ config editor still exports the same GoldSrc cfg surface

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

357 preset JSON files now live under `configs/357-presets/` with checked-in `default`, `precision_test`, and `headshot_test` examples for live-lab iteration.

## Why this helps future weapons

357 is the first proof that the shared seam is usable for a new weapon, not just a refactor of the original Glock and MP5 paths. The next server-side weapon can plug into the same shared spread and damage helpers instead of re-implementing movement penalty, first-shot qualification, spread clamping, and trace-damage/headshot handling from scratch. The intent is not a large framework; it is a small shared core with weapon-specific wrappers at the edges.

## Verified state

On `2026-04-21`, the shared-core path was verified in live play on `crossfire` with the existing live-lab workflow:

- `editor_glock_simple.cfg` applied successfully through `exp_cfg_apply`
- `editor_mp5_simple.cfg` applied successfully through `exp_cfg_apply`
- `editor_357_test.cfg` applied successfully through `exp_cfg_apply`
- persisted target-spot selection and `exp_target_respawn` still worked
- accepted Glock shots were logged after the refactor
- accepted MP5 shots were logged after the refactor
- accepted 357 shots, dummy hits, and dummy kills were logged in a real live session

357 remains experimental. The live proof shows that the shared core, editor export path, cfg commands, telemetry, and analyzer now cover a third weapon. It does not claim final gameplay balance or exact Counter-Strike parity.

Gameplay tuning and balance are still manual. This change is an architecture cleanup and compatibility-preserving refactor, not a gameplay rebalance pass.
