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

For no-client or dedicated-only flows, that means the cached dedicated template can remain the preferred executable source. For live client-attached flows, the repo now has an explicit client-matched mode: it resolves the stock `hl.exe` root, mirrors that client root into `testbed/runtime/`, and then supplements missing server-side files from the selected HLDS-capable source when needed. That keeps `-game valve` while making the runtime content match the launched stock client.

## HLDS launch behavior

`run-server.ps1` launches the disposable runtime as:

- `-game valve`
- default map `crossfire`
- LAN-friendly defaults for local iteration
- log capture into `testbed/logs/`

It also records a small `hlds-*-launch.txt` file so the working directory, executable path, Steam AppID, and exact launch arguments are visible after the fact.

That preserves stock client compatibility while still swapping in the custom server DLL.

## Live BAT launchers

For Explorer or `cmd.exe` usage, the repo now includes thin BAT wrappers that call the existing PowerShell flows without duplicating the launch logic:

- `scripts\play-glock-live.bat`
- `scripts\play-mp5-live.bat`
- `scripts\play-live-test.bat`
- `scripts\show-latest-log-analysis.bat`
- `scripts\show-latest-glock-analysis.bat`
- `scripts\show-latest-mp5-analysis.bat`

The live launchers always:

- run `doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair -BuildIfMissing` first
- keep the disposable runtime on `-game valve`
- print the chosen client root, runtime source root, content source root, chosen map, `testbed/logs/`, and `testbed/logs/reports/`
- require a stock `hl.exe` for client-attached play unless `-NoClient` is explicitly forwarded
- delegate the actual session startup to `run-glock-test-session.ps1` or `run-mp5-test-session.ps1`

Default live presets:

- Glock: `cs_tight` against `vest_headprotected`
- MP5: `cs_burst` against `vest`

You can override those defaults from `cmd.exe`, for example:

```bat
scripts\play-glock-live.bat -Preset cs_mobile -TargetProfile unarmored
scripts\play-mp5-live.bat -Preset cs_mobile -TargetProfile vest_headprotected
```

The menu wrapper exposes common combinations directly:

```bat
scripts\play-live-test.bat
```

The existing dispatcher also exposes the live launchers:

```bat
scripts\run-testbed.bat play-glock
scripts\run-testbed.bat play-mp5
scripts\run-testbed.bat play-glock-clientmatched
scripts\run-testbed.bat play-mp5-clientmatched
scripts\run-testbed.bat play-menu
```

If the repo cannot find a stock `hl.exe`, the live BAT helper fails before the session hand-off and tells you to set `HL_EXE` in `.env` or pass `-HlExe`.

You can inspect the current client/runtime pairing without launching a session:

```powershell
.\scripts\check-live-map-match.ps1
.\scripts\check-live-map-match.ps1 -PreferClientMatchedRuntime
```

## Smoke test signal

The smoke test confirms startup by searching the server logs for the placeholder `sv_exp_*` cvars registered from `src/future_gameplay_hooks.cpp` during `GameDLLInit`. That proves the server started far enough to load the custom `hl.dll`, not merely that the binary exists on disk.
