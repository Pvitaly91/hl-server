# Disposable testbed

## Goal

The testbed exists to run HLDS with the locally built `hl.dll` without modifying the user's real Steam install.

## Layout

- `testbed/runtime/` - disposable mirrored runtime used for execution
- `testbed/logs/` - launch logs captured by the scripts
- `testbed/cache/` - reusable cache for SteamCMD downloads and optional HLDS template installs

## Runtime source selection

`install-testbed.ps1` and `doctor-testbed.ps1` resolve a template root in this order:

1. explicit script parameter
2. `.env` / process environment override
3. explicit or environment `HLDS_EXE`
4. explicit or environment `HL_EXE` when that root also contains `hlds.exe`
5. cached dedicated template under `testbed/cache/hlds-template`
6. installed `Half-Life Dedicated Server`
7. regular Half-Life client install only as a fallback
8. SteamCMD-driven dedicated-template provisioning when repair or explicit download is allowed

The scripts do not write back into the original template root. They mirror the selected source into `testbed/runtime/`, validate key executable-side dependencies, generate `steam_appid.txt` inside the disposable runtime when the source type makes the AppID unambiguous, then install the built DLL into `testbed/runtime/valve/dlls/`.

## HLDS launch behavior

`run-server.ps1` launches the disposable runtime as:

- `-game valve`
- default map `crossfire`
- LAN-friendly defaults for local iteration
- log capture into `testbed/logs/`

It also records a small `hlds-*-launch.txt` file so the working directory, executable path, Steam AppID, and exact launch arguments are visible after the fact.

That preserves stock client compatibility while still swapping in the custom server DLL.

## Smoke test signal

The smoke test confirms startup by searching the server logs for the placeholder `sv_exp_*` cvars registered from `src/future_gameplay_hooks.cpp` during `GameDLLInit`. That proves the server started far enough to load the custom `hl.dll`, not merely that the binary exists on disk.
