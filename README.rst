CornOBS
=======

.. image:: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml/badge.svg?branch=cornobs
   :alt: CornOBS Build Status - GitHub Actions
   :target: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml?query=branch%3Acornobs

**CornOBS is an unofficial, personal, performance-focused fork of OBS Studio.**
It is not affiliated with or endorsed by the OBS Project. For the official
software, go to https://obsproject.com.

It is a drop-in build: the config directory (``%APPDATA%\obs-studio``), bundle
identifiers and the virtual-camera GUID are left as upstream, so existing OBS
profiles and scene collections load unchanged. Only the display name and
executable metadata are rebranded.

- Base: **OBS Studio 32.1.2** (the last release that builds with Visual Studio
  2022 and free GitHub-hosted CI runners).
- License: **GNU General Public License v2 or later**, unchanged - see ``COPYING``.
- Full rationale and roadmap: `docs/CORNOBS.md <docs/CORNOBS.md>`_.
- How to measure the difference: `docs/PERF_BASELINE.md <docs/PERF_BASELINE.md>`_.


What this fork changes
----------------------

The goal is to lower CPU core usage while streaming **without hurting stream
quality or smoothness**, improve how the real-time threads are scheduled, and
keep memory bounded on long sessions.

1. **Built as Release** so link-time / interprocedural optimization is enabled
   (the upstream developer default, ``RelWithDebInfo``, disables it).
2. **Higher scheduling priority** for the latency-sensitive threads - the
   compositor, the video-io output thread, the GPU-encode submission thread
   (all above normal) and the audio mixing thread (high). A cross-platform
   ``os_set_thread_priority()`` was added for this.
3. **Windows real-time media scheduling.** Those same threads register with the
   Multimedia Class Scheduler Service (MMCSS, "Pro Audio") and opt out of
   per-thread power throttling / EcoQoS, so a foreground game cannot park them
   on efficiency cores or slow them down.
4. **Hardware encoder chosen by default on more GPUs.** Upstream auto-selected
   NVENC only; CornOBS also defaults a new profile's Simple-output encoder to
   Intel QuickSync or AMD AMF when present, falling back to x264. Moving the
   encode off the CPU is the single biggest saving when a game already
   saturates the cores. x264 stays one click away in Settings. (H.264 only, to
   match Twitch ingest; the Advanced-output default is left untouched.)
5. **Cheaper volume meters.** The meter repaint rate drops from 60 Hz to 30 Hz
   (indistinguishable to the eye) and skips entirely when a meter is not
   visible.
6. **Opt-in CCD pinning.** Set the environment variable ``CORNOBS_SCHED=ccd``
   to confine the compositor and GPU-encode threads to the CPU die with the
   most cores on multi-CCD Ryzen (e.g. 9900X / 9950X), so they share one L3
   while a game runs on the other die. It is a deliberate no-op on single-die
   parts (e.g. 9800X3D) and when the variable is unset.

The core A/V queues (video-io frame cache, GPU-encode texture pool, the RTMP
output's frame-drop and dynamic-bitrate logic) were audited and found already
bounded, and are left untouched on purpose.


Who it is for
-------------

- People who stream a **CPU-heavy game** from the **same PC** and see stutter
  when the game is under load.
- People on **AMD or Intel GPUs** who want a hardware encoder picked for them.
- People on **multi-CCD Ryzen** who want to experiment with die-aware thread
  placement (``CORNOBS_SCHED=ccd``).
- Windows x64. macOS / Linux still compile but get only the cross-platform
  subset (priorities); the MMCSS, power-throttling and CCD parts are
  Windows-only.

If you do not have a CPU bottleneck while streaming, stock OBS Studio is the
better choice - most of these changes do nothing for you and you stay on the
latest release.


Risks and caveats
-----------------

- **Base is OBS 32.1.2**, a few months behind the latest OBS release. Bug
  fixes and features after that are not included.
- **Hardware-encoder quality varies by GPU generation.** If the picture is
  not good enough, switch back to x264 in Settings.
- **CCD pinning is experimental.** A wrong affinity can be worse than none.
  It is off by default; remove the environment variable to disable. The
  compositor logs the mask it applied.
- **Twitch account linking has no built-in OAuth client ID** (a fork cannot
  ship the official keys). Streaming with a **stream key** works normally.
- The thread-priority, MMCSS and throttling calls are best effort - a failure
  is ignored rather than surfaced.
- Not code-reviewed by the OBS Project. Do not report issues with this build
  to them.


Credits
-------

CornOBS is a modified version of **OBS Studio**, created by the **OBS Project**
and Lain Bailey, with contributions from many people listed in the ``AUTHORS``
file. All of the software here is their work; this fork only adds the changes
described above. The OBS Project logo and the "OBS" / "OBS Studio" names are
trademarks of the OBS Project and are not used to endorse or promote this
build.

- Upstream project: https://github.com/obsproject/obs-studio
- Upstream website: https://obsproject.com
- Support the OBS Project: https://obsproject.com/contribute

If this fork is ever distributed more widely, the "OBS" element should be
dropped from the name (per the OBS Project's trademark guidance).


Upstream quick links
--------------------

- Website: https://obsproject.com
- Help / Documentation / Guides: https://github.com/obsproject/obs-studio/wiki
- Build Instructions: https://github.com/obsproject/obs-studio/wiki/Install-Instructions
- Developer / API Documentation: https://obsproject.com/docs
- Bug Tracker (upstream only): https://github.com/obsproject/obs-studio/issues
