# CornOBS Gaming Stream Performance Mode

CornOBS 的目標是在單機遊戲直播中，優先保護遊戲 frametime 與真正直播輸出，允許 OBS 本地 Preview / Meter / UI visual refresh 降級。基底 OBS Studio **32.2.2**；唯一維護與預設分支為 `cornobs`。沒有硬體效能增益宣稱。

優先序：遊戲 frametime / 1% low → 真正 1080p60 stream → 音訊連續性 → encoder/network 連續性 → 操作 responsiveness → 本地 preview → meter/stats/thumbnail。降低 visual refresh 不能降低操作 responsiveness。

## GUI 與持久化

Settings → Advanced → CornOBS → Gaming Stream Performance Mode。儲存在 app config 的 `[CornOBS] PerformanceMode`，不是 profile encoder 設定；變更需重新啟動，避免半途重新分配媒體執行緒。取消設定不儲存；未知模式回到 Compatibility。

| 模式 | Scheduler | 本地 preview | Meter | Telemetry |
|---|---|---|---|---|
| Compatibility | CornOBS off；保留 upstream Audio MMCSS | upstream 原有 cadence | 16 ms，沿用 upstream 頻率 | 不做 DXGI 壓力採樣 |
| Balanced（預設） | mmcss，無 CPU placement | NORMAL 無 cap；ELEVATED/HIGH/CRITICAL 為 30/15/8 FPS | 33 ms，高壓 67 ms | 4 Hz |
| Gaming Stream（待四組硬體驗收） | mmcss，無 CPU placement | 直播期間 15/10/8/5 FPS | 直播期間 100 ms，高壓 200 ms | 直播期間 4 Hz |

Gaming Stream 停止直播後恢復無 cap、16 ms meter，停止壓力採樣；只錄影不觸發 Gaming shedding。既有 preview disable/minimize 路徑繼續使用，不自動開啟手動停用的 preview。Studio mode 的主 preview 受同一 display cap；program display/projector 保留既有行為。操作回饋仍由 Qt event loop 處理，拖曳的視覺更新會受 preview cap 影響，需人工驗收。

本地來源／場景縮圖 priority queue 原本最快 100 ms，Gaming 直播期間至少間隔 500 ms；一般 5 秒更新不加速。Stats 原本每 2 秒更新、hide 停止 timer，直接沿用。Preview overlays 隨 preview draw 降頻。隱藏或最小化音量表不排 repaint。不增加全域 UI timer、不減慢 hotkey/menu/scene switch/start-stop/property/drag-drop 事件處理。

GUI 預設 Balanced 保留正式 `mmcss` 排程；Gaming Stream 是待驗收選項，**尚不可作四組客戶的正式推薦**。沒有硬體白名單或 VTube Studio process 偵測；無 VTube Studio 的 A/B 機器不承擔 VTuber 專用控制成本。

## 除錯 override 與 rollback

一般使用者只需 GUI。開發除錯可在啟動前設定：

- `CORNOBS_SCHED=off/mmcss/auto`：覆蓋 GUI scheduler；`auto` 僅為 CPU Sets / LLC 實驗，不推薦給客戶。
- `CORNOBS_GPU_ADAPTIVE=off/balanced/on/gaming`：覆蓋 GUI visual policy；`on` 等同 balanced，未知或空值 fail open 為 Compatibility visual policy。
- 兩個 override 彼此獨立，會造成與 GUI 不同的組合；benchmark 先清除它們。啟動 log 列出有效排程與 visual 模式，不列出任何憑證。
- 回復：選 Compatibility，儲存、重啟、確認沒有 debug override。不改 affinity、process priority、timer resolution、registry 或遊戲程序。

Windows role default 只在 startup thread、媒體執行緒啟動前設定，之後不變。Meter/thumbnail 共享值只由 GUI thread 讀寫。Display cap 使用既有 draw mutex；不在每 frame 配置記憶體、查 topology、寫 log 或改排程。

## Upstream 與 CornOBS 的界線

完整比較：[Upstream feature audit](UPSTREAM_FEATURE_AUDIT.md)。官方已提供 preview disable/minimize、Process Priority、硬體 encoder、Game Capture、Audio MMCSS、Stats hidden timer、Dynamic Bitrate/TCP pacing/network optimizations；不把這些算作 CornOBS 新功能。

