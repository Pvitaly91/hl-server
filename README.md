# hl-server

`hl-server` bootstraps an empty repository into a Windows-first, VS2022-ready server-side Half-Life workspace for gameplay experiments that must stay compatible with the stock Steam Half-Life client.

The repository vendors a pinned snapshot of Valve's official Half-Life source base under `third_party/valve-halflife-sdk/`, builds only the server-side GameDLL (`hl.dll`) first, and keeps all runtime edits inside a disposable local testbed under `testbed/`.

## Why the official Valve SDK

- The compatibility baseline is Valve's official `ValveSoftware/halflife` repository, pinned and vendored directly in-tree.
- No TWHL Updated, Unified SDK, Xash3D, ReGameDLL, or other compatibility-diverging fork is used.
- The disposable runtime still launches as `-game valve`, so a stock Steam Half-Life client can connect without a custom client DLL.

## What this repo is today

- A reproducible VS2022 Win32 build for the vanilla Half-Life server GameDLL.
- A disposable HLDS test stand that mirrors a user-supplied or SteamCMD-provisioned runtime into `testbed/runtime/`.
- A server-only experimental Glock path for tap-fire, movement-dependent spread, and optional first-shot accuracy without changing the stock client DLL.

It is not yet a gameplay conversion and it does not ship any proprietary game assets, Steam files, or HLDS binaries.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 with Desktop development with C++
- Windows SDK
- PowerShell 5.1 or newer
- Git
- One of:
  - an existing Half-Life Dedicated Server install
  - an existing Half-Life install that already contains `hlds.exe`
  - SteamCMD, or permission to let the scripts download SteamCMD into `testbed/cache/`

## Quick start

```powershell
.\scripts\bootstrap.ps1
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\install-testbed.ps1 -Configuration Debug -AllowSteamCmdDownload
.\scripts\run-server.ps1 -Configuration Debug -Detached
```

To smoke-test the whole flow non-interactively:

```powershell
.\scripts\smoke-test.ps1 -Configuration Debug -AllowSteamCmdDownload
```

To build Release instead:

```powershell
.\scripts\build.ps1 -Configuration Release
.\scripts\install-testbed.ps1 -Configuration Release -AllowSteamCmdDownload
```

## Experimental Glock mode

The current gameplay pass is server-only and limited to Glock primary fire. It keeps the disposable runtime on `-game valve` and stays compatible with the stock Steam Half-Life client, but the altered tap-fire cadence and spread logic can feel slightly different from local client prediction because no client DLL code is changed.

Build and install:

```powershell
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\install-testbed.ps1 -Configuration Debug -AllowSteamCmdDownload
```

Launch the disposable HLDS runtime in vanilla mode with PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached
```

Launch the disposable HLDS runtime in experimental Glock mode with PowerShell:

```powershell
.\scripts\run-server.ps1 -Configuration Debug -Detached -EnableExperimentalGlock
```

Launch the same disposable runtime from `cmd.exe` or Explorer with the BAT wrapper:

```bat
scripts\run-testbed.bat
```

Enable the experimental Glock mode from the BAT wrapper:

```bat
scripts\run-testbed.bat experimental
```

Enable the experimental Glock mode with debug telemetry and dedicated weapon logging:

```bat
scripts\run-testbed.bat experimental-debug
```

Launch the one-click manual Glock session:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug
```

Launch the same session and analyze the latest Glock telemetry log after you finish the manual firing pass and press Enter in the original console:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -AnalyzeLatestOnExit
```

Launch the same one-click session from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-session
```

Forward extra launch arguments through the BAT wrapper when needed:

```bat
scripts\run-testbed.bat experimental -Port 27016 -Map crossfire
```

The one-click Glock session always:

- reinstalls the selected disposable runtime into `testbed/runtime/`
- launches HLDS in experimental-debug mode on `-game valve`
- waits until the server actually reports `Started map "<map>"`
- opens `scripts/tail-weapon-log.ps1` in a second PowerShell window when the session weapon log is ready, if possible
- launches a stock `hl.exe` and auto-connects to `127.0.0.1:<port>` unless `-NoClient` is set
- prints a compact in-terminal checklist for the manual firing pass

