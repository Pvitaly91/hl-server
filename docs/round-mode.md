# Round-Based Duel Mode

This repo now includes a first server-side round loop for live duel testing on top of the existing `hlserver_testbed` workflow.

## What it does

- waits for players
- starts a round with freeze time
- resets player health and armor
- grants a deterministic loadout when configured
- blocks respawn while the round is live
- ends the round on elimination
- starts the next round after a short restart delay

## What it does not do yet

- economy
- buy menu
- team-based win logic
- Counter-Strike parity
- polished join-in-progress handling

This is a clean first round loop, not a full game-mode conversion.

## Cvars

- `sv_exp_round_mode`
  - `0` = off
  - `1` = on
- `sv_exp_round_freeze_time`
- `sv_exp_round_restart_delay`
- `sv_exp_round_start_health`
- `sv_exp_round_start_armor`
- `sv_exp_round_no_respawn`
- `sv_exp_round_friendlyfire`
  - metadata now, and forwarded to `mp_friendlyfire` for future team-oriented work
- `sv_exp_round_weapon_profile`
  - optional profile/notes field for the current round config
- `sv_exp_round_loadout_mode`
  - `none`
  - `glock`
  - `mp5`
  - `357`
  - `shotgun`

## Round states

- `waiting_for_players`
- `freeze_time`
- `live`
- `round_end`
- `restart_pending`

## Commands

- `exp_round_start`
  - enables round mode and starts a round when a player is present
- `exp_round_restart`
  - forces a clean round reset immediately
- `exp_round_status`
  - prints current state, timers, player counts, loadout mode, start health/armor, and last winner/reason
- `exp_round_stop`
  - disables round mode and restores normal deathmatch respawn
- `exp_round_slay [all]`
  - optional round-test helper for server-side elimination during local validation

## Recommended workflow

Example 357 duel loop:

```text
exp_cfg_apply editor_357_test.cfg
sv_exp_round_mode 1
sv_exp_round_loadout_mode 357
sv_exp_round_start_health 100
sv_exp_round_start_armor 0
exp_round_start
```

Example shotgun duel loop:

```text
exp_cfg_apply editor_shotgun_test.cfg
sv_exp_round_mode 1
sv_exp_round_loadout_mode shotgun
exp_round_start
```

During a live session:

1. Use `exp_round_status` to confirm the current state.
2. Wait through freeze time.
3. Fight the round.
4. Dead players stay out until restart.
5. Use `exp_round_restart` if you want a forced reset without waiting for elimination.
6. Use `exp_round_stop` when you want to return to regular deathmatch behavior.

## Notes

- With one human player, the mode still works for solo validation. The round goes live, and the only player must be eliminated before the restart path triggers.
- Round loadout reset is server-side and deterministic. It reuses the same experimental weapon surfaces already driven by cfg files.
- Existing target dummy, cfg apply, and persistent saved-spot workflows remain available when round mode is off.