本次恢復 upstream `audio-io.c`，解決 scheduler off 連官方音訊 MMCSS 都關掉的退步；POSIX role hooks 回到 no-op。Windows graphics/video-IO/GPU-encode 保留保守 Playback/Low MMCSS 與 failure rollback；不預設 CPU Sets，不綁 P-core 或雙 CCD。

Compatibility 仍有 fork 的 branding、updater/OAuth、既有 fresh Simple profile hardware selection 與 GPU priority build 差異，不等於官方 binary。現有 profile 不因模式切換而改 encoder、解析度、FPS、bitrate 或 preset。

## GPU priority build parity

`libobs-d3d11/d3d11-subsystem.cpp` 的 upstream D3DKMT / IDXGIDevice priority path 由 `GPU_PRIORITY_VAL` 控制。官方 workflow 使用私有 secret；CornOBS 專用 CI 沒有提供，因此 clean release build **unavailable**，以 CMake log `CornOBS GPU priority: unavailable` 明確標示。

存在編譯值時 log 為 `available`，只表示編入，不代表 API 成功。這項工作沒有猜值、反組譯、加入新 REALTIME path 或自行設計 GPU boost。即使以 Administrator 執行，也不能補回未編入的 path。比較官方 release 必須記錄這個限制。

Process-local VRAM usage/budget 不是整張卡 utilization。壓力分類只使用有效 memory budget、平均 compositor render deadline 與 rendering lag；network drops 不參與。Encoder host submission peak 是 CPU 端呼叫時間診斷，不能宣稱 GPU kernel time。Telemetry 無效會回到 NORMAL；Gaming 仍保留 15 FPS preventive preview cap。

不修改 Game Capture copy、Spout2、keyed mutex、NVENC texture copy/pool/staging/cache；需 PIX/GPUView/ETW 證據才另立工作。

## Recommended VTuber Gaming Setup

依四組矩陣驗收 Gaming Stream，日常預設 Balanced；VTube Studio 固定 60 FPS、Windows 使用 Spout2，不用的 NDI / Virtual Webcam 關閉，避免 VSync 把 VTS 拉到 144/240/unlimited。Avatar source、scene compositor、stream/recording、audio mixing/output、encoder submission、network cadence 都不因 UI shedding 降頻。

先使用合理 game FPS cap、Windows Game Mode，停用不需要的 Game DVR 背景錄影；Preview 可手動關閉。Profile D 特別測 240 FPS 遊戲 + 60 FPS stream。OBS 官方建議先測 Administrator 與限制遊戲 GPU 負載；這不能保證 CornOBS 的 GPU priority build parity。

僅在**新建**名為 `CornOBS Gaming Stream` 的 OBS Profile 中，手動套用建議：1920×1080、60 FPS；NVIDIA 使用 NVENC H.264，P5 起步、一般 High Quality 而非 Ultra High Quality、Lookahead off，bitrate/keyframe 遵循目的平台。保留現有 Profile；程式不自動改 x264 或寫入 encoder preset。AMD Profile A 使用平台支援的 AMD H.264，另以支援 AV1 的接收端測 AMD AV1；不强制 NVIDIA 設定到 AMD。

參考：[OBS encoding troubleshooting](https://obsproject.com/kb/encoding-performance-troubleshooting)、[VTube Studio lag troubleshooting](https://github.com/DenchiSoft/VTubeStudio/wiki/Lag-Troubleshooting)、[NVIDIA NVENC guide](https://www.nvidia.com/en-gb/geforce/guides/broadcasting-guide/)。以上設定是測試起點，非本版本已證明的增益。

## 建置、驗收與交付

CI `.github/workflows/cornobs.yaml` 使用 Windows x64 / Visual Studio 2026，先跑 scheduler 與 GPU policy tests，再 build/package。此變更仍需完整 Windows build、GUI smoke、四種硬體各至少三次 A/B/C/D workload；見 [PERF_BASELINE](PERF_BASELINE.md)。不要以單元測試、平均 CPU 或平均 FPS 宣稱效能成功。

正式 native Twitch/YouTube OAuth 憑證仍未包含；stream key/custom browser docks 沿用既有方式。fork updater 保持停用，避免被官方版本替換。Windows 啟動問題見 [WINDOWS_STARTUP](WINDOWS_STARTUP.md)。
