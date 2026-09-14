CornOBS
=======

.. image:: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml/badge.svg?branch=cornobs
   :alt: CornOBS Windows Release build
   :target: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml?query=branch%3Acornobs

**針對單機遊戲直播最佳化的 OBS：遊戲順 + 實際直播順，優先於預覽與 UI 順。**

CornOBS 是 OBS Studio 的個人 fork，與 OBS Project 無隸屬關係。
基底為 **OBS 32.2.2**。正式版本在 ``cornobs``；``cornobs-dev`` 僅供開發整合。

面向遊戲、VTube Studio、Browser Sources 與 NVENC 同時運作的場景。
硬體資源不足時，先降低 OBS 自己非必要的顯示工作；不修改 VTube Studio 或遊戲程序。
目前沒有硬體 A/B 結果，沒有 FPS 增益宣稱。

目前功能
--------

- Windows role scheduler：Audio / Playback MMCSS、power policy 與 rollback。
  預設 ``CORNOBS_SCHED=mmcss``，不指定 CPU placement；``auto`` 才嘗試 CPU Sets。
  ``off`` 可關閉。不使用舊版 Pro Audio / HIGHEST / 強制 CCD affinity 策略。
- 每秒 4 次 DXGI process-local VRAM budget 與 render lag 診斷。
  NORMAL / ELEVATED / HIGH / CRITICAL 壓力狀態具有 EMA、hysteresis 與 recovery cooldown。
- ``CORNOBS_GPU_ADAPTIVE=balanced`` 啟用主預覽 FPS cap（30 / 15 / 8）與高壓音量表約 15 Hz。
  預設 ``off``，等待硬體驗收；串流 compositor、來源與音訊頻率維持原設定。
- 新簡易設定檔預設可用的 NVENC / QSV / AMF H.264，最後回退 x264。
- Release + LTO、隱藏音量表不 repaint、fork 自動更新停用。

建置與測試
----------

Windows CI 使用 Visual Studio 2026，先跑 scheduler / GPU policy tests，再建置與封裝。
到 `Actions <https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml>`_
下載對應 commit 的 ``CornOBS-windows-x64-<hash>`` portable artifact。
Windows x64 預設只建置 x64；需要 legacy 32-bit companion targets 時，另以
``-DCORNOBS_BUILD_X86=ON`` 配置。

- `功能、開關、風險與限制 <docs/CORNOBS.md>`_
- `固定遊戲 + VTube Studio benchmark 流程 <docs/PERF_BASELINE.md>`_
- `Windows scheduler <docs/WINDOWS_SCHEDULER.md>`_
- `分支盤點、保留與整合紀錄 <docs/BRANCH_AUDIT.md>`_

請先以 portable mode 與複製的設定進行測試。此 fork 沿用原 OBS 設定目錄；
原生 Twitch / YouTube OAuth integration 未附官方憑證，可使用串流金鑰與自訂 browser docks。
Browser 視覺節流、其餘 UI shedding、VRAM cache 回收及 GPU priority boost 尚未實作。

English
-------

A personal OBS Studio fork for single-PC game streaming: protect game frametimes
and real stream continuity before preview and UI smoothness. Based on OBS 32.2.2.
Conservative MMCSS is the default; CPU placement and GPU adaptive shedding require
explicit opt-in. Unit tests verify decisions, not performance gains. Hardware A/B
acceptance is pending. See the linked documentation for limitations and rollback.

Credits and license
-------------------

OBS Studio is developed by the OBS Project, Lain Bailey and the contributors in
``AUTHORS``. CornOBS is unaffiliated with the OBS Project. GNU GPL v2 or later;
see ``COPYING``. Official OBS: https://obsproject.com .
