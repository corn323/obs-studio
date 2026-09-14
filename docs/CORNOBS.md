# CornOBS

A personal, **performance-focused fork of OBS Studio**.

## Goals

1. **Lower CPU core usage** while streaming — without hurting stream quality or smoothness.
2. **Better thread / core scheduling** — CCD-aware and adaptive. Must stay generic across
   many CPUs (Ryzen 9900X / 9950X / 9800X3D, etc.) and GPUs. No per-model hardcoding.
3. **Prevent memory growth / overflow** during long streams.

Primary scenario: CPU-bound games + streaming, where encode + compositing competes with
the game for CPU and causes stutter.

Output target: Twitch 1080p60 now, higher resolutions/framerates later.

## Base

Forked from `obsproject/obs-studio` and pinned at tag **`32.1.2`** (2026-04-21).

Reason: `32.1.2` is the last OBS release that builds with `Visual Studio 17 2022` and
the free GitHub-hosted `windows-2022` CI runner. OBS `32.2.0+` requires Visual Studio 2026,
which is only available on OBS Project's private CI runner.

`upstream` remote points at `obsproject/obs-studio` for cherry-picking fixes.

## Branding

Deliberately minimal. Only display name and executable metadata change
(`OBS_PRODUCT_NAME`, window title, About dialog). The config directory
(`%APPDATA%\obs-studio`), bundle identifiers, virtual-camera GUID, and installer
internals are **unchanged**, so CornOBS is a drop-in replacement and existing
profiles / scene collections load without migration.

> Note on the name: the OBS trademark policy discourages using "OBS" in the name of a
> publicly distributed modified version. This is fine for personal use; rename (e.g.
> "CornCast") before any public release.

## Change log (this fork)

| Phase | Area | Status |
|-------|------|--------|
| Rebrand | Display name / exe metadata → CornOBS | done |
| Build | CI builds `Release` so LTO/IPO is on (upstream dev default `RelWithDebInfo` disables it) | done |
| Scheduler | Centralized Windows thread roles, conservative Audio/Playback MMCSS, CPU Sets topology and stable locality; `CORNOBS_SCHED=off/auto` | implemented; hardware A/B pending — see [Windows Scheduler](WINDOWS_SCHEDULER.md) |
| Phase 1 | Hardware-encoder default extended to Intel QSV and AMD AMF (upstream auto-selected NVIDIA only); x264 one click away; Advanced-output default left as upstream on purpose | done |
| Phase 1 | VolumeMeter repaint 60 Hz → 30 Hz + skip when not visible | done |
| Phase 1 | Preview auto-pause on minimize | already upstream (`OBSBasic::changeEvent`) |
| Phase 0 | `tools/cornobs/soak-monitor.ps1` + `docs/PERF_BASELINE.md` for before/after CPU & memory measurement | done |
| Fork fit | Auto-updater and "What's New" fetch hard-disabled (`IsUpdaterDisabled()` always true, `EnableAutoUpdates` default false) — a fork has nothing to update against and the updater could replace CornOBS with stock OBS | done |
| Memory | Core A/V pipeline (video-io cache `MAX_CACHE_SIZE 16`, GPU-encode texture pool `NUM_ENCODE_TEXTURES`, RTMP `check_to_drop_frames` + DBR) verified already bounded — left untouched on purpose | n/a |
| Phase 3 | CEF disk-cache / media-cache caps for browser sources | deferred — the code lives in the `obs-browser` submodule; needs a fork of that repo too before CI can build it |
| Phase 3 | x64-only packaging | not needed (CI only produces x64) |

## Known feature gaps in a fork build

The native **Twitch / YouTube integration** (auto chat dock, "Stream Information"
title/category panel, Manage Broadcast) is **compiled out**. `feature-twitch.cmake`
and `feature-youtube.cmake` only build it when a platform **OAuth client ID +
hash** is provided at build time, and OBS's OAuth flow additionally routes token
exchange through OBS Project's own auth proxy (`auth.obsproject.com`). A fork
build has neither. Workarounds:

- **Chat** and **title/category**: add them back as **Custom Browser Docks**
  (Docks → Custom Browser Docks) pointing at the Twitch/YouTube chat popout and
  the Twitch Stream Manager / YouTube Studio. No OAuth needed. Streaming itself
  works normally with a **stream key**.
- Restoring the *native* panels would need registering your own Twitch app and
  reworking `TwitchAuth` to a proxy-less device-code flow (YouTube also needs
  Google app verification for live scopes — impractical). Not done.

## Building

`.github/workflows/cornobs.yaml` builds Windows x64 Release on `windows-2022`.
It runs scheduler logic/API-failure tests before the full build and packaging.
Pushes to `cornobs` or a manual workflow dispatch trigger it; download the
`CornOBS-windows-x64-<hash>` artifact (portable zip) from the run.

Actions must be enabled once in the fork's **Actions** tab (forks default to disabled).
