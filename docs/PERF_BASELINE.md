# CornOBS performance baseline

How to measure whether a change actually helped, and a place to record numbers.

## Method

1. Pick **one** fixed scene and keep it identical across runs: same sources,
   same game, same resolution/FPS (1080p60), same encoder settings.
2. Build A = a known reference (e.g. stock OBS 32.1.2, or the previous
   CornOBS build). Build B = the build under test.
3. For each build, stream or record to disk for the same duration (>= 1 hour;
   a full session is better) while running:

   ```
   pwsh tools/cornobs/soak-monitor.ps1 -IntervalSeconds 15
   ```

4. Also open **Settings -> Advanced -> per-source profiling** is not a thing;
   instead use the built-in **Stats** dock (View -> Docks -> Stats) and note:
   - Average time to render frame
   - Skipped frames due to encoding lag
   - Frames missed due to rendering lag
   - Dropped frames (network)
5. Compare:
   - `cpu_pct_one_core` mean and 95th percentile from the CSV
   - `working_set_mb` growth from first sample to peak (memory bound check)
   - Stats dock: skipped/missed/dropped frame counts (quality/smoothness must
     not get worse)

A change is good only if CPU and/or memory improve **and** the Stats-dock
frame counters do not regress.

## Records

| Date | Build | Scene | Duration | CPU %/core (mean / p95) | RAM start -> peak | Skipped/Missed/Dropped | Notes |
|------|-------|-------|----------|-------------------------|-------------------|------------------------|-------|
| _tbd_ | stock 32.1.2 | | | | | | reference |
| _tbd_ | CornOBS (rebrand+LTO+Phase1+Phase2) | | | | | | |

## Knobs to test

- `CORNOBS_SCHED=ccd` environment variable: on multi-CCD Ryzen (9900X /
  9950X) confines the compositor + GPU-encode threads to the larger die.
  Compare with and without on the same rig. Leave unset on single-CCD parts
  (9800X3D) - it is a deliberate no-op there.
- Encoder: hardware (NVENC/QSV/AMF, the new default) vs `x264`. The whole
  point is that hardware moves the load off the cores the game needs.