The session prefers `testbed/runtime/hl.exe` when the mirrored runtime contains a stock client executable, which keeps client-side writes inside the disposable testbed. If the runtime does not contain `hl.exe`, it falls back to the same `HL_EXE` and Steam auto-detection path used by `scripts/run-client.ps1`. If no stock client is found, the script leaves a server-only session running and prints a clear message instead of failing.

Run a server-only one-click session when you only want readiness and telemetry validation:

```powershell
.\scripts\run-glock-test-session.ps1 -Configuration Debug -NoClient
```

Tail the newest disposable weapon debug log in PowerShell:

```powershell
.\scripts\tail-weapon-log.ps1
```

Read the newest disposable weapon debug log once without following it:

```powershell
.\scripts\tail-weapon-log.ps1 -NoFollow
```

Run the non-interactive smoke test with the same experimental path:

```powershell
.\scripts\smoke-test.ps1 -Configuration Debug -AllowSteamCmdDownload -EnableExperimentalGlock
```

Telemetry stays fully quiet by default. `scripts\run-testbed.bat experimental-debug` enables:

- `sv_exp_debug_weaponlog 1`
- `sv_exp_debug_weaponlog_rejections 1`

Accepted Glock primary shots log one grep-friendly server-side line to both the server console and `testbed/logs/weapon-debug-<timestamp>.log`, for example:

```text
[weaponlog] type=accepted ts=2026-04-16T21:24:55.314 map=crossfire player="Player" entindex=1 userid=2 weapon=glock fire=primary experimental=1 tapfire=1 firstshot=1 spread=0.0000 base=0.0100 move_penalty=0.0000 speed2d=18.4 maxspeed=270.0 grounded=1 ducking=0 delta_prev=0.421 clip=16
```

Blocked tap-fire hold attempts log only when rejection logging is also enabled, for example:

```text
[weaponlog] type=rejected ts=2026-04-16T21:24:56.002 map=crossfire player="Player" entindex=1 userid=2 weapon=glock fire=primary reason=tapfire_hold_blocked speed2d=18.4 grounded=1 ducking=0
```

Each session also starts with a single header line:

```text
[weaponlog] type=session ts=2026-04-16T21:24:54.901 map=crossfire event=weapon_debug_session status=ready game=valve file="d:/dev/cpp/hl-server/testbed/logs/weapon-debug-20260416-212454.log"
```

## Analyze Glock telemetry

Analyze the newest disposable weapon log:

```powershell
.\scripts\analyze-weapon-log.ps1
```

Analyze a specific log file:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\testbed\logs\weapon-debug-20260416-114837.log
```

Export the summary JSON and per-event CSV to `testbed/logs/reports/`:

```powershell
.\scripts\analyze-weapon-log.ps1 -Latest -ExportJson -ExportCsv
```

Use the BAT alias from `cmd.exe` or Explorer:

```bat
scripts\run-testbed.bat glock-report
scripts\run-testbed.bat glock-report -ExportJson -ExportCsv
```

Assert that a manual run exercised the expected signals:

```powershell
.\scripts\analyze-weapon-log.ps1 -Path .\scripts\fixtures\sample-weapon-debug.log -RequireAccepted -RequireRejections -RequireFirstShot -RequireMovePenalty -RequireCrouchMoveEvidence
```

Example summary:

```text
Weapon log analysis
  log path                 : D:\DEV\CPP\HL-Server\scripts\fixtures\sample-weapon-debug.log
  session                  : ts=2026-04-16T12:00:00.000 map=fixture_range game=valve status=ready
  accepted shots           : 5
  rejected shots           : 1
  first-shot accepted      : 2
  accepted move_penalty>0  : 3
  accepted grounded        : 4
  accepted airborne        : 1
  accepted ducking         : 1
  spread min/avg/max       : 0.0000 / 0.0487 / 0.1300
