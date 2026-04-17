# Testbed runtime troubleshooting

## What this doc covers

This repository keeps runtime writes inside `testbed/runtime/`, but HLDS still depends on a valid local source runtime outside the repo. The new doctor and repair tooling exists to make that dependency visible instead of leaving failures as opaque `hlds.exe` crashes.

## Symptoms the doctor now diagnoses

The doctor is aimed at the Windows startup failures that previously blocked the disposable testbed flow, including:

- `FATAL ERROR (shutting down): Unable to initialize Steam.`
- `Assertion Failed: Failed to load "SDL3.dll"`
- missing executable-side DLLs after a disposable runtime refresh
- a runtime refresh blocked because `testbed/runtime` is still in use by a live `hlds.exe` or `hl.exe`
- a stale disposable runtime that was mirrored from an older source root than the one the scripts would choose now
- a regular Half-Life client install being used only because no dedicated HLDS-capable source was available
- `Your map [maps/crossfire.bsp] differs from the server's.`
- `crossfire different map` or another stock-map mismatch during a client-attached live connect

## How to run the doctor

Inspect the current source selection and the disposable runtime:

```powershell
.\scripts\doctor-testbed.ps1
```

Refresh the disposable runtime and let the repo provision or refresh a cached dedicated HLDS template under `testbed/cache/`:

```powershell
.\scripts\doctor-testbed.ps1 -Repair
```

Inspect or repair the runtime for a live client-attached session:

```powershell
.\scripts\check-live-map-match.ps1
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair
```

Use the thin BAT alias:

```bat
scripts\run-testbed.bat doctor
```

## What the doctor checks

For the currently selected source runtime, the doctor prints:

- the selected source root
- why that source was chosen
- the selected `hlds.exe`
- the selected `hl.exe` when one exists
- the actual disposable runtime root
- the launched `testbed/runtime\hlds.exe`
- the launched client executable path that will be used for the live session
- the resolved stock client root used for live client-attached sessions
- the effective content source root that will be mirrored into `testbed/runtime/`
- `Same-root launch` and `content_match` as explicit `yes` / `no` verdicts
- the representative `valve\maps\crossfire.bsp` paths and SHA256 hashes for the runtime, the selected source, and the client root

For the disposable runtime under `testbed/runtime/`, the doctor prints:

- the runtime root
- the runtime manifest path
- the requested build configuration and `hl.dll` path
- key runtime-side files and directories that are found or missing
- the latest known startup blocker when that failure happened after the current disposable runtime was prepared

The key runtime-side entries checked today are:

- `hlds.exe`
- `hl.exe` when available
- `steam_appid.txt`
- `SDL2.dll`
- `SDL3.dll`
- `steam_api.dll`
- `tier0.dll`
- `vstdlib.dll`
- `platform/`
- `bin/` when present in the selected source
- `valve/`

## How source selection works

The runtime source chooser now prefers the best available HLDS-capable source in this order:

1. explicit `-TemplateRoot` or `HL_RUNTIME_TEMPLATE`
2. explicit `-HldsExe` or `HLDS_EXE`
3. explicit `-HlExe` or `HL_EXE` when that root also contains `hlds.exe`
4. the cached dedicated template under `testbed/cache/hlds-template`
5. an installed `Half-Life Dedicated Server` runtime under Steam
6. a regular Half-Life install only as a fallback when it already contains `hlds.exe`

That fallback matters. A Half-Life client install can be good enough for some local HLDS launches, but it is not the preferred long-term template. When the doctor has to fall back to it, it prints an explicit warning instead of silently treating it as equivalent to a dedicated-server runtime.

## What the disposable runtime copies

`install-testbed.ps1` and `doctor-testbed.ps1 -Repair` now:

1. choose a source runtime using the order above
2. refuse to refresh `testbed/runtime/` while a live testbed `hlds.exe` or `hl.exe` still has that tree open
3. mirror the selected source root into `testbed/runtime/`
4. validate executable-side dependencies after the mirror
5. repair missing key sidecars when the selected source provides them
6. generate `testbed/runtime/steam_appid.txt` when the selected source does not carry one but the source type makes the AppID unambiguous
7. install the locally built `hl.dll` into `testbed/runtime/valve/dlls/`
8. write `testbed/runtime/.hl-server-runtime.json` so stale mirrors are detectable later

