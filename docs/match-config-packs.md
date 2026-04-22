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

During the normal `hlserver_testbed` refresh path, those checked-in starter packs are copied into the live mod and preserved alongside user-created pack files.

## File format

Each pack is one JSON file per named mode, for example:

- `duel_glock.json`
- `duel_357.json`
- `team_mp5_buy.json`
- `team_shotgun.json`
- `armor_test.json`
- `aim_lab.json`

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

## Starter packs

The checked-in starter set is intentionally practical rather than authoritative:

- `duel_glock`
- `duel_357`
- `team_mp5_buy`
- `team_shotgun`
- `armor_test`
- `aim_lab`

They are meant as quick reusable starting points for the gameplay systems already present:

- duel round mode
- team round mode
- deterministic loadouts
- buy prototype
- armor and helmet prototype
- live-lab target testing

They are not claims of final balance.

## Editor integration

The native C++ editor now understands pack metadata on the `General` page:

- `Pack name`
- `Description`
- `Tags`

When those fields are present and the cfg is exported into `<HalfLifeRoot>\hlserver_testbed\`, the editor writes a sidecar JSON file into `match_packs\` next to the runtime cfg workflow. That gives one project two compatible entry points:

- plain cfg: `exp_cfg_apply my_match.cfg`
- named pack: `exp_matchcfg_apply my_pack`

The editor self-test now verifies this sidecar path too.

## Recommended workflow

1. Run `exp_matchcfg_list`.
2. Apply a starter pack such as `exp_matchcfg_apply duel_glock` or `exp_matchcfg_apply team_mp5_buy`.
3. Inspect the applied state with `exp_matchcfg_status` and `exp_cfg_status`.
4. If you want to tweak the mode, open the editor project, change values, export a new cfg, and optionally export pack metadata from the editor.
5. Reapply with either `exp_matchcfg_apply <name>` or `exp_cfg_apply <cfg>`.

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
