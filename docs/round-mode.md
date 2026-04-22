# Round Mode

This repo now includes a server-side no-respawn round loop for live testing on top of the existing `hlserver_testbed` workflow. It supports:

- duel mode
- simple two-team round mode for 1v1 / 2v2 style tests

## What it does

- waits for players
- starts a round with freeze time
- resets player health and armor
- grants a deterministic loadout when configured
- blocks respawn while the round is live
- ends the round on elimination
- starts the next round after a short restart delay
- can assign players onto two explicit server-side teams
- can reset per-team health, armor, and loadout in team mode
- can end a team round on last team alive

## What it does not do yet

- full economy tree
- buy menu
- Counter-Strike parity
- polished join-in-progress handling

This is a clean first round loop with a small console-driven buy prototype, not a full game-mode conversion.

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
- `sv_exp_team_round_mode`
  - `0` = off
  - `1` = on
- `sv_exp_team_round_teamplay`
  - `0` = teammate relationship metadata only
  - `1` = teammate relationship plus same-team damage blocking when `sv_exp_round_friendlyfire 0`
- `sv_exp_team_round_spawn_mode`
  - `dm_spawns`
  - `manual_spots`
  - `manual_spots` prefers persisted per-team saved spawns, then explicit fallback to the normal map deathmatch spawns
- `sv_exp_team_round_team1_name`
- `sv_exp_team_round_team2_name`
- `sv_exp_team_round_team1_loadout`
- `sv_exp_team_round_team2_loadout`
- `sv_exp_team_round_team1_health`
- `sv_exp_team_round_team2_health`
- `sv_exp_team_round_team1_armor`
- `sv_exp_team_round_team2_armor`
- `sv_exp_buy_mode`
  - `0` = off
  - `1` = on
- `sv_exp_buy_freeze_only`
  - `0` = buy any time while round mode is active
  - `1` = buy only during `freeze_time`
- `sv_exp_buy_team_shared_catalog`
  - `0` = reserved for future per-team catalog splits
  - `1` = current shared weapon shop for all players
- `sv_exp_buy_start_money`
- `sv_exp_buy_round_win_reward`
- `sv_exp_buy_round_loss_reward`
- `sv_exp_buy_max_money`
- `sv_exp_buy_allow_glock`
- `sv_exp_buy_allow_mp5`
- `sv_exp_buy_allow_357`
- `sv_exp_buy_allow_shotgun`
- `sv_exp_buy_cost_glock`
- `sv_exp_buy_cost_mp5`
- `sv_exp_buy_cost_357`
- `sv_exp_buy_cost_shotgun`

## Round states

- `waiting_for_players`
- `freeze_time`
- `live`
- `round_end`
- `restart_pending`

## Commands

- `exp_round_start`
  - enables round mode and starts a round when the required players are present
- `exp_round_restart`
  - forces a clean round reset immediately
- `exp_round_status`
  - prints current state, timers, player counts, team counts when enabled, loadout data, and last winner/reason
- `exp_round_stop`
  - disables round mode and restores normal deathmatch respawn
- `exp_round_slay [all|team1|team2]`
  - optional round-test helper for server-side elimination during local validation
- `exp_team_join <player> <team>`
  - manually assign one player to `team1`, `team2`, or the configured team names
- `exp_team_autoassign`
  - evenly assign connected players across the two configured teams
- `exp_team_status`
  - prints current team config, counts, and player assignments
- `exp_team_fake_add <team> [name]`
  - optional local verification helper that spawns one server-side fake client on the requested team
- `exp_team_fake_clear`
  - removes any fake clients created by `exp_team_fake_add`
- `exp_team_spawn_mark <team> [name]`
  - saves or updates a named spawn for the requested team on the current map. If no name is given, it writes `default`
- `exp_team_spawn_unmark <team> <name>`
  - removes one named saved spawn from the requested team
- `exp_team_spawn_list`
  - prints the saved team-spawn state for the current map
- `exp_team_spawn_use <team> <name>`
  - selects the active named spawn for the requested team
- `exp_team_spawn_status`
  - prints the current team-spawn file path, saved names, active selection, last applied spawn source, and last failure per team
- `exp_buy_list`
  - prints the allowed weapons, their costs, whether the shared catalog is enabled, and whether buying is currently open
- `exp_buy_status`
  - prints buy configuration plus each tracked player's money, selected weapon, and bought-this-freeze state
