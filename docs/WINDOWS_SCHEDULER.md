# CornOBS Windows thread scheduler

The Windows scheduler is applied once at entry to OBS's audio, graphics,
video-IO, and GPU-encode threads. Those call sites supply only a role and retain
an opaque cleanup handle, which they release on the same thread before exit.
There is no per-frame polling, reassignment, or scheduler logging.

## Modes

`CORNOBS_SCHED=auto` is the default when the variable is absent.
`CORNOBS_SCHED=off` makes no CornOBS scheduling changes, including priority,
MMCSS, power throttling, or CPU set assignments. Read-only topology information
is still logged for comparison. Restart OBS after changing the variable.
An empty, invalid, or old `ccd` value safely disables the scheduler; the old
largest-L3 affinity policy has been removed.

```powershell
$env:CORNOBS_SCHED = 'off'  # change to 'auto' for the other sample
# Launch the same CornOBS build from this shell, using the usual executable path.
```

## Architecture

- `libobs/util/threading-scheduler.h`: roles and begin/end lifetime contract.
- `threading-scheduler-policy.c`: pure role and topology decisions, independent
  of Windows calls; consumes CPU records and one process-lifetime seed.
- `threading-scheduler-windows.c`: one-time, thread-safe API/topology initialization,
  application, rollback, cleanup, and diagnostic logging.
- `threading-posix.c`: preserves the existing non-Windows media-thread priorities.

Only the calling OBS thread is modified. There are no process-wide CPU set,
affinity, or priority changes, game-process operations, registry writes,
timer-resolution changes, CPU-model tables, or additional runtime dependencies.

## Role policy

| Role | MMCSS task | Relative MMCSS priority | Base thread priority | Execution-speed throttling | Placement |
| --- | --- | --- | --- | --- | --- |
| AUDIO | Audio | Normal | Normal | Disabled | Shared critical pool |
| GRAPHICS | Playback | Low | Normal | Disabled | Shared critical pool |
| VIDEO_IO | Playback | Low | Normal | Disabled | Shared critical pool |
| GPU_ENCODE | Playback | Low | Normal | Disabled | Shared critical pool |
| NETWORK | None | Unchanged | Unchanged | Windows default | Windows default |
| BACKGROUND | None | Unchanged | Unchanged | Windows default | Windows default |

Network and background roles are available to callers without touching the RTMP
implementation in this change. The four media entry points no longer call the
old priority/MMCSS/affinity helpers. Audio's duplicate MMCSS registration is
removed. A single `Audio` registration is now owned and reverted explicitly.

MMCSS owns dynamic priority; the scheduler does not stack `HIGHEST` on top of
it and does not request `Pro Audio`, real-time process priority, or time-critical
thread priority. Windows task definitions and CPU quotas still determine actual
MMCSS behavior; this is not a guaranteed latency budget. Disabling execution-speed
throttling is a power/QoS decision, not a way to select P-cores.

## CPU topology and placement

`GetSystemCpuSetInformation` supplies CPU set IDs, processor groups,
`LogicalProcessorIndex`, `CoreIndex`, `LastLevelCacheIndex`, and `EfficiencyClass`.
Core and LLC identities are qualified by processor group. SMT siblings count
as one physical core. CPU sets exclusively allocated to another process and
real-time-reserved CPU sets are excluded; a transient parked flag does not remove
a core from the lifetime policy.

- **Hybrid:** differing efficiency classes select the highest numeric class.
  Windows defines higher values as faster, less energy-efficient processors.
  There must be at least two eligible physical cores in the preferred pool.
- **Multi-LLC:** eligible domains must contain at least two preferred physical
  cores. A hash of `(group, LLC index)` and the startup process ID selects a domain,
  independent of enumeration order or logical-processor count. All critical
  threads share that immutable choice until the process exits. This distributes
  choices across processes without pretending to know game load or which domain
  is best. It may choose a different domain on the next OBS launch; the seed and
  chosen IDs are logged. LLC domains are locality hints, not proof of physical CCDs.
