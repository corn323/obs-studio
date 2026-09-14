# CornOBS 效能基準與驗收

**尚無本版本的硬體 A/B 結果。** Unit test、CI build 成功與人工驗收是不同狀態。不能以 CPU 3.1% → 2.9% 宣稱成功，也不能用遊戲程序 CPU 使用量推論遊戲流暢度。

## 固定工作負載

建議平台：Ryzen 9 9900X、RTX 3070 8GB、2560x1440 遊戲、60 FPS stream、NVENC。完整記錄實際 CPU/GPU、VRAM、Windows、driver、HAGS、Game Mode、電源模式、顯示器 refresh rate、遊戲版本与 FPS cap。

固定遊戲內 benchmark／replay／路線、畫質、OBS scene collection、canvas/output 解析度、縮放、encoder preset/bitrate/lookahead、VTube Studio model/追蹤輸入、chat/alert browser、Discord 與 Chrome 工作負載。輸出解析度可為 1080p60 或 1440p60，但整組比較必須相同。先 warm up 5 分鐘以完成 shader/cache 初始化。

使用獨立 portable 目錄及複製的場景，先確認沒有第二個 OBS 搶同一份設定。不要把含串流金鑰或 OAuth 的設定檔、完整設定 dump 上傳到 benchmark report。

## Matrix

每一列都分成「GPU 有餘裕」與「由固定遊戲畫質/負载形成的 GPU 95–100%」兩組。不要用 CornOBS 的 process-local VRAM ratio 代替 GPU utilization。

| Build | Scheduler | GPU Adaptive | 用途 |
|---|---|---|---|
| Stock OBS 32.2.2 | upstream | upstream | 官方基準 |
| CornOBS 同一個 binary | off | off | fork 本身差異 |
| CornOBS 同一個 binary | mmcss（預設） | off | MMCSS/power 成本與收益 |
| CornOBS 同一個 binary | auto | off | CPU Sets / LLC 是否值得 |
| CornOBS 同一個 binary | off | balanced | 獨立比較 GPU shedding |
| CornOBS 同一個 binary | mmcss | balanced | 建議候選組合 |
| CornOBS 同一個 binary | auto | balanced | 交互作用；非必要預設 |

每組至少 3 次、每次同一段 10 分鐘；交錯 A/B/B/A 次序，避免熱、cache 或遊戲伺服器順序偏差。候選通過後再做至少 1 小時直播/錄影 soak。環境變數啟動時讀取，每一列完全退出後重新開啟 OBS。

```powershell
$env:CORNOBS_SCHED = 'off'          # mmcss / auto
$env:CORNOBS_GPU_ADAPTIVE = 'off'  # balanced
& 'D:\CornOBS-test\bin\64bit\obs64.exe' --portable
```

上方路徑是範例，換成該次 artifact 解壓路徑。每次記錄 binary SHA256、commit、build config、環境開關與 UTC 起訖。Stock 與 CornOBS 使用同版本 base；不要拿舊 32.1.2 的歷史數字當這輪對照。

## 同步蒐集