- `exp_buy <weapon> [player]`
  - attempts a server-side buy for the requested player. If the player argument is omitted, the command uses the only connected real player when that is unambiguous
- `exp_buy_clear [player]`
  - refunds and clears the current selected purchase for the requested player, then reapplies the base round/team loadout
- `exp_buy_grant [player]`
  - reapplies the currently selected purchase for the requested player for local verification
- `exp_buy_setmoney <player> <amount>`
  - small server-side verification helper for adjusting one player's money during testing

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

Example team-round loop:

```text
exp_cfg_apply editor_mp5_simple.cfg
sv_exp_round_mode 1
sv_exp_team_round_mode 1
sv_exp_team_round_team1_name alpha
sv_exp_team_round_team2_name bravo
sv_exp_team_round_team1_loadout mp5
sv_exp_team_round_team2_loadout 357
exp_team_autoassign
exp_team_status
exp_round_start
```

During a team session:

1. Use `exp_team_status` to verify assignments and alive counts.
2. Use `exp_round_status` to inspect the round state and timers.
3. Dead players stay out until restart.
4. The round ends when one configured team has no living players left.
5. Use `exp_round_restart` for a forced reset or `exp_round_stop` to return to plain deathmatch.
6. If you only have one real client available, `exp_team_fake_add <team>` can be used as a local verification helper for the team-elimination loop.

Example freeze-phase buy loop:

```text
exp_cfg_apply editor_357_test.cfg
sv_exp_round_mode 1
sv_exp_team_round_mode 1
sv_exp_buy_mode 1
sv_exp_buy_freeze_only 1
sv_exp_buy_start_money 2500
sv_exp_team_round_team1_loadout none
sv_exp_team_round_team2_loadout none
exp_team_join Poni alpha
exp_team_status
exp_round_start
exp_buy_list
exp_buy 357 Poni
```

During a buy-enabled round session:

1. Use `exp_buy_list` to inspect allowed weapons and costs.
2. Use `exp_buy_status` to inspect current money, selected weapon, and whether buy phase is open.
3. Buy during freeze with `exp_buy <weapon> [player]`.
4. When buying succeeds, the selected weapon is granted immediately during freeze and becomes that player's deterministic round loadout override for the current round.
5. When the round becomes live, freeze-only buy mode rejects further purchases with an explicit reason instead of silently ignoring them.
6. On the next round freeze, the selected weapon resets and round rewards are applied to the player's money.

Example persisted team-spawn setup:

```text
sv_exp_round_mode 1
sv_exp_team_round_mode 1
sv_exp_team_round_spawn_mode manual_spots
exp_team_join Poni alpha
exp_team_spawn_mark alpha default
exp_team_join Poni bravo
exp_team_spawn_mark bravo default
exp_team_spawn_use alpha default
exp_team_spawn_use bravo default
exp_round_restart
```

## Persistent team spawn spots

Saved team spawns live under:

- `<HalfLifeRoot>\hlserver_testbed\team_spawns\<map>.json`

The file is human-readable JSON. It stores:

- two team buckets
- the active saved spawn name for each team
- one or more named spawn records per team
- origin and yaw for each saved spawn
- creation/update timestamps

Round-start spawn resolution in team mode with `sv_exp_team_round_spawn_mode manual_spots` is:

1. active saved spawn for the player's team
2. `default` saved spawn for the player's team
3. normal deathmatch map spawn fallback

If a saved spawn is blocked or invalid, the server logs the failure reason and the fallback source instead of silently ignoring it.

## Notes

- With one human player, the mode still works for solo validation. The round goes live, and the only player must be eliminated before the restart path triggers.
- In team mode, one human player is enough to verify status and waiting behavior, but you still need players on both teams for a real last-team-alive round result.
- Round loadout reset is server-side and deterministic. It reuses the same experimental weapon surfaces already driven by cfg files.
- Team loadout and start health/armor inherit from the global round cvars unless the team-specific overrides are set.
- Team mode can now use persisted per-team saved spawns. If none are available, or if a saved spawn is blocked, the server falls back to the normal map spawn logic and records that fallback in status and telemetry.
- The buy prototype is intentionally small: console-driven, server-side, deterministic, and limited to the four experimental weapons. There is still no client buy UI, inventory polish, armor shop, utility shop, or full Counter-Strike economy model.
- `exp_team_fake_add` and `exp_team_fake_clear` exist only as small server-side validation helpers for local team-round testing when a second human is not available.
- Existing target dummy, cfg apply, and persistent saved-spot workflows remain available when round mode is off.
