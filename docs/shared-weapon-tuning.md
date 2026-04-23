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
- cadence-sensitive follow-up penalties that reward patient clicks and decay after idle time
- optional deterministic per-shot pattern offsets with idle reset
- shared damage and headshot handling for dummy and non-dummy traces

## What stayed weapon-specific

The refactor does not force fake symmetry where the weapons behave differently by design:

- Glock still owns optional legacy tap-fire semantics and its primary-attack wrapper.
- MP5 still owns burst growth, burst spread accumulation, and MP5-specific lab loadout behavior.
- 357 still owns its single-shot fire wrapper and 357-specific lab loadout behavior while reusing the shared cadence, spread, and deterministic pattern helpers.
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
- Glock now also has an explicit cadence mode that measures time since the previous accepted shot and adds a fast-click plus hold-fire penalty instead of relying on a hard tap-fire gate as the main skill lever.
- Glock can now optionally layer a deterministic follow-up pattern over that cadence model so second and third shots are learnable instead of feeling like a pure random cone.
- The old hard tap-fire gate can still exist as a legacy switch, but it is no longer the recommended path for skillful pistol feel.
- 357 now uses the same cadence model so patient clicks stay precise, rushed follow-up clicks add a visible penalty, and waiting long enough resets the cadence state.
- 357 can now also layer a deterministic follow-up pattern over that cadence model so the second and third accepted shots stay readable instead of feeling like random luck.
- MP5 now favors controlled bursts: repeated accepted shots add burst spread, waiting lets that extra spread decay, and long held fire is meant to bloom more than short bursts.
- MP5 now also has an explicit burst reset window and sustained-spray hold penalty so short 2-5 shot strings recover cleanly while deeper spray stacks more aggressively.
- MP5 can now optionally layer a deterministic early-burst pattern over the same shared spread and recovery model so the opening spray shape is more learnable.
- Shotgun primary fire now uses a deterministic pellet-layout mode on top of the shared spread and recovery inputs so the cone stays recognizably shotgun-like while repeated close-range shots become more learnable than a pure random pellet cloud.
- Movement, air state, crouch stability, and readable headshot damage remain part of the shared model.
- Because the client DLL is still stock Half-Life, the server can improve authoritative spread and damage behavior but cannot promise exact client-side recoil or prediction parity.

## Shotgun Telemetry Model

Shotgun keeps per-pellet traces internally, but the live weapon log uses one aggregated hit or kill line per shell per victim.

That aggregate line is the authoritative tuning record:

- `health_before` / `health_after` come from the shell aggregate, not a later post-death entity snapshot.
- `armor_before` / `armor_after` come from the same aggregate.
- `applied_damage` and `damage_to_health` should now agree for shotgun hit lines.
- `damage_absorbed` and `armor_drain` show the armor contribution from that full shell.
- `pellets_hit` and `headshot_pellets` show how many pellets contributed to the final result.
- `hitgroup="mixed"` means the shell combined multiple pellet hitgroups on the same victim. `headshot=1` still means at least one pellet hit the head.

This keeps the log readable without losing the information needed to validate dummy, fake-player, and real-player shotgun tuning.

For repeatable live verification, two focused helpers are now available:

- `exp_verify_shotgun_hit [player]` keeps the dummy in front of the resolved live player and fires one real shotgun `PrimaryAttack()` through the server weapon path.
- `sv_exp_shotgun_verify_autofire 1` with `sv_exp_shotgun_verify_autofire_count N` arms a short automatic sequence after the live player joins, which is useful for proving nonlethal hit, kill, and pattern-reset telemetry in a fresh log.

## Current Recommended Presets

- Glock: `glock_cadence_soft` for a softer cadence-sensitive pistol feel, `glock_cadence_tight` for a stricter rhythm-focused single-shot path, and `glock_pattern_tight` when you also want a stronger learnable follow-up pattern.
- 357: `357_precision_duel` for deliberate duel pacing, `357_cadence_headshot` for stronger headshot validation, `357_pattern_soft` for a gentler learnable pattern, and `357_pattern_tight` for a stricter precision path.
- MP5: `mp5_controlled_burst` for the main "short burst beats spray" path, `mp5_mobile_soft` for a lighter movement-oriented variant, `mp5_pattern_burst` when you want stronger learnable early-burst patterning, and `mp5_spray_harsh` when you want a deliberately punishing long-spray comparison preset.
- Shotgun: `shotgun_pattern_soft` for a gentler learnable pellet spread, `shotgun_pattern_tight` for the tighter deterministic layout, `shotgun_close_quickkill` for heavier close-range testing, and `shotgun_precision_test` for cleaner dummy validation.

