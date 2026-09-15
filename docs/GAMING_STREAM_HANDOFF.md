# Current release delivery

[Latest Release: 32.2.2-corn4](https://github.com/corn323/obs-studio/releases/tag/32.2.2-corn4), source commit `219a14421da9ab98a8dfb42d4fb4ad44857bccb9`. The sole remote/default branch is `cornobs`; the earlier `cornobs-dev` delivery below is historical.

CI run `34941448647` passed all tests, Windows x64 Release build and packaging. The downloaded ZIP passed CRC for all 2195 entries; `obs64.exe` and `obs.dll` are AMD64, the corn4 version string and scheduler startup API are present. GitHub's uploaded ZIP digest matches the local digest:

`e3519fa9828434ca7c94a9bd6b8f061f79578114305038152e0dad278c31a818`

Assets: `CornOBS-32.2.2-corn4-windows-x64.zip` and `.zip.sha256`. Build log confirms GPU priority unavailable. Release publication and default-branch consolidation were explicitly authorized by the user. GUI/game/stream and four-hardware acceptance remain for the user to test; Latest is not a performance acceptance claim.

---

# Gaming Stream implementation handoff

Status: implementation pushed to `cornobs-dev`; Windows x64 CI build/package passed. **Hardware acceptance remains pending with the user.**
Base commit `a25e745daf09d8c14f927317db55a18e6be2a4a1` (remote release and development tips verified 2026-09-15).
Original parent checkout's uncommitted `docs/PERF_BASELINE.md` remains untouched.

## Actual changed files and reasons

| Files | Change / performance reason |
|---|---|
| `frontend/OBSApp.cpp`, `frontend/settings/OBSBasicSettings.cpp`, `.hpp`, `frontend/forms/OBSBasicSettings.ui`, `frontend/data/locale/en-US.ini`, `zh-TW.ini` | Persistent Advanced GUI mode with restart boundary; Balanced/mmcss default; no per-frame settings lookup or live scheduling mutation |
| `frontend/utility/CornPressure.c`, `.h` | Pure mode/override/preview/meter/thumbnail policies; gaming preventive display cap, no output FPS control |
| `frontend/utility/CornAdaptive.cpp`, `.hpp`, `frontend/widgets/OBSBasic.cpp` | Reuse display-only cap and 4 Hz controller; active streaming gate; Compatibility creates no telemetry timer; no VTS process polling |
| `frontend/components/VolumeMeter.cpp` | 100/200 ms visual refresh during gaming stream, upstream 16 ms in Compatibility; hidden/minimized repaint skip; samples unaffected |
| `frontend/utility/ThumbnailManager.cpp` | Cap priority visual refresh at 500 ms while retaining normal 5 s updates and source pipeline |
| `libobs/media-io/audio-io.c` | Restore exact upstream Audio MMCSS registration/revert; eliminate scheduler-off regression and duplicate implementation |
| `libobs/util/threading-scheduler-windows.c`, `threading-scheduler.h` | GUI default set before media-thread startup; debugging override retained; no new CPU placement strategy |
| `libobs/util/threading-posix.c` | Remove unintended non-Windows role priority boost; startup setter no-op there |
| `libobs-d3d11/CMakeLists.txt` | Explicit available/unavailable build status without printing a private value or inventing GPU boost |
| `test/gpu-pressure/pressure-test.c`, `test/scheduler/scheduler-windows-test.c` | Mode, caps, stop restore, thumbnail interval, startup default tests; existing API-failure/rollback tests retained |
| `README.rst`, `docs/CORNOBS.md`, `docs/PERF_BASELINE.md`, `docs/WINDOWS_SCHEDULER.md`, `docs/UPSTREAM_FEATURE_AUDIT.md`, this file | Product boundaries, upstream attribution, GPU build limitation, four-hardware acceptance and stop condition |

## Validation performed

- Zig 0.14.1 `cc -std=c11 -Wall -Wextra -Werror` compiled and ran:
  - `test/gpu-pressure/pressure-test.c` + `frontend/utility/CornPressure.c`: PASS (mode/override, 15/10/8/5 caps, no-stream recovery, Compatibility, thumbnails, pressure hysteresis, invalid telemetry, jittered display timing).
  - `test/scheduler/scheduler-policy-test.c` + `libobs/util/threading-scheduler-policy.c`: PASS (hybrid, multi-LLC, single-LLC, groups, missing topology, modes).
  - `test/scheduler/scheduler-windows-test.c` + same policy: PASS (default setter, API failures/rollback, constraints, topology). Fake APIs; no game process changed.
- Settings `.ui` parsed as XML; mode combo appears once and widget names are unique: PASS.
- `git diff --quiet 32.2.2 -- libobs/media-io/audio-io.c`: PASS, exact upstream restoration.
- `git diff --check`: PASS.
- Full configure attempted with the existing CMake 3.30.9 and `-G "Visual Studio 18 2026"`: **failed, unsupported generator**. Local full build unavailable; the subsequent GitHub Actions full Windows build passed (see below).
- Existing clang-format 19 formatted frontend changes, then rejected upstream libobs's `Language: C` configuration. Formatting-tool compatibility is not a compiler failure; full repository formatting CI remains unverified.

## Pending

Qt settings persistence/cancel/restart and visual inspection; actual minimize/manual preview behavior; stream+recording cadence, Spout2 continuity, AMD H.264/AV1/NVENC runtime; four hardware matrices and network cases. Commit `a30165f3f8501ce6bc1c506b4fc3f0a362c76f5c` was pushed only to `cornobs-dev` with user authorization. No release-branch promotion or Release publication performed.

Benchmark procedure and blank results are in [PERF_BASELINE.md](PERF_BASELINE.md). Official preview/minimize, Process Priority, Audio MMCSS, Stats timer, browser lifecycle, hardware encoders and network features were reused; see [audit](UPSTREAM_FEATURE_AUDIT.md).

There is currently **insufficient evidence to recommend CornOBS for performance or justify more optimizers**. Equally, no measurements prove official OBS already meets every target. If repeatable testing shows no meaningful benefit on a target, record: 「此目標硬體不需要 CornOBS，建議使用官方 OBS。」

## GitHub Actions delivery — 2026-09-15 (Asia/Taipei)

[Run 34896516443](https://github.com/corn323/obs-studio/actions/runs/34896516443): success.
Windows scheduler tests, GPU pressure tests, full Release build, packaging and upload all passed.
Build log confirms `CornOBS GPU priority: unavailable`.

Artifact: `CornOBS-windows-x64-a30165f3f`, ID `10369636314`, 231793890 bytes.
GitHub artifact archive digest: `sha256:b4da5f97beff27d8c384b27fb409dc016725017293f4c2a2d0b77cc12bd2d0a3`.
[Download artifact](https://github.com/corn323/obs-studio/actions/runs/34896516443/artifacts/10369636314).
This digest is GitHub's outer artifact archive digest, not an independently measured portable ZIP checksum.
The user will download and perform GUI/game/stream/hardware acceptance. This documentation-only follow-up does not change the built code.