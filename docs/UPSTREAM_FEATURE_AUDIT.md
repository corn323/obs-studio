# Upstream feature audit — 2026-09-15

Baseline: upstream tag `32.2.2` (`ba2f32bdf791005443988a4955e963663e16b1ed`).
Read-only remote verification: both current `origin/cornobs` and `origin/cornobs-dev`
resolve to `a25e745daf09d8c14f927317db55a18e6be2a4a1`. The local `cornobs` branch
is stale; it is not the current release comparison. Changes below are on `cornobs-dev`.

| Area / evidence | Upstream 32.2.2 | Existing CornOBS | Decision in this change |
|---|---|---|---|
| Preview enable/disable; minimize (`frontend/widgets/OBSBasic.cpp`, `changeEvent`, `EnablePreviewDisplay`) | Already supported; preserves user preview choice | Same path | Reuse; no second enable/disable state machine |
| Preview swapchain cadence (`libobs/obs-display.c`) | Display runs at compositor cadence | Display-only FPS cap already added | Reuse for Gaming Stream 15/10/8/5 FPS; no compositor/source/output changes |
| Process Priority (`frontend/settings/OBSBasicSettings.cpp`) | Existing GUI control | Same control | Do not override process priority or game settings |
| Windows GPU priority (`libobs-d3d11/d3d11-subsystem.cpp`) | D3DKMT and IDXGIDevice path, gated by `USE_GPU_PRIORITY` | Same source, but CornOBS CI supplies no `GPU_PRIORITY_VAL` | Build parity unavailable; explicit CMake status and documented benchmark confound |
| GPU priority build (`CMakePresets.json`, `libobs-d3d11/CMakeLists.txt`, `.github/workflows/build-project.yaml`) | Preset imports value; official workflow supplies private secret | Dedicated `.github/workflows/cornobs.yaml` omits it | Do not guess or extract value; do not add another priority implementation |
| NVENC (`plugins/obs-nvenc/nvenc-properties.c`) | Hardware encoding and P5 default; Lookahead default depends on capability | Unmodified encoder; broader fresh Simple profile hardware selection | Retain encoders; manual recommended preset only for a new named profile |
| AMD H.264/AV1 and QSV | Existing hardware encoder plugins | No encoder changes | AMD/NVIDIA runtime acceptance required; no automatic x264 switch |
| Game Capture (`plugins/win-capture`) | Existing capture/copy/synchronization | Runtime path unchanged | No copy/pool/mutex optimization without PIX/GPUView/ETW |
| Audio MMCSS (`libobs/media-io/audio-io.c`) | Audio registration + revert already present | Replaced by CornOBS role scheduler; off lost upstream registration | Remove duplicate audio role integration; restore exact upstream file in all modes |
| Windows power throttling (`libobs/util/threading-scheduler-windows.c`) | No matching CornOBS media-role execution-speed policy | Per-thread opt-out with rollback for critical roles | Keep conservative MMCSS mode for graphics/video/encode; off makes no CornOBS scheduling mutation |
| POSIX priorities (`libobs/util/threading-posix.c`) | No CornOBS role priority boost | Role hooks raised priority despite comment claiming preservation | Make role hooks no-op on non-Windows; Windows policy must not change POSIX defaults |
| Browser lifecycle (`plugins/obs-browser`, pinned at `3f0a2cdf378939ebe3c6f9ab36d4ea100c25aac2`) | Already supports visibility messages/WasHidden and shutdown_on_invisible destroy/recreate | Same gitlink; pinned upstream source checked remotely | Reuse lifecycle; no browser FPS changes or new optimization claim |
| Meter (`frontend/components/VolumeMeter.cpp`) | Shared 16 ms timer | Unconditional 33 ms + hidden skip, 67 ms when pressure high | Compatibility restores 16 ms; Gaming stream 100/200 ms, hidden/minimized repaint skip; audio samples untouched |
| Stats (`frontend/widgets/OBSBasicStats.cpp`) | 2000 ms; show starts / hide stops timer | Same | Already cheaper than proposed 1–2 Hz; do not increase refresh |
| Source/scene thumbnails (`frontend/utility/ThumbnailManager.cpp`, `ThumbnailItem.cpp`) | 5 s normal, 100 ms priority queue; enabled/view checks | Same | Gaming stream caps priority refresh at 500 ms; reuse queue and capture path |
| Preview overlays | Part of preview display draw | Same | Included in display cap; input/event dispatch unaffected |
| Hidden docks / Qt animations | Widget-specific visibility and event-driven updates | No general animation controller | Keep upstream; no global event-loop throttle or blanket animation override |
| Network (`plugins/obs-outputs/rtmp-stream.c`, frontend Advanced network controls) | Dynamic Bitrate, TCP pacing and network optimizations exist | Unmodified | Reuse; network drops never enter pressure classification |
| Release LTO, bounded buffers | Upstream build options and bounded pipeline pools | Release configuration already selected | Do not claim upstream mechanisms as CornOBS invention |

## GPU priority limitation

The upstream code requests HIGH with HAGS and REALTIME without HAGS, followed by
`SetGPUThreadPriority(GPU_PRIORITY_VAL)`. This is an upstream path, **not a new
CornOBS optimization**. This task does not enable it with invented values, add
REALTIME policy, or claim equivalence to the official binary. The current dedicated
CI has no supplied value, so its clean configuration reports
`CornOBS GPU priority: unavailable`. A truthy externally supplied build value would
compile the existing path and report `available`; that means compiled, not that
Windows accepted it. No private value is printed. Running as administrator alone
does not compile a missing path. Benchmark must retain this difference.

## Newly added / remaining risk

New: persistent Advanced GUI mode; Gaming Stream preventive local visual shedding;
GUI-selected startup scheduler default; explicit GPU build status.
Removed duplication/regression: audio scheduler wrapper and non-Windows role boost.
Existing broader fresh-profile encoder defaults and fork updater/OAuth differences
remain documented Compatibility differences; Compatibility is not byte-for-byte
an official release. No hardware benefit has been demonstrated.

Pinned browser source: [obs-browser-source.cpp](https://github.com/obsproject/obs-browser/blob/3f0a2cdf378939ebe3c6f9ab36d4ea100c25aac2/obs-browser-source.cpp), SendBrowserVisibility and SetShowing. Runtime behavior remains untested.