When `-PreferClientMatchedRuntime` is requested, the copy policy changes intentionally for live play:

1. the repo resolves the stock `hl.exe` root that will be used for the live session
2. that client root becomes the mirror base for `testbed/runtime/`
3. the selected HLDS-capable source still supplies missing server-side files such as `hlds.exe` or server-only DLLs
4. the doctor records both the executable source root and the client content root in the runtime manifest

This is what prevents `Your map [maps/crossfire.bsp] differs from the server's.` when the dedicated template and the stock client would otherwise pull `valve` content from different bases.

The launch flow also now records a small `hlds-*-launch.txt` file under `testbed/logs/` that shows:

- the executable path
- the working directory
- the effective Steam AppID
- the exact launch arguments

That makes working-directory bugs diagnosable instead of implied.

## What still requires a valid external install

The repo can mirror and validate a source runtime, but it does not replace the need for a valid local Steam or HLDS installation context.

You still need at least one of:

- a valid Half-Life Dedicated Server install
- a valid Half-Life install that includes `hlds.exe`
- network access plus permission for `doctor-testbed.ps1 -Repair` to download SteamCMD into `testbed/cache/` and install app `90`

For client-attached testing, you also still need a valid stock `hl.exe` somewhere. The dedicated HLDS cache intentionally does not provide one, so the session helpers fall back to `HL_EXE` or the auto-detected Half-Life client install.

## Why `different map` happens

The symptom looks like:

- `Your map [maps/crossfire.bsp] differs from the server's.`
- or `crossfire different map`

That happens when the disposable server runtime and the launched stock client resolve `valve` content from different roots and at least one representative map file differs. Before this patch, the repo could legitimately prepare `testbed/runtime/` from `testbed/cache/hlds-template` while the stock client launched from a separate Half-Life install. If those `valve\maps` trees diverged, the connect failed even though both sides were still on `-game valve`.

The new same-root/client-matched live mode keeps the dedicated-template preference for no-client flows, but for live play it mirrors the stock client root into `testbed/runtime/` first and only supplements missing server-side files from the dedicated source. That keeps the runtime and the launched stock client aligned on the same `valve` content base.

## External blockers the repo cannot solve automatically

The new tooling is intentionally honest about the remaining boundary with the local machine. The repo cannot automatically repair:

- a broken Steam install outside the repo
- a missing or corrupted user-supplied `HLDS_EXE`, `HL_EXE`, or `HL_RUNTIME_TEMPLATE`
- SteamCMD download failures caused by network policy, DNS, firewall, or Valve-side issues
- a stock client executable that is missing when you request a client-attached session
- a live `hlds.exe` or `hl.exe` that you intentionally left running and which still has `testbed/runtime/` locked

When the doctor cannot auto-repair the environment, it should tell you which of those boundaries you still need to fix manually.

## Practical remediation examples

Use a user-supplied dedicated runtime directly:

```powershell
.\scripts\doctor-testbed.ps1 -TemplateRoot D:\Games\Half-Life Dedicated Server
```

Pin an explicit `hlds.exe`:

```powershell
.\scripts\install-testbed.ps1 -Configuration Debug -HldsExe D:\Steam\steamapps\common\Half-Life\hlds.exe
```

Refresh the disposable runtime after a source-layout change:

```powershell
.\scripts\doctor-testbed.ps1 -Repair
```

Refresh the runtime explicitly for a live client-attached session:

```powershell
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair
scripts\play-glock-live.bat
scripts\play-mp5-live.bat
```

Stop a stale testbed server before refreshing:

```powershell
taskkill /F /IM hlds.exe
```

If you need a stock client for attached sessions while using the cached dedicated HLDS template, set:

```powershell
$env:HL_EXE = 'D:\Steam\steamapps\common\Half-Life\hl.exe'
```

You can also pass the paths explicitly when you need to override auto-detection:

```powershell
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -HlExe D:\Steam\steamapps\common\Half-Life\hl.exe
.\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -TemplateRoot D:\Games\Half-Life Dedicated Server -HlExe D:\Steam\steamapps\common\Half-Life\hl.exe
```

The live BAT launchers (`scripts\play-glock-live.bat`, `scripts\play-mp5-live.bat`, and `scripts\play-live-test.bat`) enforce that requirement for client-attached play. If no stock `hl.exe` is available after the repair pass, they stop with a clear remediation message instead of silently starting a server-only visual-test launcher.
