CornOBS
=======

CornOBS 的目標是：**在單機遊戲直播中，優先保護遊戲 frametime 與真正直播輸出，允許 OBS 本地 Preview / Meter / UI visual refresh 降級。**

基底為 OBS Studio **32.2.2**，與 OBS Project 無隸屬關係。唯一維護與預設分支為 ``cornobs``。
目前沒有四組目標硬體的 A/B 結果，不能宣稱效能提升或正式推薦 Gaming Stream。

Gaming Stream Performance Mode
------------------------------

Settings → Advanced → CornOBS，儲存後重新啟動：

- **Compatibility**：CornOBS scheduler / adaptive shedding 關閉，保留 upstream Audio MMCSS 與正常 preview/meter cadence。
- **Balanced（預設）**：保守 ``mmcss``，無 CPU placement；保留既有壓力式 preview/meter 節流。
- **Gaming Stream（待驗收）**：``mmcss``，直播時主 preview 約 15 / 10 / 8 / 5 FPS，meter 約 10 / 5 Hz，降低本地縮圖 priority refresh；停止直播恢復。

不降低 stream、recording、scene compositor、來源 tick、Game Capture、Spout2、audio、encoder submission 或 network output cadence。
不預設 CPU Sets/P-core affinity；``auto`` 僅供實驗。不控制遊戲或 VTube Studio 程序。
手動 Disable Preview 與最小化行為沿用 upstream。低頻畫面不能造成操作失去 responsiveness。

Upstream 與 CornOBS
------------------

官方原有 Preview disable/minimize、Process Priority、Audio MMCSS、硬體編碼、Game Capture、Stats 隱藏 timer、Dynamic Bitrate / TCP pacing / Network Optimizations，全部重用。
CornOBS 新增持久化模式、非輸出 display cap / meter / thumbnail policy，以及排程與 telemetry 的保守整合。
詳見 `Upstream feature audit <docs/UPSTREAM_FEATURE_AUDIT.md>`_。

**GPU priority build 差異：** 官方 workflow 提供私有 ``GPU_PRIORITY_VAL``，CornOBS 專用 CI 沒有提供，clean build 會顯示 ``CornOBS GPU priority: unavailable``。
Administrator 執行不能補回未編入的 path。沒有猜測官方值、反組譯或另外啟用 REALTIME boost；benchmark 必須揭露差異。

Compatibility 仍有 fork branding、updater/OAuth 與既有 fresh Simple profile encoder defaults 差異，不等於官方 release binary。
現有使用者 Profile 不因模式切換被改寫。

Recommended VTuber Gaming Setup
-------------------------------

- 依 `四組硬體驗收矩陣 <docs/PERF_BASELINE.md>`_ 評估 Gaming Stream；預設 Balanced/mmcss。
- 有 VTube Studio 時固定 **60 FPS + Spout2**；關閉不用的 NDI / Virtual Webcam，避免 VSync 跑到 144/240/unlimited。
- NVIDIA 優先 NVENC；AMD 使用平台支援的硬體 encoder。Preview 可直接關閉。
- Windows Game Mode、停用不需要的 Game DVR 背景錄影、合理 game FPS cap。Profile D 必測 240 FPS 遊戲 + 1080p60 stream。
- 只在**新建** ``CornOBS Gaming Stream`` Profile 手動套建議 preset：1080p60、NVENC H.264、P5 起步、一般 High Quality、Lookahead off；不改既有 Profile，不自動切 x264。

設定理由與官方參考連結見 `CORNOBS.md <docs/CORNOBS.md>`_。A/B 硬體無 VTube Studio，不需啟動它。
RX 9070 XT 的 H.264/AV1、RTX 5070 Ti/3070/4060 Ti 的 NVENC，以及三種 CPU 拓樸都必須完成驗收。

建置與驗收
----------

Windows x64 CI 使用 Visual Studio 2026，先跑 scheduler / GPU policy tests，再 build/package。
Release + LTO、既有 fork updater disable 沿用。原生 Twitch/YouTube OAuth 未附官方憑證，沿用串流金鑰與 custom browser docks。
請用 portable mode 與複製設定測試；fork 共用原 OBS 設定目錄。

- `功能、設定、rollback 與限制 <docs/CORNOBS.md>`_
- `四組硬體、每組四模式與至少三次測試 <docs/PERF_BASELINE.md>`_
- `Windows scheduler <docs/WINDOWS_SCHEDULER.md>`_
- `Windows 啟動說明 <docs/WINDOWS_STARTUP.md>`_
- `GitHub Actions <https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml>`_

若官方 OBS 正確設定後已滿足需求、CornOBS 無可重複且有意義的改善，該硬體結論必須寫：
**「此目標硬體不需要 CornOBS，建議使用官方 OBS。」** 不為了保留 fork 繼續增加 optimizer。

Credits and license
-------------------

OBS Studio is developed by the OBS Project, Lain Bailey and the contributors in
``AUTHORS``. CornOBS is unaffiliated with the OBS Project. GNU GPL v2 or later;
see ``COPYING``. Official OBS: https://obsproject.com .
