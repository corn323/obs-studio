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
| Phase 1 | Thread-priority API + raised priority for compositor / video-io / GPU-encode / audio threads | done |
| Phase 1 | Hardware-encoder default extended to Intel QSV and AMD AMF (upstream auto-selected NVIDIA only); x264 one click away; Advanced-output default left as upstream on purpose | done |
| Phase 1 | VolumeMeter repaint 60 Hz → 30 Hz + skip when not visible | done |
| Phase 1 | Preview auto-pause on minimize | already upstream (`OBSBasic::changeEvent`) |
| Phase 2 | Windows: media threads register with MMCSS ("Pro Audio") and opt out of per-thread power throttling / EcoQoS, so a foreground game cannot park them on E-cores or slow them down | done |
| Phase 0 | Measurement baseline + soak-test tooling | planned |
| Phase 2 | Bounded output/encode queues with explicit drop policy | planned |
| Phase 2 | CCD-aware adaptive thread affinity | deferred — needs per-rig validation; MMCSS + throttling opt-out covers the main "game in foreground" case first |
| Phase 3 | CEF (browser source) memory watchdog; x64-only packaging | optional |

## Building

Cloud CI only (no local toolchain required). Push to `master` on the fork triggers
`.github/workflows/push.yaml` → `build-project.yaml`; download the
`obs-studio-windows-x64-<hash>` artifact (portable zip) from the run.

Actions must be enabled once in the fork's **Actions** tab (forks default to disabled).