Relevant new cvars for this pass:

- Glock cadence: `sv_exp_glock_primary_cadence_mode`, `sv_exp_glock_primary_cycle_time`, `sv_exp_glock_primary_click_penalty`, `sv_exp_glock_primary_click_penalty_scale`, `sv_exp_glock_primary_click_reset_time`, `sv_exp_glock_primary_hold_penalty_scale`
- 357 cadence: `sv_exp_357_primary_cadence_mode`, `sv_exp_357_primary_cycle_time`, `sv_exp_357_primary_click_penalty`, `sv_exp_357_primary_click_penalty_scale`, `sv_exp_357_primary_click_reset_time`, `sv_exp_357_primary_hold_penalty_scale`
- Glock: `sv_exp_glock_pattern_mode`, `sv_exp_glock_pattern_scale_x`, `sv_exp_glock_pattern_scale_y`, `sv_exp_glock_pattern_reset_time`, `sv_exp_glock_pattern_max_index`
- 357: `sv_exp_357_pattern_mode`, `sv_exp_357_pattern_scale_x`, `sv_exp_357_pattern_scale_y`, `sv_exp_357_pattern_reset_time`, `sv_exp_357_pattern_max_index`
- MP5: `sv_exp_mp5_primary_burst_growth`, `sv_exp_mp5_primary_burst_max_additional_spread`, `sv_exp_mp5_primary_spread_recovery`, `sv_exp_mp5_primary_burst_reset_time`, `sv_exp_mp5_primary_hold_penalty_scale`, `sv_exp_mp5_pattern_mode`, `sv_exp_mp5_pattern_scale_x`, `sv_exp_mp5_pattern_scale_y`, `sv_exp_mp5_pattern_reset_time`, `sv_exp_mp5_pattern_max_index`
- Shotgun: `sv_exp_shotgun_primary_shot_growth`, `sv_exp_shotgun_pattern_mode`, `sv_exp_shotgun_pattern_scale_x`, `sv_exp_shotgun_pattern_scale_y`, `sv_exp_shotgun_pattern_reset_time`, `sv_exp_shotgun_pattern_max_index`, `sv_exp_shotgun_primary_pellet_spread_mode`

Cadence mode and pattern mode are still server-side approximations. They do not add client-side recoil animation or a custom prediction model.

For the current MP5 polish pass, read the model like this:

- first few accepted shots: deterministic pattern makes the opening burst more learnable
- continued held fire: burst growth plus hold penalty push `next_additional_spread` up faster
- short idle pause: recovery reduces current spread
- longer idle pause: burst reset and pattern reset return the weapon closer to its clean opening state

## Real-Player Headshot And Armor Telemetry

The current gameplay layer now exposes the real-player and fake-verification-client armor/head-protection path directly in the weapon log instead of forcing later inference from status snapshots.

Important model notes:

- This is still a deterministic server-side approximation, not a claim of exact Counter-Strike armor parity.
- Real players and fake verification clients use the same experimental armor/head-protection logic when `sv_exp_armor_mode` and `sv_exp_helmet_mode` are enabled.
- Body armor and helmet/head protection are surfaced separately in logs and status output even though they still share the same underlying `armorvalue` pool on the player entity.
- `armor_model=player_custom` means the experimental player armor path produced the logged result.
- `armor_model=player_stock` is reserved for cases where stock player armor behavior is still what the trace observed.

Direct per-hit player telemetry now logs:

- attacker / victim identity
- `victim_is_player`
- `victim_is_fake`
- `helmet_equipped`
- `head_protection_active`
- `armor_hit_protected`
- `armor_model`
- `health_before` / `health_after`
- `armor_before` / `armor_after`
- `damage_raw`
- `damage_to_health`
- `damage_absorbed`
- `armor_drain`
- `verification`

That makes these verification paths practical without client DLL changes:

