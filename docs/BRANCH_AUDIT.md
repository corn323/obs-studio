# CornOBS branch audit - 2026-09-14

Fetched origin and upstream before evaluating every origin branch. GitHub default_branch was already cornobs. Latest stable release: OBS 32.2.2 (2026-08-14), ba2f32bdf791005443988a4955e963663e16b1ed.

| Origin branch | Tip | Upstream comparison / unique work | Decision |
|---|---|---|---|
| 24.0.4-patch | 6594d0fb1596 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| 25.0.8-patch | 4c0d4a1d8e14 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| allow-high-precision-images | 97e5e3efde31 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| automated/clean-services | 6dbed7ffdfe0 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| codex/windows-thread-scheduler | f9d64279c895 | One unique commit over cornobs: f9d64279c (scheduler) | Port; preserve ancestry and safety tag before cleanup |
| cornobs | 0ccc8ed04beb | 13 custom commits over 32.1.2; ancestor of scheduler branch | Keep as release branch; port final changes to 32.2.2 |
| hash-table | bccee93c1cd9 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| master | 6b3e550729f1 | upstream-only/fork-only: 8/0 | Keep: official branch; no custom commits |
| metal | 45a7aca99301 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/27.2 | fa50f64c3ced | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/28.0 | d21891b3ca1a | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/28.1 | c1841e43b04e | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/29.0 | 0fb8bb4b1e18 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/29.1 | c58e511813c3 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/30.0 | d822584cb1c5 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/30.1 | 69d274074e8d | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/30.2 | ca51d330eafd | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/31.0 | 614fb6ae0902 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/32.0 | dcdbd2e9048a | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/32.1 | fb4d98bf88fa | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/32.2 | ba2f32bdf791 | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |
| release/oauth-panel-fix | 2663ed55ea9a | upstream-only/fork-only: 0/0 | Keep: official branch; no custom commits |

Local branches initially: cornobs and codex/windows-thread-scheduler matched origin exactly; master matched origin/master and had no unique fork changes.

## Safety and port accounting

- cornobs-pre-cleanup-20260914 -> 0ccc8ed04bebdda8072ba3a254bf12f702715686
- cornobs-pre-cleanup-20260914-scheduler -> f9d64279c895599d0bd3c2e6899f3521aa006ee6
- Both safety tags pushed before any branch deletion.
- Isolated cornobs-dev starts at 32.2.2. Applied the final 32.1.2..scheduler tree delta with three-way context, rather than replaying superseded scheduler experiments.
- Only port conflict: OBSBasic title branding; retained new upstream braces and CornOBS branding.
- Retained: branding, Release/LTO CI, hardware H.264 fresh-profile defaults, meter visibility skip, updater/Whats New disable, role scheduler and its tests.
- Replaced: old active HIGHEST/Pro Audio/CCD-affinity path with role scheduler. New default mmcss does not assign CPU Sets; explicit auto retains tested placement candidate.
- Updated: CI uses windows-2025-vs2026; version fixed to 32.2.2 so archive tags cannot corrupt version detection.
- Upstream already removed two gs_flush calls from render_video; retained upstream implementation. No speculative copy/pool rewrites.
- Historical intermediate commits are preserved by safety tags; their obsolete behavior is intentionally not reintroduced.

## Completion gate

Promote only after full Windows Release build, scheduler tests and GPU policy tests pass. Retain development branches until the integrated history and remote tips are verified. Hardware performance acceptance is separate and remains pending; no performance gain is claimed.

## Original custom commit dispositions

| Commit | Original change | Final disposition |
|---|---|---|
| 18d66e7e1 | CornOBS: rebrand display name and executable metadata | Retained in final implementation/documentation |
| 5e4979e64 | ci: add dedicated CornOBS Windows x64 build workflow | CI intent retained; updated runner/base version and isolated upstream workflows |
| 5bc5630c0 | ci: build CornOBS as Release to enable LTO/IPO | CI intent retained; updated runner/base version and isolated upstream workflows |
| 6147d7760 | libobs: raise priority of the latency-sensitive media threads | Scheduling superseded by f9d64279c role system; bounded-memory findings retained in docs |
| e2d44bf40 | frontend: cheaper volume meters + hardware encoder default for Intel/AMD | Retained in final implementation/documentation |
| 176cfe8f3 | libobs: register media threads with MMCSS, opt out of power throttling | Scheduling superseded by f9d64279c role system; bounded-memory findings retained in docs |
| f37bdca00 | sched+mem: opt-in CCD pinning + soak-test tooling | Scheduling superseded by f9d64279c role system; bounded-memory findings retained in docs |
| 021681e22 | docs: rewrite README for CornOBS | Retained in final implementation/documentation |
| 292f88b9f | docs: make README Chinese-primary, English secondary | Retained in final implementation/documentation |
| bac3c4777 | frontend: disable the auto-updater and What's New for CornOBS | Retained in final implementation/documentation |
| 92a7d8c87 | docs: note updater-disable and the Twitch/YouTube feature gap | Retained in final implementation/documentation |
| b6442dc54 | ci: pin OBS_VERSION_OVERRIDE so the release tag can't break version parsing | CI intent retained; updated runner/base version and isolated upstream workflows |
| 0ccc8ed04 | ci: use a version-shaped tag, silence stock upstream workflows | CI intent retained; updated runner/base version and isolated upstream workflows |
| f9d64279c | Centralize Windows media thread scheduling with CPU Sets and role policies | Ported; mmcss default added; CPU Sets retained as explicit auto candidate |

The audited original scheduler tip is a second parent of the port history. A history-only merge kept the already reviewed 32.2.2 port tree unchanged; git rev-list HEAD..codex/windows-thread-scheduler returned 0. This preserves commit reachability without reintroducing obsolete intermediate implementations.

The original checkout still contains the user-provided uncommitted PERF_BASELINE.md edits. It was not reset, stashed, or overwritten; all implementation work is in the isolated worktree.
