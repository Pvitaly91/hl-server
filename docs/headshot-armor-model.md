# Headshot And Armor Model

This note describes the current real-player and fake-verification-client armor/headshot path that sits under the experimental weapon-feel work.

## Scope

- server-side only
- stock Half-Life client compatible
- useful for live tuning and verification
- not a claim of exact Counter-Strike armor or helmet parity

## Current Model

When the experimental armor path is enabled:

- player `armorvalue` is the shared protection pool
- helmet state is tracked separately as an on/off head-protection flag
- body hits can drain armor and split damage between armor and health
- head hits can either follow the protected-head path or the unprotected-head path depending on helmet state
- the same logic is used for real players and fake verification clients

The model is intentionally readable in logs rather than hidden behind opaque damage math.

## Status And Verification Commands

Useful live commands:

```text
exp_armor_status [player]
exp_armor_set <player> <armor>
exp_helmet_set <player> <0|1>
exp_player_hit_test <attacker> <victim> <weapon> <hitgroup>
```

Supported hitgroups for `exp_player_hit_test`:

```text
head
body
chest
stomach
leftarm
rightarm
leftleg
rightleg
generic
```

The verification command path currently supports:

```text
glock
mp5
357
shotgun
```

## Direct Hit Telemetry

The important improvement in this pass is that player and fake-player hits are now inspectable directly on the `type=hit` and `type=kill` lines.

Key fields:

- `victim_is_player`
- `victim_is_fake`
- `helmet_equipped`
- `head_protection_active`
- `armor_hit_protected`
- `armor_model`
- `health_before`
- `health_after`
- `armor_before`
- `armor_after`
- `damage_raw`
- `damage_to_health`
- `damage_absorbed`
- `armor_drain`
- `headshot`
- `headshot_lethal_active`
- `headshot_lethal_applied`
- `verification`

Interpretation:

- `helmet_equipped=1` and `head_protection_active=1` on a head hit means the protected-head path was active for that hit.
- `armor_hit_protected=1` means armor actually participated in the damage resolution for that hitgroup.
- `damage_absorbed` and `armor_drain` show what armor consumed on that exact hit, rather than requiring a later status diff.
- `verification=1` marks scripted verification hits triggered through `exp_player_hit_test`.

## Recommended Live Test

One practical live flow:

```text
exp_team_fake_add team1 verify_alpha
exp_team_fake_add team2 verify_bravo
exp_armor_set verify_bravo 100
exp_helmet_set verify_bravo 1
exp_player_hit_test verify_alpha verify_bravo glock body
exp_player_hit_test verify_alpha verify_bravo glock head
exp_armor_set verify_bravo 100
exp_helmet_set verify_bravo 0
exp_player_hit_test verify_alpha verify_bravo 357 head
```

Expected outcomes:

- Glock body hit: armor should absorb part of the hit and the log should show both armor and health changing.
- Glock helmeted head hit: `helmet_equipped=1`, `head_protection_active=1`, and nonzero armor absorption should be visible on the hit line.
- 357 unprotected head hit: `helmet_equipped=0`, `head_protection_active=0`, and near-full health damage should be visible directly on the hit or kill line.

## Analyzer Support

`scripts/analyze-weapon-log.ps1` now summarizes direct player/fake-player evidence separately from dummy evidence, including:

- player hit count
- real-player hit count
- fake-player hit count
- player headshot hit count
- player headshot kill count
- helmet-protected headshot count
- unprotected headshot count
- direct player armor evidence count
- player armor absorbed total
- player armor drain total

## Verified Example

On `2026-04-23`, `weapon-debug-20260423-140809.log` captured direct fake-player verification hits in a live client-attached session on `crossfire`:

- a Glock body hit on `verify_bravo` with `helmet_equipped=1`, `armor_hit_protected=1`, `health_before=100.0`, `health_after=91.0`, `armor_before=100.0`, `armor_after=91.0`, and `damage_absorbed=9.0000`
- a Glock protected head hit on the same fake victim with `helmet_equipped=1`, `head_protection_active=1`, `damage_absorbed=31.5000`, and direct armor/health deltas on the same line
- a 357 unprotected head hit with `helmet_equipped=0`, `head_protection_active=0`, `damage_absorbed=0.0000`, and a direct `type=kill` line for the fake victim

That verified:

- direct hit telemetry exists for fake-player victims
- helmet-protected headshot evidence is directly inspectable
- unprotected headshot evidence is directly inspectable
- armor absorption is logged per hit instead of only being inferred later
