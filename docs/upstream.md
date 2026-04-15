# Upstream provenance

## Vendored source

- Upstream repository: `https://github.com/ValveSoftware/halflife`
- Upstream ref pinned in this repository: `master`
- Upstream commit pinned in this repository: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Vendored path: `third_party/valve-halflife-sdk/`
- Vendoring model: committed source snapshot, not a git submodule

## Why this upstream

This repository targets stock Steam Half-Life client/server compatibility, so it starts from Valve's official source tree rather than a gameplay fork or engine fork.

Only the server-side `hl.dll` build is wired up first. Client DLL builds, toolchain updates for other Valve tools, or broader gameplay changes are intentionally out of scope for the bootstrap.

## Local delta against upstream

The vendored tree is kept as close to upstream as practical. The only intentional source-level delta inside `third_party/valve-halflife-sdk/` is a small `GameDLLInit` hook in `dlls/game.cpp` so the local `src/future_gameplay_hooks.cpp` module can:

- register placeholder server-only cvars for future gameplay work
- expose a unique logged cvar surface that the smoke test can use to prove the custom `hl.dll` was loaded

All other bootstrap logic lives outside the vendored tree.

## Licensing

- Upstream license copied verbatim to `LICENSES/valve-halflife-sdk-LICENSE.txt`
- Additional notice: `LICENSES/valve-halflife-sdk-NOTICES.md`

The pinned upstream snapshot ships with a top-level `LICENSE` file. No separate top-level third-party license bundle is present in that snapshot, so the notices file documents that fact rather than inventing one.