1. **遊戲**：用同版 [PresentMon](https://github.com/GameTechDev/PresentMon/blob/main/README-CaptureApplication.md) 鎖定遊戲 PID 與主要 swapchain，匯出每幀資料。固定 CPU frame time 或 display frame time 定義，不能跨版本任意混用欄位。記錄 Average FPS、1% low、p95/p99 frametime、超過 33.3/50 ms 的 spike 次數。
2. **OBS**：Stats 的 render frame time、render missed、encode skipped、network dropped、FPS；記錄開始/結束計數器，用差值除以該區間 total frames。重置／重開時切開樣本，不把累積值混為區間值。
3. **CornOBS log**：只擷取 `CornOBS GPU:` 與 `CornOBS scheduler:` 診斷行，記錄狀態停留時間、preview_fps/cap、process-local VRAM/Budget、render_ms 與 encode_host_call_peak_us。Log 為 30 秒／狀態改變取樣，不能拿它計算逐幀 p99 或完整狀態時間線。
4. **CPU / RAM**：用附帶 `tools/cornobs/collect-runtime.ps1` 指定 OBS、遊戲 PID，每秒收集 CPU（單核心 100%）及 Working Set。除以 logical processor 數才是 Task Manager 風格的整機百分比。RAM 取 start/peak/end 與每小時成長。
5. **GPU**：用同一套硬體監控／PresentMon GPU telemetry 记录整卡 utilization、整卡 VRAM 與 clock/temperature。與 OBS 自己的 DXGI 預算分列。來源不支援的欄位寫 N/A，不填 0。
6. **Output**：錄製同一段測試輸出（先確認 storage 足夠），播放檢查 freezes、重複 frame、audio crackle、A/V drift。實际串流以測試目的地/私人環境驗證 network continuity；本地錄影無法證明網路穩定。

```powershell
powershell -ExecutionPolicy Bypass -File tools/cornobs/collect-runtime.ps1 `
  -ObsProcessId 1234 -GameProcessId 5678 -DurationSeconds 600 `
  -OutputPath 'D:\bench\cornobs-mmcss-adaptive.csv'
```

PID 與輸出位置是範例。工具不讀 OBS 設定、不取得 WebSocket 密碼。既有本機 `perf-capture.ps1` 若仍使用，需另外確認其欄位有效性；它不是本版本必要依賴。不要僅靠 parser 成功就宣稱 collector 的 WebSocket 或實際直播已驗證。

明確定義 1% low：本報告建議 `1000 / 最慢 1% frame-time 的平均毫秒`，另列 p99 frame time；它與單純 `1000/p99` 不相同。先排除 warmup、loading 與其他 swapchain，任何排除規則 A/B 一致。

## 行為驗收

- GPU adaptive off/on 切換後確認 log；NORMAL 主 preview 跟隨實際 OBS cadence，高壓出現 30/15/8 cap，串流設定仍為 60 FPS。
- 同時錄製 preview 螢幕與 stream output 以檢查「preview 卡，但 output 沒有跟著跳幀」。僅 policy test 不足以證明實際 D3D11 行為。
- 壓力下降後觀察分級恢復，不應 HIGH/NORMAL 快速抖動。
- 切場景、resize preview、Studio Mode、最小化/還原、顯示/隱藏 mixer、開 properties、停開錄影、切換 video 設定與退出，檢查 crash、deadlock、resource leak。
- 驗證 VTube Studio 在實際輸出維持更新；不要誤把 preview 節流當作來源節流。
- Chat WebSocket、donate/follow alert、timer、browser audio 與登入保持可用。第一版沒有 browser 節流。
- 無 adapter3、memory query 失敗／budget=0：應繼續 OBS，用有效 render 資料判斷；無全部資料則不節流。硬體失敗案例可用不支援 backend 驗證，單元測試只驗證決策 fallback。
- Scheduler off/mmcss/auto 比較；auto 若沒有明顯收益或遊戲 1% low 退步，不推薦開啟。

## 成功條件與報告

先用 Stock 的 3 次重複量測估計噪聲。候選必須在遊戲 1% low / frametime spikes、rendering/encoding lag、audio 與 stream continuity 均無可重複退步，同時至少有一项有意義的改善。只改善平均 FPS 或 CPU 不足以通過。音訊 glitch、hang、leak、場景切換 stutter 是否決項；不能用平均值掩蓋。

| 日期 / commit / binary hash | GPU 情境 | Mode | 次數 / 時長 | Game avg / 1% low | p99 ms / spikes | Render ms / missed % | Encode skipped % / host peak | VRAM usage / budget / board | CPU / WS start-peak-end | Network drops / A/V continuity | 結論 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 待硬體量測 | | | | | | | | | | | 未驗收 |

## 進一步 GPU profile

以同一 workload 做 PIX/GPUView trace：Game Capture copy/resolve、compositor、color conversion、NVENC CopyResource、keyed mutex wait、staging Map，以及 texture allocation 次數。將時間區段與 game frametime spikes 對齊後才提出 copy/pool 修改。不要只因看到 CopyResource 就認定可刪。

## 已完成的自動驗證

邏輯 tests 包含 scheduler role/CPU topology/API failure/rollback、mmcss 無 CPU API、config parser、pressure 門檻、EMA recovery/hysteresis、無效資料 fallback、preview cap 與 60 Hz jitter 下的節奏。Windows 完整 Release build 結果以交付 commit 的 CI 為準。這些不代表硬體效能或直播驗收。
