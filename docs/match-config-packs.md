# Match Config Packs

Match config packs are a thin usability layer over the existing cfg workflow. They are not a second gameplay system. Each pack is simple human-readable JSON metadata that points at a real cfg file the server already knows how to execute.

## Location

Runtime pack files live under:

```text
<HalfLifeRoot>\hlserver_testbed\match_packs\
```

The repository also ships starter packs under:

```text
<repo-root>\configs\match-packs\
```

During the normal `hlserver_testbed` refresh path, those checked-in starter packs are copied into the live mod and preserved alongside user-created pack files. The disposable testbed installer now syncs the same checked-in packs into `testbed\runtime\valve\match_packs\`, so `scripts\run-server.ps1` can apply the same names without a manual copy step.

## File format

Each pack is one JSON file per named mode, for example:

- `duel_glock.json`
- `duel_357.json`
- `team_mp5_buy.json`
- `team_shotgun.json`
- `armor_test.json`
- `aim_lab.json`
- `hldm_skill_default.json`

Current fields are intentionally small:

- `name`
- `description`
- `cfg`
- `weapon_under_test`
- `target_profile`
- `mode`
- `tags`
- `notes`

Example:

```json
{
  "name": "duel_glock",
  "description": "1v1 no-respawn Glock duel with a tighter first-shot profile.",
  "cfg": "match_packs/duel_glock.cfg",
  "weapon_under_test": "glock",
  "target_profile": "",
  "mode": "duel_round",
  "tags": "duel,glock,round",
  "notes": "Uses direct round loadout mode with buy and team rules disabled."
}
```

The important design point is that packs still resolve to real cfg execution. That keeps them compatible with:

- `exec`
- `exp_cfg_apply`
- `exp_lab_apply`
- cfg-driven live launch with `-CfgProfile` or `-CfgPath`

Because the underlying runtime still consumes normal cfg files, packs can also carry newer match-progression cvars such as:

- `sv_exp_match_mode`
- `sv_exp_match_rounds_to_win`
- `sv_exp_match_max_rounds`
- `sv_exp_match_enable_halftime`
- `sv_exp_match_halftime_after_round`
- `sv_exp_match_side_swap`
- `sv_exp_match_reset_money_on_halftime`
- `sv_exp_match_reset_loadout_on_halftime`
- `sv_exp_match_auto_restart_after_end`
- `sv_exp_match_end_delay`

That means packs can describe a whole reusable duel, team, buy, armor, or match-progression setup without any pack-system redesign.

Packs can also be simpler than a match. The `hldm_*` packs are deliberately deathmatch-oriented cfg bundles that leave round, team, buy, and match progression off while applying the current weapon-feel tuning.

## Commands

The runtime adds three high-level commands:

- `exp_matchcfg_list`
- `exp_matchcfg_apply <name>`
- `exp_matchcfg_status`

Behavior:

- `exp_matchcfg_list` scans `match_packs\` and prints the available packs plus description/metadata.
- `exp_matchcfg_apply <name>` loads the JSON metadata, resolves the referenced cfg, applies it through the normal cfg path, then records active-pack metadata for later debugging.
- `exp_matchcfg_status` prints the current active pack, applied cfg, pack file path, and descriptive metadata.

If you later want to leave pack mode and go back to a direct cfg workflow, run:

```text
exp_cfg_apply my_match.cfg
```

or legacy:

```text
exec my_match.cfg
```

That direct cfg apply remains the lower-level fallback and is not replaced by packs.

## Weapon sandbox reuse

Match packs can also be used as a sandbox source. This keeps the pack metadata and cfg resolution path intact while adding a one-command weapon-plus-dummy reset:

```text
exp_sandbox_start
exp_sandbox_weapon mp5
exp_sandbox_pack hldm_mp5_burst
exp_sandbox_target vest_headprotected
exp_sandbox_spot default
exp_sandbox_reset
```

`exp_sandbox_pack <name>` selects and applies the same pack that `exp_matchcfg_apply <name>` would apply. `exp_sandbox_reset` then enables debug weapon logging, grants the selected weapon to the first live player, applies the selected target profile, uses the selected saved target spot when available, and respawns the dummy. The sandbox is a productivity layer over the existing cfg and pack commands, not a separate pack format.

## Starter packs

The checked-in starter set is intentionally practical rather than authoritative:

- `hldm_skill_default`
- `hldm_precision_duel`
- `hldm_mp5_burst`
- `hldm_shotgun_control`
- `hldm_headshot_lab`
- `duel_glock`
- `duel_357`
- `team_mp5_buy`
- `team_shotgun`
- `armor_test`
- `aim_lab`

They are meant as quick reusable starting points for the gameplay systems already present:

- improved stock-client-compatible HLDM weapon feel
- duel round mode
- team round mode
- deterministic loadouts
- buy prototype
- armor and helmet prototype
- match progression when the referenced cfg includes `sv_exp_match_*`
- live-lab target testing

They are not claims of final balance.

## Improved HLDM packs

The recommended everyday pack is:

```text
exp_matchcfg_apply hldm_skill_default
```

It is the default Improved HLDM recommendation because it:

- keeps normal deathmatch flow
- keeps round, team, buy, match progression, armor, and dummy systems off
- enables the current Glock cadence/pattern work
- enables MP5 controlled-burst and deterministic pattern behavior
- enables 357 cadence/pattern precision behavior
- enables shotgun deterministic pellet behavior

The focused Improved HLDM variants are:

- `hldm_precision_duel`
  More deliberate Glock/357 timing for precision sidearm testing inside normal deathmatch.
- `hldm_mp5_burst`
  MP5-focused controlled-burst tuning with deterministic early spray and stronger long-spray bloom.
- `hldm_shotgun_control`
  Shotgun-focused deterministic pellet layout and tighter repeated-shot control.
- `hldm_headshot_lab`
  Verification pack that turns debug logging and the protected-head dummy on for headshot/armor telemetry checks.

These packs are intentionally not full Counter-Strike modes. They package the current weapon-feel work for HLDM-style play while leaving the advanced round/team/buy systems available as separate packs.

## Editor integration

The native C++ editor now understands pack metadata on the `General` page:

- `Pack name`
- `Description`
- `Tags`

When those fields are present and the cfg is exported into `<HalfLifeRoot>\hlserver_testbed\`, the editor writes a sidecar JSON file into `match_packs\` next to the runtime cfg workflow. That gives one project two compatible entry points:

- plain cfg: `exp_cfg_apply my_match.cfg`
- named pack: `exp_matchcfg_apply my_pack`

The editor self-test now verifies this sidecar path too.

The editor now also has a dedicated `Browser` tab for the pack layer:

- it lists the available runtime packs from `<HalfLifeRoot>\hlserver_testbed\match_packs\`
- selecting a pack shows description, tags, notes, referenced cfg path, and the leading cvars from that cfg
- the diff pane compares the current editor project against the incoming pack cvars
- `Load Into Current Project` merges that pack into the open project
- `Copy Apply Command` then gives the exact runtime command for the loaded project

That means the pack layer can now be reused from inside the editor without leaving the existing `.hlcfg.json -> .cfg -> apply command` workflow.

## Recommended workflow

1. Run `exp_matchcfg_list`.
2. Apply a starter pack such as `exp_matchcfg_apply hldm_skill_default`, `exp_matchcfg_apply duel_glock`, or `exp_matchcfg_apply team_mp5_buy`.
3. Inspect the applied state with `exp_matchcfg_status` and `exp_cfg_status`.
4. If you want to tweak the mode, open the editor project, change values, export a new cfg, and optionally export pack metadata from the editor.
5. Reapply with either `exp_matchcfg_apply <name>` or `exp_cfg_apply <cfg>`.

If you want a pack to represent a whole match instead of just a round/buy baseline:

1. Export a cfg that already contains the desired `sv_exp_match_*` values.
2. Point the pack JSON at that cfg.
3. Apply it through `exp_matchcfg_apply <name>`.

The pack layer does not need any special embedded match logic beyond that referenced cfg.

## Verified state

One real client-attached session on `2026-04-22` verified:

- pack listing from `exp_matchcfg_list`
- duel-pack apply through `exp_matchcfg_apply duel_glock`
- team-pack apply through `exp_matchcfg_apply team_mp5_buy`
- active-pack status output
- preservation of the older plain cfg path via `exp_cfg_apply editor_buy_armor.cfg`

The strongest runtime proof is in:

- `D:\Steam\steamapps\common\logs\weapon-debug-20260422-181420.log`

That log captures the pack list, the applied pack metadata, the resulting round/team/buy state changes, and the real connected player in the same live session.