```text
exp_team_fake_add team1 verify_alpha
exp_team_fake_add team2 verify_bravo
exp_armor_set verify_bravo 100
exp_helmet_set verify_bravo 1
exp_player_hit_test verify_alpha verify_bravo glock body
exp_player_hit_test verify_alpha verify_bravo glock head
exp_helmet_set verify_bravo 0
exp_player_hit_test verify_alpha verify_bravo 357 head
```

The analyzer now distinguishes direct player/fake-player evidence from inferred evidence and summarizes:

- player hit count
- real-player hit count
- fake-player hit count
- player headshot hit/kill count
- helmet-protected headshot count
- unprotected headshot count
- direct player armor evidence count
- total armor absorbed / armor drain on player hits

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

On `2026-04-23`, deterministic pattern mode was verified in fresh client-attached sessions on `crossfire`:

- Glock telemetry in `weapon-debug-20260423-010332.log` showed `pattern_mode=1`, pattern indices progressing from `1` to `3` across consecutive careful follow-up shots, and later `pattern_reset=1` after an idle pause.
- MP5 telemetry in `weapon-debug-20260423-010419.log` showed `pattern_mode=1`, a learnable early burst progressing through indices `1` to `5`, and repeated `pattern_reset=1` events after short pauses between bursts.
- The analyzer summaries for both logs surfaced nonzero pattern evidence counts, pattern index ranges, and reset counts, so the deterministic-pattern layer is inspectable in the normal live tuning workflow.

On `2026-04-23`, Glock and 357 cadence mode was verified in fresh client-attached sessions on `crossfire`:

- Glock cadence telemetry showed `cadence_mode=1`, changing `cadence_interval`, nonzero `cadence_penalty`, and later `cadence_reset=1` after an idle pause under an editor-exported cadence cfg.
- 357 cadence telemetry showed the same cadence fields progressing across consecutive accepted clicks, then resetting after a longer pause, again without client DLL changes.
- The analyzer summary for those sessions surfaced cadence evidence counts, cadence-reset counts, and penalty ranges so the cadence layer is inspectable in the normal live workflow.

On `2026-04-23`, shotgun deterministic pellet-pattern mode was verified in a fresh client-attached session on `crossfire` under the normal cfg-driven workflow:

- the exported `editor_shotgun_test.cfg` settings were active under `weapon_under_test="shotgun"` with `sv_exp_shotgun_pattern_mode=1`, `sv_exp_shotgun_pattern_scale_x=0.56`, `sv_exp_shotgun_pattern_scale_y=0.68`, `sv_exp_shotgun_pattern_reset_time=0.95`, and `sv_exp_shotgun_primary_pellet_spread_mode=1`
- the target dummy still spawned from the persisted saved spot and could be repositioned with `exp_target_tp_front`, so the normal live-lab dummy workflow stayed intact
- accepted-shot telemetry in `weapon-debug-20260423-152423.log` showed `pattern_mode=1`, pattern indices progressing from `0` to `1` across consecutive shots, and later `pattern_reset=1` after a longer idle gap
- the same session logged direct dummy hit and kill lines under shotgun pattern mode, plus a dummy respawn afterward
- the analyzer summary for that log surfaced `accepted shots 5`, `hit events 3`, `kill events 1`, `pattern evidence 5`, `pattern resets 1`, and pellet-hit fields, so the new shotgun pattern layer is inspectable through the normal tuning workflow

On `2026-04-23`, the shotgun telemetry aggregation bug was then corrected for fresh live logs:

- the runtime now keeps `health_after`, `armor_after`, damage-to-health, armor absorption, and kill state from the authoritative shell aggregate instead of mixing pellet totals with a later dummy entity snapshot
- that fixes the prior nonlethal-looking line where `applied_damage` and the logged health delta disagreed after the dummy corpse health had already been rewritten by monster death handling
- the analyzer now reports hit and kill consistency counts so fresh shotgun logs can explicitly prove the corrected model instead of requiring manual eyeballing

357 remains experimental. The live proof shows that the shared core, editor export path, cfg commands, telemetry, and analyzer now cover a third weapon. It does not claim final gameplay balance or exact Counter-Strike parity.

Gameplay tuning and balance are still manual. This shared core now supports a better server-side feel pass, but it does not claim full CS parity or perfect client-side feel.