- **Single-LLC:** no LLC restriction. A homogeneous CPU keeps Windows's default
  CPU placement; a hybrid CPU can still select its high-performance class.
- **Too-small domains:** relax LLC locality rather than confining work to one
  physical core. If the overall preferred pool also has fewer than two cores,
  use Windows defaults for the entire policy.

Assignments use `SetThreadSelectedCpuSets`, never `SetThreadAffinityMask`.
They include every eligible logical processor in the selected pool, including
SMT siblings. They do not reserve cores exclusively for OBS and cannot guarantee
that a game runs elsewhere. Pre-existing process/thread CPU sets or restrictive
process affinity are left alone, with a fallback log. The policy understands
group-relative indices, but the Windows adapter conservatively falls back if
`GetProcessAffinityMask` cannot report an unrestricted process (including some
multi-group configurations).

Topology and availability are a startup snapshot. CPU hotplug, external CPU-set
allocation changes, and load balancing between running processes are deliberately
not an adaptive subsystem in this implementation.

## Failure and cleanup

Unavailable APIs, unknown or malformed topology, insufficient eligible cores,
allocation failures, and pre-existing placement constraints are nonfatal.
No policy is applied when initialization or the preflight queries fail.

Application order is CPU sets (when useful), normal base priority, MMCSS task,
MMCSS relative priority, and power policy. Any failed step stops the transaction
and attempts every required rollback: revert MMCSS, return power control to
Windows, restore the original base priority, and clear this scheduler's CPU sets.
Successful thread exit performs the same cleanup. Non-Windows priorities retain
their previous behavior.

Every failed API and every failed rollback is logged. If Windows also refuses a
rollback, software cannot promise that the earlier setting has been undone:
the log explicitly says rollback is incomplete, and the handle is retained for
another cleanup attempt at thread exit. No failure aborts OBS startup or crashes
the process. The begin/end API is for fresh OBS-owned threads and must not be
nested or used as a temporary override on an externally managed worker.

Initialization logs mode, processor count, efficiency classes, LLC domains, seed,
and selected CPU set IDs. Per-thread logs record role, task/relative priority,
base priority, CPU set count and LLC choice, power policy, and fallback reason.
Individual processor/core mapping is additionally available at debug log level.

## Verification

The standalone tests require only a C11 compiler; they do not link libobs, Qt,
FFmpeg, or a testing framework. Checks remain active in Release builds.

```powershell
cmake -S test/scheduler -B build_scheduler_tests -G 'Visual Studio 17 2022' -A x64
cmake --build build_scheduler_tests --config Release
ctest --test-dir build_scheduler_tests -C Release --output-on-failure
```

The policy test covers hybrid and homogeneous topologies, unequal multi-LLC
domains, stable order-independent assignment, single-LLC behavior, group-relative
identities, SMT, unavailable CPUs, unknown/malformed input, roles, and modes.
The Windows adapter test injects failures in each apply API, preflight queries,
topology reads/parsing, and rollback, and checks successful lifecycle cleanup.
It includes the actual adapter implementation; fake mutation APIs touch no real
thread scheduling state.

An optional `scheduler-windows-test.exe --live` smoke run calls real Windows APIs
on the disposable helper's own thread, then reverts them. It is not an OBS runtime
or performance test. `CornOBS Build` runs the two tests before its existing full
Windows x64 build and packaging steps.

Hardware A/B validation remains necessary on Intel hybrid, AMD multi-LLC, and
AMD single-LLC systems: compare scheduling delay, OBS rendering/encoding lag and
deadline misses, and game frametime under the same high-CPU workload. Record the
selected LLC in each run, since process seeds can change across launches. No FPS
or frame-time improvement is established by logic tests or a successful build.

## Windows API references

- [CPU set topology fields](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_cpu_set_information)
- [CPU set enumeration](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemcpusetinformation)
- [Thread CPU set assignment and clearing](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadselectedcpusets)
- [MMCSS tasks and priority categories](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)
- [Thread information and power policy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadinformation)
