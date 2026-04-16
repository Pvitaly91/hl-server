# Future gameplay hooks

## Current seams

`src/future_gameplay_hooks.cpp` remains the main server-only seam for future gameplay work.

- `third_party/valve-halflife-sdk/dlls/game.cpp` calls it during `GameDLLInit` to register typed experimental cvars.
- `third_party/valve-halflife-sdk/dlls/client.cpp` now calls it from `StartFrame` so debug-only runtime helpers can arm themselves after launch-time cvars are applied.
- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp` remains the Glock-specific primary-fire patch site.
- `third_party/valve-halflife-sdk/dlls/mp5.cpp` is now the focused MP5 primary-fire patch site for movement spread, burst growth, and recovery experiments.
- `third_party/valve-halflife-sdk/dlls/combat.cpp` and `third_party/valve-halflife-sdk/dlls/player.cpp` carry the smallest safe server-side damage and post-hit telemetry hooks for the current Glock and MP5 passes.
- `src/weapon_debug_logger.cpp` is the local, non-vendored logger that mirrors Glock and MP5 telemetry to the server console and a disposable `testbed/logs/weapon-debug-<timestamp>.log` file.

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

## Current MP5 cvars

The MP5 experiment uses the same typed-accessor seam in `src/future_gameplay_hooks.cpp`.

- `sv_exp_weapon_under_test`
  Default empty string.
  Metadata-only cvar that records which weapon the session is intentionally exercising, for example `mp5`.
- `sv_exp_mp5_profile_name`
  Default `default`.
  String-like metadata cvar that records the selected checked-in MP5 preset name.
- `sv_exp_mp5_primary_enabled`
  Default `0`.
  `1` enables the experimental MP5 primary-fire path. `0` keeps the stock MP5 primary behavior.
- `sv_exp_mp5_primary_base_spread`
  Base spread for the experimental MP5 primary path before movement or burst penalties.
- `sv_exp_mp5_primary_ground_move_penalty`
  Grounded movement penalty coefficient, scaled by current horizontal speed.
- `sv_exp_mp5_primary_air_move_penalty`
  Flat airborne penalty used when the player is not grounded.
- `sv_exp_mp5_primary_duck_penalty_scale`
  Multiplier applied to the movement penalty while ducking.
- `sv_exp_mp5_primary_burst_growth`
  Additional spread added after each accepted rapid primary shot.
- `sv_exp_mp5_primary_burst_max_additional_spread`
  Clamp ceiling for the accumulated burst-added spread.
- `sv_exp_mp5_primary_spread_recovery`
  Recovery time in seconds for burst-added spread to decay back toward zero.
- `sv_exp_mp5_primary_damage`
  Experimental MP5 base damage used by the server-side primary path.
- `sv_exp_mp5_primary_headshot_scale`
  Head hitgroup multiplier for the experimental MP5 primary path.
- `sv_exp_mp5_primary_headshot_lethal`
  `1` allows the server to raise pre-armor MP5 headshot damage high enough to force a lethal result for the current target snapshot, and telemetry explicitly marks when that path actually ran.
- `sv_exp_mp5_primary_first_shot_accuracy`
  `1` allows a fully accurate grounded shot when the player is moving slowly enough and has recovered burst spread.
- `sv_exp_mp5_primary_first_shot_speed_threshold`
  Maximum horizontal speed for first-shot accuracy qualification.
- `sv_exp_mp5_primary_max_spread`
  Clamp ceiling for the final experimental MP5 primary spread.
- `sv_exp_mp5_lab_loadout`
  Default `0`.
  `1` enables the MP5 lab loadout helper for the first live testing player.
- `sv_exp_mp5_lab_ammo`
  Default `250`.
  Practical ammo target for the MP5 lab helper.
- `sv_exp_mp5_lab_autoswitch`
  Default `1`.
  When the helper grants the MP5 to a player for the first time, it also tries to select it immediately.

Defaults preserve current behavior when the MP5 experiment is off.

## Session and matrix metadata cvars

The session and matrix tags also live behind typed accessors in `src/future_gameplay_hooks.cpp`.

- `sv_exp_session_tag`
  Default empty string.
  Human-readable session tag for a single launch or matrix step.
- `sv_exp_matrix_name`
  Default empty string.
  Human-readable matrix identifier for grouped comparison runs.
- `sv_exp_matrix_step`
  Default empty string.
  Human-readable step tag inside the selected matrix.

These are metadata-only cvars. They do not change gameplay behavior by themselves. When set, the session header written to `[weaponlog]` includes them so later analyzer exports can correlate per-step artifacts across multiple sessions.

## Glock lab dummy cvars

The one-player Glock lab dummy also lives behind typed accessors in `src/future_gameplay_hooks.cpp`.

- `sv_exp_glock_lab_dummy`
  Default `0`.
  `1` enables the server-side Glock lab dummy workflow.
- `sv_exp_glock_lab_target_profile_name`
  Default `default`.
  String-like metadata cvar that records which checked-in target profile was selected at launch time. `default` means no checked-in lab target profile was selected.
- `sv_exp_glock_lab_dummy_health`
  Default `100.0`.
  Sets the dummy's spawn and respawn health.
- `sv_exp_glock_lab_dummy_armor`
  Default `0.0`.
  Sets the dummy's spawn armor for the dummy-only armor model.
- `sv_exp_glock_lab_dummy_head_protected`
  Default `0`.
  `1` allows headshots to participate in the dummy-only armor model.
- `sv_exp_glock_lab_dummy_armor_health_fraction`
  Default `0.5`.
  Fraction of protected-hit raw damage that still reaches health while dummy armor remains.
- `sv_exp_glock_lab_dummy_armor_drain_scale`
  Default `1.0`.
  Scale used to convert absorbed protected-hit damage into armor drain for the dummy-only armor model.
- `sv_exp_glock_lab_dummy_autorespawn`
  Default `1`.
  `1` respawns the dummy automatically after death.
- `sv_exp_glock_lab_dummy_respawn_delay`
  Default `1.0`.
  Delay in seconds before the dummy respawns.
- `sv_exp_glock_lab_dummy_spawn_distance`
  Default `256.0`.
  Forward distance from the first live player to the test lane spawn point.
- `sv_exp_glock_lab_dummy_model`
  Default `models/barney.mdl`.
  Allowed stock models are currently `models/barney.mdl` and `models/scientist.mdl`.
- `sv_exp_glock_lab_dummy_face_player`
  Default `1`.
  `1` turns the dummy toward the anchor player at spawn time.
- `sv_exp_glock_lab_dummy_offset_right`
  Default `0.0`.
  Horizontal right-offset applied to the spawn lane.
- `sv_exp_glock_lab_dummy_offset_up`
  Default `0.0`.
  Vertical offset applied before the floor trace settles the dummy onto the lane.

These defaults preserve current behavior when the lab mode is off.

## Glock lab dummy implementation

The dummy hook and entity logic stay in the existing local seam instead of spreading across more vendored SDK files:

- `src/future_gameplay_hooks.cpp`
  Registers the dummy and lab-target cvars, tracks the current map, finds the first live player, computes a safe lane position, spawns one stock `monster_generic`, and handles clear or autorespawn behavior.
- `src/weapon_debug_logger.cpp`
  Emits `dummy_spawn`, `dummy_respawn`, and `dummy_clear` lifecycle lines, applies the dummy-only armor or head-protection model inside the existing Glock trace-damage context, and writes armor-aware dummy victim classification on Glock `hit` and `kill` telemetry.

The current implementation intentionally stays small:

- it uses a stock human model from the base `valve` content
- it stays passive and stationary
- it respawns near the same lane position
- it generates real server-side hitgroup, headshot, kill, lethal-headshot, and dummy-armor telemetry

The dummy-only armor model is explicit rather than hidden behind claims of exact reuse:

- chest and stomach hits use the dummy armor model when dummy armor remains
- headshots bypass the dummy armor model unless `sv_exp_glock_lab_dummy_head_protected 1`
- protected-hit raw damage is split into `damage_to_health` and `damage_absorbed`
- absorbed damage drains dummy armor through `sv_exp_glock_lab_dummy_armor_drain_scale`
- once dummy armor reaches `0`, later hits behave as unarmored again

The current dummy is still an approximation rather than a real player surrogate:

- its armor or head-protection behavior is a dummy-only experimental model, not guaranteed exact Half-Life or Counter-Strike player armor parity
- non-head hitgroups still use monster-side skill multipliers rather than real player damage scaling
- it does not validate player movement, client prediction, or exact PvP behavior

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
3. selected lab target profile from `configs/glock-lab-targets/`
4. explicit command-line overrides

This keeps tuning iteration versioned, reviewable, and server-side while staying stock-client-compatible on `-game valve`.

Checked-in Glock lab target profiles now live under `configs/glock-lab-targets/` as small JSON files.

Current examples:

- `unarmored.json`
  Experimental no-armor dummy target.
- `vest.json`
  Experimental torso-armored dummy target. Chest and stomach hits use the dummy-only armor model, but headshots still bypass it.
- `vest_headprotected.json`
  Experimental armored dummy target with head protection enabled. Headshots also use the dummy-only armor model.

Each lab target profile file contains:

- `name`
- `description`
- `cvars`

The descriptions intentionally say "experimental" because the lab target profiles are one-player testing aids, not exact PvP armor presets.

## MP5 preset files

Checked-in MP5 tuning presets live under `configs/mp5-presets/` as small JSON files.

Current examples:

- `baseline.json`
  Experimental MP5 tuning that stays close to the current server-side defaults.
- `cs_burst.json`
  Experimental tighter MP5 tuning with stronger burst-growth sensitivity.
- `cs_mobile.json`
  Experimental more mobile MP5 tuning with softer grounded movement penalties.

Each preset file contains:

- `name`
- `description`
- `cvars`

The current launcher merge order for MP5 sessions is:

1. built-in experimental MP5 defaults
2. selected MP5 preset from `configs/mp5-presets/`
3. selected lab target profile from `configs/glock-lab-targets/` when the MP5 lab flow also uses the one-player dummy
4. explicit command-line overrides

The MP5 lab loadout helper also stays in `src/future_gameplay_hooks.cpp` and is intentionally narrow:

- it looks for the first live player during `StartFrame`
- it grants `weapon_9mmAR` when missing
- it tops the player's `9mm` ammo toward `sv_exp_mp5_lab_ammo`
- it tries `SelectItem("weapon_9mmAR")` on first grant when `sv_exp_mp5_lab_autoswitch 1`

This helper exists only to make one-player MP5 lab sessions practical. It is not a general gameplay rebalance hook.

## Glock comparison matrices

Checked-in manual comparison matrices now live under `configs/glock-comparison-matrices/` as small JSON files.

Current examples:

- `quick_smoke`
  Minimal manual sanity pass for launch, tagging, and report aggregation.
- `armor_sweep`
  Same Glock profile across unarmored, armored, and protected-head dummy states.
- `profile_sweep`
  Multiple Glock profiles against one consistent target profile.

Each matrix file contains:

- `name`
- `description`
- optional `defaults`
- ordered `steps`

Each step can define:

- `name`
- `glockProfile`
- `labTargetProfile`
- optional `operatorNote`
- optional `map`
- optional `extraCvars`

The current matrix runner is intentionally Glock-lab-specific. It reuses the existing one-click session, analyzer, and report exports instead of trying to become a generic tournament harness.

## What the MP5 telemetry proves

The current MP5 telemetry is still server-authoritative and stock-client-compatible:

- it proves which authoritative MP5 primary shots were accepted
- it records burst index, burst-added spread, movement penalty, grounded or ducking state, current speed, and time since the previous accepted shot
- it proves whether burst-growth evidence and movement-penalty evidence appeared in a session
- it records the active MP5 preset metadata in the session header and exported analyzer JSON
- it records hitgroup-aware hit and kill telemetry, including dummy victim classification, applied damage, and explicit headshot-lethal evidence when configured
- it can be filtered in `scripts/analyze-weapon-log.ps1` through `-Weapon mp5`
- it can be asserted through `-RequireWeaponAccepted`, `-RequireWeaponHits`, `-RequireWeaponKills`, `-RequireWeaponHeadshotKills`, `-RequireBurstGrowthEvidence`, and `-RequireMovementPenaltyEvidence`

It still does not prove:

- client-side recoil feel
- client prediction quality
- exact Counter-Strike recoil or damage parity
- that synthetic fixtures prove live weapon behavior
- that a one-player dummy session replaces real PvP testing

## What the Glock telemetry proves

The current telemetry is server-authoritative and stock-client-compatible:

- it proves which authoritative Glock primary shots were accepted
- it proves when the tap-fire hold gate rejected a primary attempt
- it records the spread/recovery inputs that produced those shot-attempt decisions
- it records when the one-player lab dummy spawned, respawned, or was cleared
- it records which authoritative Glock primary hits and kills were observed after damage resolution
- it marks when a hit or kill victim was the lab dummy through `victim_kind=dummy` and `victim_class=glock_lab_dummy`
- it records which lab target profile and dummy-armor coefficients were active in the session
- it records the optional session tag, matrix name, and matrix step on the session header when the matrix runner or another caller sets them
- it records whether dummy armor actually applied on a logged hit plus how much raw damage was absorbed versus forwarded to health
- it records whether the explicit headshot-lethal path was merely configured or actually applied on a logged hit
- it proves the disposable runtime is still running on the stock `valve` game content with no custom client DLL requirement

The telemetry still has important limits:

- it does not prove subjective stock-client feel, pacing, or readability
- it does not prove exact Counter-Strike parity
- it does not prove that every headshot is universally one-shot outside the logged target state
- it does not bypass Half-Life armor rules; lethal-headshot behavior is still evaluated against the server's health and armor snapshot for the specific victim
- dummy headshot or armor evidence does not prove real player armor behavior because the dummy uses a deliberately explicit approximation model
- it only proves what the server observed for that hitgroup and that damage path, not the player's intent

Launch and smoke verification are automated. Actual weapon feel still requires manual in-game testing against the stock Steam Half-Life client.

## Stable telemetry shape

The telemetry now uses a stable, single-line, parser-friendly prefix plus `key=value` fields:

- session header
  `[weaponlog] type=session ts=... map=... event=weapon_debug_session status=ready game=valve file="..." profile="..." ... sv_exp_glock_lab_target_profile_name="vest_headprotected" sv_exp_glock_lab_dummy_armor=100.0 sv_exp_glock_lab_dummy_head_protected=1 sv_exp_glock_lab_dummy_armor_health_fraction=0.500 sv_exp_glock_lab_dummy_armor_drain_scale=1.000 session_tag="armor_sweep-20260416-01-unarmored" matrix_name="armor_sweep" matrix_step="unarmored"`
- accepted primary shot
  `[weaponlog] type=accepted ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary ...`
  `[weaponlog] type=accepted ts=... map=... player="..." entindex=... userid=... weapon=mp5 fire=primary profile="cs_burst" spread=... move_penalty=... burst_additional_spread=... burst_index=... delta_prev=...`
- rejected tap-fire hold
  `[weaponlog] type=rejected ts=... map=... player="..." entindex=... userid=... weapon=glock fire=primary reason=tapfire_hold_blocked ...`
- dummy lifecycle
  `[weaponlog] type=dummy_spawn ts=... map=... dummy="Glock Lab Dummy" entindex=... dummy_class=glock_lab_dummy dummy_model="models/barney.mdl" health=... autorespawn=... respawn_delay=... spawn_distance=... anchor="..." ... target_profile="vest_headprotected" spawn_health=... spawn_armor=... head_protected=... armor_health_fraction=... armor_drain_scale=...`
  `[weaponlog] type=dummy_respawn ...`
  `[weaponlog] type=dummy_clear ...`
- hit result
  `[weaponlog] type=hit ts=... map=... attacker="..." victim="..." victim_kind=... victim_class=... victim_model="..." weapon=glock fire=primary hitgroup=... target_profile="vest_headprotected" applied_damage=... damage_raw=... damage_to_health=... damage_absorbed=... armor_drain=... dummy_armor_before=... dummy_armor_after=... armor_applied=... head_protected=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`
  `[weaponlog] type=hit ts=... map=... attacker="..." victim="..." victim_kind=... victim_class=... victim_model="..." weapon=mp5 fire=primary hitgroup=... target_profile="vest_headprotected" applied_damage=... damage_raw=... armor_applied=... head_protected=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`
- kill result
  `[weaponlog] type=kill ts=... map=... attacker="..." victim="..." victim_kind=... victim_class=... victim_model="..." weapon=glock fire=primary hitgroup=... target_profile="vest_headprotected" applied_damage=... dummy_armor_after=... armor_applied=... head_protected=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`
  `[weaponlog] type=kill ts=... map=... attacker="..." victim="..." victim_kind=... victim_class=... victim_model="..." weapon=mp5 fire=primary hitgroup=... target_profile="vest_headprotected" applied_damage=... dummy_armor_after=... armor_applied=... head_protected=... headshot=... headshot_lethal_active=... headshot_lethal_applied=...`

The line remains human-readable, but the stable prefix and `type=` field make it fast to parse. The current analyzer is also backward-compatible with older unprefixed `key=value` Glock logs so earlier manual sessions are still usable.

The session header now exposes the active profile name and the actual tuning values that the server used for that run. That makes it possible to compare telemetry evidence against the real launch profile instead of relying on memory or handwritten notes.

The analyzer now preserves those tags in exported JSON through:

- `session.sessionTag`
- `session.matrixName`
- `session.matrixStep`
- `metadata.sessionTag`
- `metadata.matrixName`
- `metadata.matrixStep`
- `comparisonSummary`

`comparisonSummary` also flattens the key counters, damage stats, evidence booleans, and missing-signal notes that the multi-session aggregation script uses.

For MP5 logs, the exported JSON also carries normalized fields such as:

- `metadata.weaponUnderTest`
- `metadata.mp5Profile`
- `comparisonSummary.weaponUnderTest`
- `comparisonSummary.mp5Profile`
- `comparisonSummary.burstGrowthEvidenceCount`
- `comparisonSummary.movementPenaltyEvidenceCount`

For dummy-driven sessions, the authoritative dummy configuration that matters for evidence review is the lifecycle telemetry itself:

- `dummy_spawn` or `dummy_respawn` tells you that a stock server-side dummy actually existed in the live session
- the lifecycle line exposes the dummy health, armor, model, autorespawn state, respawn delay, spawn distance, target profile, and anchor player identity
- `victim_kind=dummy` on `hit` and `kill` lines distinguishes real dummy evidence from generic Glock telemetry
- `armor_applied`, `damage_absorbed`, `dummy_armor_before`, and `dummy_armor_after` distinguish armored dummy evidence from generic dummy evidence

## One-click manual Glock session

`scripts/run-glock-test-session.ps1` exists to collapse the manual validation path into one disposable flow:

- reinstall the selected testbed runtime
- launch the experimental-debug Glock server on `-game valve`
- wait for either the session-ready weapon log header or the traditional map-start marker
- surface the server log path and the current `weapon-debug-*.log` path
- optionally open a tail window and launch a stock client that auto-connects
- print the compact checklist that a human still has to execute in-game
- optionally pause for a manual stop point and run `scripts/analyze-weapon-log.ps1` against the latest weapon log when `-AnalyzeLatestOnExit` is requested

The corresponding BAT alias is `scripts\run-testbed.bat glock-session`.

The one-player range variant is `scripts\run-testbed.bat glock-lab`, or `scripts\run-glock-test-session.ps1 -LabDummy`.

The comparison-matrix layer sits above that same one-click session flow:

- `scripts/run-glock-comparison-matrix.ps1` loads a checked-in matrix, launches each step in order, tags the session with `sv_exp_session_tag`, `sv_exp_matrix_name`, and `sv_exp_matrix_step`, prints a comparison-oriented checklist, and exports per-step analyzer JSON and CSV.
- `scripts/compare-weapon-reports.ps1` reads those analyzer JSON outputs, prints a concise step/session comparison table, and can export consolidated JSON, CSV, and Markdown.

This helps validate:

- that the right Glock preset and dummy target profile were actually used for each step
- that tap-fire rejection, movement penalty, dummy headshot, armored dummy, protected-head dummy, and lethal-headshot evidence appeared or did not appear for each step
- that repeated manual runs can be tagged and aggregated without hand-editing notes

It still does not validate by itself:

- subjective stock-client feel
- whether the operator actually aimed or moved the way they intended unless the log proves it
- exact real-player or exact PvP armor behavior
- any conclusion that still depends on real opponents, map pressure, or broader balance context

That path keeps the same disposable runtime flow, but it also:

- enables `sv_exp_glock_lab_dummy 1`
- constrains the intended session to one human player
- optionally merges `-LabTargetProfile <name>` on top of the selected Glock weapon preset
- prints a dummy-focused checklist that covers appearance, active target profile, careful standing shots, headshots, hold-to-fire rejection, moving shots, kill or respawn behavior, and post-pass analysis

Dedicated lab-target helpers now exist:

- `scripts\run-testbed.bat glock-lab-targets`
  Lists the checked-in target profiles.
- `scripts\run-testbed.bat glock-lab-target vest_headprotected`
  Starts a one-player lab session with that target profile.

## Telemetry analysis

`scripts/analyze-weapon-log.ps1` turns the manual Glock telemetry into a reviewable report.

It summarizes:

- which Glock profile metadata and tuning values were present in the session header
- which lab target profile metadata and dummy armor coefficients were present in the session header
- whether dummy spawn, respawn, and clear lifecycle lines were logged
- whether accepted shots were logged
- whether tap-fire hold rejections were logged
- whether hit and kill lines were logged
- whether any hit and kill lines targeted the lab dummy
- whether any dummy hits actually applied the dummy armor model
- whether any protected-head dummy headshots were logged
- whether any hit lines were headshots
- whether any kill lines were headshot kills
- whether any hit line explicitly recorded `headshot_lethal_applied=1`
- whether any dummy hit or kill line explicitly recorded `headshot_lethal_applied=1`
- whether first-shot accepted events occurred
- whether accepted shots with `move_penalty > 0` occurred
- whether a later accepted event returned to `firstshot=1` after earlier non-firstshot accepted shots
- whether grounded crouch-moving accepted shots look like lower movement-penalty candidates than comparable grounded standing movement
- summary statistics for spread, movement penalty, horizontal speed, applied damage, raw dummy damage, damage-to-health, absorbed damage, and armor drain
- per-hitgroup counts when hit telemetry is present

It can assert non-zero on missing evidence through:

- `-RequireHits`
- `-RequireKills`
- `-RequireHeadshotKills`
- `-RequireLethalHeadshotEvidence`
- `-RequireDummySpawns`
- `-RequireDummyHits`
- `-RequireDummyHeadshotHits`
- `-RequireDummyHeadshotKills`
- `-RequireArmoredDummyHits`
- `-RequireProtectedDummyHeadshotHits`
- `-RequireProtectedDummyHeadshotKills`
- `-RequireDummyLethalHeadshotEvidence`

It cannot infer:

- subjective stock-client feel
- prediction smoothness or viewmodel timing
- whether the player intentionally executed the exact movement pattern you wanted unless the telemetry clearly reflects it
- whether a single crouch-moving sample is enough to prove balance quality
- whether a kill alone means the lethal-headshot path ran; that requires explicit telemetry such as `headshot_lethal_applied=1`
- whether a logged lethal headshot proves every target state is one-shot; armor and current health still matter
- whether dummy evidence automatically generalizes to real player-vs-player armor or movement
- whether protected-head dummy evidence proves exact player helmet rules; it only proves the dummy's logged armor model

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
- in `glock-lab`, confirm one stationary stock human dummy appeared in front of the player
  expected: a `dummy_spawn` line and a visible Barney or Scientist target in the lane
- confirm which target profile is active
  expected: the printed checklist and telemetry agree on the selected `target_profile`
- stand still, wait briefly, and fire one careful primary shot
  expected: one accepted Glock line and `firstshot=1` once the recovery gate is satisfied
- land at least one dummy headshot and one dummy kill in the lab flow
  expected: `victim_kind=dummy` hit or kill lines, plus `headshot=1` evidence for the headshot attempt
- hold primary without releasing
  expected: no repeated accepted shots and `tapfire_hold_blocked` rejection lines when rejection logging is enabled
- move continuously and fire primary
  expected: accepted lines with `move_penalty > 0`
- if autorespawn is enabled, confirm the dummy dies and reappears in the same lane
  expected: `dummy_clear` followed by `dummy_respawn`
- after the pass, analyze the newest log
  expected: the analyzer reports whether dummy-driven live firing, armored dummy hits, protected-head headshot kills, and dummy lethal-headshot evidence were actually present

This checklist is intentionally honest. It validates the authoritative telemetry and a human-observed firing pass, but it does not claim that stock-client prediction or weapon feel is already solved.

## Glock telemetry hook location

The current Glock and MP5 telemetry lives in four focused places:

- `src/future_gameplay_hooks.cpp`
  Owns the typed experimental cvars plus the one-player Glock lab dummy lifecycle and the MP5 lab loadout helper. This is where the dummy spawn position is computed, where the stock `monster_generic` is created, where the MP5 loadout is granted, and where clear or autorespawn decisions happen from the `StartFrame` seam.

- `third_party/valve-halflife-sdk/dlls/wpn_shared/hl_wpn_glock.cpp`
  The accepted-shot and tap-fire rejection decisions are logged exactly where the server decides whether Glock primary fire proceeds, and this file now brackets experimental Glock primary shots with a narrow hit-telemetry context.
- `third_party/valve-halflife-sdk/dlls/mp5.cpp`
  The accepted-shot decision for experimental MP5 primary fire now computes movement spread, burst growth, burst recovery, optional first-shot accuracy, and the narrow per-shot telemetry context for hit and kill logging.
- `src/weapon_debug_logger.cpp`
  Formats single-line telemetry, resolves the disposable `testbed/logs/` path from the loaded `hl.dll`, writes the one-time session header, records dummy lifecycle lines, applies the dummy-only armor model during experimental Glock or MP5 trace damage, marks dummy victims on hit and kill lines, and keeps console logging alive even if the file cannot be opened.
- `third_party/valve-halflife-sdk/dlls/combat.cpp` and `third_party/valve-halflife-sdk/dlls/player.cpp`
  The experimental Glock and MP5 hit and kill telemetry finalizes after the stock server damage path resolves. Those are the smallest practical hook points for hitgroup-aware damage scaling and post-damage telemetry in this pass.

The accepted-shot line includes timestamp, map, player identity, fire mode, experiment/tap-fire/first-shot flags, spread values, movement inputs, grounded or ducking state, time since the previous accepted primary shot when known, and clip ammo after the shot.

The rejection line includes timestamp, player identity, the `tapfire_hold_blocked` reason, and the movement and stance inputs that explain why the blocked evaluation happened.

The hit and kill lines include timestamp, map, attacker and victim identity, victim kind or class or model, hitgroup, applied damage, pre/post health when available, pre/post armor for player victims when available, active profile metadata, dummy target-profile metadata when relevant, dummy raw or absorbed damage accounting, and whether the explicit lethal-headshot logic was configured or actually applied.

The analyzer distinguishes dummy evidence from generic hit or kill telemetry by looking for either:

- explicit dummy lifecycle lines such as `type=dummy_spawn`, `type=dummy_respawn`, or `type=dummy_clear`
- dummy victim classification on `hit` and `kill` lines through `victim_kind=dummy` or `victim_class=glock_lab_dummy`

The analyzer distinguishes armored dummy evidence from generic dummy evidence by looking for fields such as:

- `target_profile`
- `armor_applied`
- `damage_absorbed`
- `dummy_armor_before`
- `dummy_armor_after`
- `head_protected`

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
- `scripts/run-testbed.bat glock-lab`
  One-player Glock lab session: the same disposable experimental-debug flow plus `sv_exp_glock_lab_dummy 1` and the dummy-focused checklist.
- `scripts/run-testbed.bat glock-lab-targets`
  Lists the checked-in lab target profiles.
- `scripts/run-testbed.bat glock-lab-target <name>`
  One-player Glock lab session with `-LabTargetProfile <name>`.
- `scripts/run-testbed.bat glock-lab-profile <name>`
  Same one-player Glock lab flow, but injects `-GlockProfile <name>` and can still forward `-LabTargetProfile <name>`.
- `scripts/run-testbed.bat glock-report`
  Analyze the newest disposable Glock telemetry log, with optional forwarded export or assertion switches.
- `scripts/run-glock-test-session.ps1`
  PowerShell entry point for the same one-click manual session flow, with `-LabDummy` for the one-player range variant, `-LabTargetProfile <name>` for checked-in dummy target profiles, and an optional `-AnalyzeLatestOnExit` handoff after a manual stop point.
- `scripts/list-glock-profiles.ps1`
  Lists the checked-in Glock preset files, their paths, and their short descriptions.
- `scripts/list-glock-lab-targets.ps1`
  Lists the checked-in dummy target-profile files, their paths, and their short descriptions.
- `scripts/analyze-weapon-log.ps1`
  PowerShell analyzer for the newest or a specific `weapon-debug-*.log`, including concise summary output, session profile or tuning metadata, dummy lifecycle counters, dummy armor evidence, JSON or CSV exports, and optional signal assertions.
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
