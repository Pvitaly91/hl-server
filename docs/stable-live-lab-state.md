# Stable Live-Lab Integration State

This note records the subsystem selections intentionally carried into `integration/stable-live-lab`.

## Selected source commits

- Live launch path: `fix/live-same-root-map-match` at `536ff76` (`Route live BAT launchers through the direct same-root path`)
- Editor deployment and direct live-mod export workflow: `feature/editor-one-step-workflow` at `513e3ff` (`Simplify config editor workflow for direct mod usage`)
- Editor Export tab fixes: `fix/editor-export-ui` at `bf4c6cc` (`Fix config editor export tab button handling`)
- Cfg-driven live commands: `feature/live-lab-cfg-commands` at `e1723a3` (`Add live cfg control and target reset commands`)
- Reliable target placement and respawn: `feature/reliable-target-placement` at `3129a1f` (`Add reliable target anchors and safe respawn placement`)

## Integration strategy

- The branch families above form a linear history in the local repository.
- `3129a1f` already includes the older live-launch, editor, export-fix, and cfg-command work.
- `integration/stable-live-lab` therefore starts from `3129a1f` instead of replaying redundant merges.
- This branch adds a documentation-level recommendation that this combined path is the preferred base for future work.

## Verified integrated workflow

- Build the C++ editor in Visual Studio 2022 or with the existing CMake/VS build path.
- Use the deployed `HlConfigEditorCpp.exe` from `hlserver_testbed`.
- Quick-export a cfg directly into `hlserver_testbed`.
- Launch live mode with the direct same-root testbed path.
- Apply cfg changes with `exp_cfg_apply` or `exp_lab_apply`.
- Rebuild the dummy with `exp_target_respawn` and inspect it with `exp_target_status`.

## Still manual

- Gameplay tuning and balance validation remain manual.
- Cross-map confidence still needs manual spot checks beyond the verified live run.
