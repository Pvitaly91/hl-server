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
- A small server-only future-hooks stub for later experiments such as pistol tap-fire and CS-like spread behavior.

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
.\scripts\run-server.ps1 -Configuration Debug
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
- `testbed/logs/` - server launch logs
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
- `scripts/run-server.ps1` launches HLDS against the disposable runtime with `-game valve` and defaults to `crossfire`.
- `scripts/run-client.ps1` optionally launches the stock Half-Life client and connects to `127.0.0.1`.
- `scripts/smoke-test.ps1` validates the end-to-end bootstrap non-interactively.
- `scripts/clean-testbed.ps1` removes generated build and disposable runtime artifacts.

## What is intentionally not committed

- Steam files and registries
- Half-Life or HLDS binaries
- proprietary game assets
- generated build outputs
- disposable testbed runtime and logs
- local `.env` overrides

## Future direction

This repository is aimed at future server-side gameplay experiments such as pistol tap-fire and CS-like spread tuning. The current bootstrap only establishes the hook path and safe runtime loop; it does not yet change vanilla gameplay.

See:

- `docs/upstream.md`
- `docs/testbed.md`
- `docs/future-gameplay-hooks.md`
