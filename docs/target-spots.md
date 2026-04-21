# Persistent Target Spots

Persistent live target spots are stored under:

```text
<HalfLifeRoot>\hlserver_testbed\target_spots\<map>.json
```

Each map gets one JSON file. The file stores:

- `active_spot`
- one or more named saved spots
- each spot's `origin`
- each spot's `yaw`
- placement metadata such as `candidate`
- `created_at` and `updated_at` timestamps

Example:

```json
{
  "map": "crossfire",
  "active_spot": "default",
  "spots": [
    {
      "name": "default",
      "origin": [1024.0, -256.0, 64.0],
      "yaw": 180.0,
      "candidate": "current_dummy",
      "created_at": "2026-04-21T16:24:12.031",
      "updated_at": "2026-04-21T16:24:12.031"
    }
  ]
}
```

## Commands

- `exp_target_mark [name]`
  Saves or updates a named spot for the current map. Omitting the name writes `default`.
- `exp_target_unmark [name]`
  Removes a named spot. Omitting the name removes `default`.
- `exp_target_list`
  Lists all named spots for the current map.
- `exp_target_use_saved <name>`
  Selects the active named spot for the next respawn.
- `exp_target_status`
  Shows target state, active saved spot, saved spot names, file/load state, and respawn viability.
- `exp_target_respawn`
  Rebuilds the dummy, preferring the active saved spot first, then the `default` spot when no active spot is selected.

## Recommended Flow

First setup on a map:

```text
exp_target_mark default
exp_target_profile vest_headprotected
exp_target_respawn
```

Later sessions on the same map:

```text
exp_cfg_apply editor_glock_simple.cfg
exp_target_use_saved default
exp_target_respawn
```

If respawn fails, run `exp_target_status` and check the saved-spot file path, active spot name, anchor state, and last failure reason before remarking the spot.