```

The analyzer summarizes server-authoritative telemetry evidence only. It helps confirm that accepted shots, tap-fire hold rejections, movement penalties, recovery, and crouch-move candidates were logged, but it does not prove subjective stock-client feel or prediction quality.

## Pointing the scripts at existing installs

Copy `.env.example` to `.env` and fill in any paths you want to pin:

```powershell
Copy-Item .env.example .env
```

Supported variables:

- `HLDS_EXE` - full path to `hlds.exe`
- `HL_EXE` - full path to `hl.exe`
- `STEAMCMD_EXE` - full path to `steamcmd.exe`
- `HL_RUNTIME_TEMPLATE` - root folder to mirror into `testbed/runtime/`

You can also pass explicit script parameters instead of using `.env`.

The scripts auto-detect common Steam install locations and the Steam install path from the Windows registry when possible. If auto-detection fails, they stop with a clear message instead of guessing unsafe locations.

## Build outputs

The CMake preset is the source of truth.

- Configure preset: `vs2022-win32`
- Build directory: `build/vs2022-win32/`
- Generated VS solution after configure: `build/vs2022-win32/hl_server.sln`
- DLL output:
  - `artifacts/Debug/hl.dll`
  - `artifacts/Release/hl.dll`

## Disposable testbed layout

- `testbed/runtime/` - disposable HLDS runtime mirror
- `testbed/logs/` - `hlds-*.log` server launch logs and `weapon-debug-*.log` Glock telemetry logs
- `testbed/cache/` - downloaded SteamCMD and optional HLDS template cache

`install-testbed.ps1` copies the selected runtime template into `testbed/runtime/` and then overwrites only:

- `testbed/runtime/valve/dlls/hl.dll`
- `testbed/runtime/valve/dlls/hl.pdb` when available

No script writes into the user's real Steam `valve` folder.

## Script reference

- `scripts/bootstrap.ps1` verifies prerequisites, ensures the vendored SDK and docs are present, and writes `.env.example`.
- `scripts/configure.ps1` configures the VS2022 Win32 CMake preset.
- `scripts/build.ps1` builds Debug or Release and prints the resulting artifact paths.
- `scripts/install-testbed.ps1` prepares the disposable runtime and installs the built `hl.dll`.
- `scripts/run-server.ps1` launches HLDS against the disposable runtime with `-game valve`, defaults to `crossfire`, accepts `-EnableExperimentalGlock` and `-EnableExperimentalGlockDebug` for the documented server-only Glock cvar bundles, and still supports launch-time overrides through `-SetCvar @('name=value', ...)` or `-Cvars @{ name = 'value' }`.
- `scripts/run-glock-test-session.ps1` reinstalls the disposable runtime, launches the experimental-debug Glock server, waits for readiness, optionally opens a tail window for the exact weapon log, optionally launches a stock client, prints the manual checklist with the connect address and log paths, and can hand off to the analyzer with `-AnalyzeLatestOnExit`.
- `scripts/analyze-weapon-log.ps1` analyzes the newest or a specific `weapon-debug-*.log`, prints a concise evidence summary, optionally exports JSON and CSV under `testbed/logs/reports/`, and can fail non-zero when required telemetry signals are missing.
- `scripts/run-testbed.bat` is a thin convenience wrapper over the PowerShell scripts that defaults to a detached Debug disposable launch, maps `experimental` and `experimental-debug` to the corresponding server switches, maps `glock-session` to the one-click manual session helper, and maps `glock-report` to `scripts/analyze-weapon-log.ps1`.
- `scripts/tail-weapon-log.ps1` finds the newest `testbed/logs/weapon-debug-*.log` file, or follows a specific log passed through `-Path`, prints a helpful message if none exists, and can either follow the log or dump it once with `-NoFollow`.
- `scripts/run-client.ps1` optionally launches a stock Half-Life client on `-game valve` and connects to `127.0.0.1`.
- `scripts/smoke-test.ps1` validates the end-to-end bootstrap non-interactively and can verify experimental cvar values from launch-time overrides.
- `scripts/clean-testbed.ps1` removes generated build and disposable runtime artifacts.

## What is intentionally not committed

- Steam files and registries
- Half-Life or HLDS binaries
- proprietary game assets
- generated build outputs
- disposable testbed runtime and logs
- local `.env` overrides

## Future direction

This repository is aimed at iterative server-side gameplay experiments that keep the stock Steam Half-Life client compatible. The first pass is Glock-only; future work can extend the same seam to additional weapons without turning the project into a custom client or full conversion.

See:

- `docs/upstream.md`
- `docs/testbed.md`
- `docs/future-gameplay-hooks.md`
