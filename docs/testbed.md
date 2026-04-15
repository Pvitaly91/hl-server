# Disposable testbed

## Goal

The testbed exists to run HLDS with the locally built `hl.dll` without modifying the user's real Steam install.

## Layout

- `testbed/runtime/` - disposable mirrored runtime used for execution
- `testbed/logs/` - launch logs captured by the scripts
- `testbed/cache/` - reusable cache for SteamCMD downloads and optional HLDS template installs

## Runtime source selection

`install-testbed.ps1` resolves a template root in this order:

1. explicit script parameter
2. `.env` / process environment override
3. auto-detected `HLDS_EXE`
4. SteamCMD-driven template provisioning when explicitly allowed

The script does not write back into the original template root. It mirrors the template into `testbed/runtime/`, then installs the built DLL into `testbed/runtime/valve/dlls/`.

## HLDS launch behavior

`run-server.ps1` launches the disposable runtime as:

- `-game valve`
- default map `crossfire`
- LAN-friendly defaults for local iteration
- log capture into `testbed/logs/`

That preserves stock client compatibility while still swapping in the custom server DLL.

## Smoke test signal

The smoke test confirms startup by searching the server logs for the placeholder `sv_exp_*` cvars registered from `src/future_gameplay_hooks.cpp` during `GameDLLInit`. That proves the server started far enough to load the custom `hl.dll`, not merely that the binary exists on disk.
