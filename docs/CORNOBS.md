# CornOBS：單機遊戲直播

**遊戲順 + 實際直播順，優先於 OBS 預覽與 UI 順。**

基底為官方穩定版 **OBS 32.2.2**（2026-08-14）。正式分支 `cornobs`，開發整合使用 `cornobs-dev`。分支與移植逐項紀錄見 [BRANCH_AUDIT.md](BRANCH_AUDIT.md)。此版本沒有硬體 A/B 效能結論；測試通過不代表遊戲 1% low 改善。

## 功能與邊界

| 修改 | 目的／預期改善 | 風險 | 關閉／驗證 |
|---|---|---|---|
| Windows role scheduler | 保護 Audio、Graphics、Video IO、GPU Encode 的服務時間 | MMCSS 仍可能與遊戲競爭；CPU Sets 可能降低 1% low | `CORNOBS_SCHED=off`；比較 off/mmcss/auto |
| 4 Hz DXGI telemetry | 觀察 OBS 的 local memory usage/budget 與預算變動 | Query 與 graphics lock 有小量成本；不是整卡 GPU 利用率 | adaptive off 仍保留相同 telemetry 供 A/B |
| GPU pressure policy | 在 deadline 接近耗盡時先減少非必要工作 | CPU 忙也可能觸發；EMA 有延遲；門檻尚待硬體驗證 | `CORNOBS_GPU_ADAPTIVE=off` |
| Adaptive preview | NORMAL 不限速，ELEVATED 30、HIGH 15、CRITICAL 8 FPS cap | 預覽操作與視覺回饋較慢 | 預設 off；`balanced` 啟用，串流仍依既有設定輸出 |
| Adaptive meter | 一般約 30 Hz，高壓約 15 Hz，隱藏時跳過 repaint | 顯示峰值反應變慢 | adaptive off 回復 30 Hz；音訊 mixing 不變 |
| Hardware H.264 defaults | 新簡易設定檔依可用性選 NVENC/QSV/AMF，最後 x264 | 編碼畫質依硬體而異 | 設定內選 x264；既有使用者設定不變 |
| Release/LTO | 使用最佳化 binary 做比較 | 完整編譯較久 | CI 可選 RelWithDebInfo 作診斷，不能混為效能基準 |
| x64-only Windows build | 跳過本次下載失敗的 x86 dependency 與其建置／複製步驟 | 不含 32 位元遊戲擷取及 32 位元虛擬攝影機客戶端支援 | `-DCORNOBS_BUILD_X86=ON` 仍需要可取得的 x86 dependencies；預設 OFF |

優先只限制主視窗 preview 的整個 swapchain render/present。來源 tick、場景 compositor、Game Capture、VTube Studio capture、真正 stream/record output、audio 與 encoder 的節奏都沒有加入 adaptive 跳幀。Studio Mode 的 program display、projector 與 properties preview 不受第一版限制。

## 設定

環境變數於啟動讀取，修改後重新啟動同一個 binary：

```powershell
$env:CORNOBS_SCHED = 'mmcss'          # 預設；也可 off / auto
$env:CORNOBS_GPU_ADAPTIVE = 'balanced' # 預設 off；on 也是 balanced
# 從同一個 PowerShell 啟動 CornOBS 的 obs64.exe
```

- `mmcss`：role MMCSS + power throttling opt-out，不查詢或指定 CPU placement。
- `auto`：MMCSS + CPU Sets/LLC 候選策略；沒有遊戲隔離保證，尚未證明優於 mmcss。
- `off`：不套用 CornOBS scheduler。舊 `ccd`、空值、未知 scheduler 值均安全停用。
- GPU adaptive 未設定、空值或未知值都視為 off；telemetry 繼續收集。
- 不更改其他程序，不使用 REALTIME、TIME_CRITICAL、registry/kernel hacks 或 GPU thread priority boost。

完整 scheduler 說明：[WINDOWS_SCHEDULER.md](WINDOWS_SCHEDULER.md)。

## Telemetry 與壓力狀態

每 250 ms 讀取當前 OBS D3D11 device 的 adapter3，使用 `QueryVideoMemoryInfo(LOCAL)`。記錄 CurrentUsage、Budget、AvailableForReservation、CurrentReservation。這是 **OBS 程序的 local segment 使用量及 OS 預算**，不是 GPU 95–100% 利用率，也不是全系統 VRAM 用量。切換／重建 device 後重新取得 adapter；API 不支援或 budget=0 時 memory_valid=false，繼續使用 render 資訊。

壓力以 memory usage/budget、OBS 平均 graphics frame time / 設定的 frame budget、取樣區間的 missed/total frames 判斷。graphics frame time 是 OBS CPU 端的工作時間（包含等待），不是 GPU timestamp duration。60 FPS 的 budget 約 16.67 ms，其他 FPS 自動換算。

| 狀態 | memory EMA | render EMA / budget | 區間 rendering lag |
|---|---|---|---|
| ELEVATED | >=80% | >=65% | >0 |
| HIGH | >=90% | >=80% | >=2% |
| CRITICAL | >=95% | >=95% | >=5% |

任一條件即可提高狀態。EMA 權重 0.25；升級可立即發生以避免延遲保護。降級需低於較低的退出門檻，連續 5 秒恢復、且在目前狀態至少 2 秒，每次只降一級。沒有有效 memory 或 render 資料時回到 NORMAL、取消節流。未取得有效 video configuration 時也回復不節流。

log 在狀態改變或每 30 秒寫一行，包含 state、adaptive 開關、memory validity、四項 DXGI 資料、render_ms、rendering_lag、encoding_lag、preview_cap、實測 preview_fps 與 meter interval。preview_fps 是最近約 250 ms 的 draw/present 呼叫速率，短窗有量化誤差，不是螢幕 scanout FPS。

`encode_host_call_peak_us` 是上次 log 以來所有 texture encoder callback 的最大 host call duration，包含插件同步等待；0 表示沒有觀測到非零微秒呼叫，**不是 GPU encoder execution time**。它先用於診斷，不直接驅動第一版 policy。音訊連續性、network drops、遊戲 frametime 仍須由完整 benchmark 收集。

## GPU 路徑盤點與延後項目

- D3D11 Game Capture：`graphics-hook/d3d11-capture.cpp` 的 shared texture copy／MSAA resolve；shmem fallback 才有 staging Map/Unmap。未改遊戲程序與 hook。
- `libobs/obs-video.c`：compositor → color conversion → texture encoder queue；raw output 才走 staging readback。32.2.2 已移除兩處 gs_flush，沿用。
- `plugins/obs-nvenc/nvenc-d3d11.c`：共享貼圖 AcquireSync → CopyResource 到 NVENC pool → ReleaseSync → NV_ENC_MAP_INPUT_RESOURCE。這個 map 不等同 CPU readback。沒有 profile 證據前不刪除 copy 或同步。
- 不新增 alloc/free 壓力策略；既有 pool 保留。先用 PIX/GPUView 查明 allocation、copy 與等待成本，再決定 bounded cache/reuse 改動。
- obs-browser submodule 保留 upstream；此輪不 fork CEF。尚未實作 inactive visual rendering 節流，避免破壞 WebSocket、timer、alert、audio 與 authentication。
- Stats、thumbnail、hidden dock、Qt animation 與 properties preview 尚未加入 adaptive 政策。第一版 UI shedding 僅音量表。
- `SetGPUThreadPriority` 未啟用或實作；沒有 A/B 結果前維持 upstream。
- 多 canvas 的 encoding-lag 診斷目前取主 video output；host encoder call peak 匯總所有 texture encoder。沒有整卡 GPU utilization 或真實 GPU deadline timestamp。

## 建置與驗證

GitHub Actions 的 `CornOBS Build` 使用 `windows-2025-vs2026`、Visual Studio 18 2026 與 upstream 32.2.2 dependencies，先執行 scheduler/pressure tests，再完整 Release build/package。`cornobs`、`cornobs-dev` 的程式推送或手動 dispatch 可觸發。

下載該次 commit 對應的 `CornOBS-windows-x64-<hash>` artifact。測試使用 portable mode 與複製的場景設定，避免改動日常直播設定。此 fork 仍沿用原 OBS 設定目錄及虛擬攝影機識別；auto updater / What's New 停用以免被官方 binary 覆蓋。

原生 Twitch/YouTube OAuth integration 未附官方 client credentials；串流金鑰與自訂 browser dock 可沿用。不可把此限制誤判為 adaptive regression。

實際硬體验收見 [PERF_BASELINE.md](PERF_BASELINE.md)；要求記錄遊戲 frametime、1% low、render/encode lag、音訊與串流連續性，不以 CPU 小幅下降宣稱成功。

API 依據：[Microsoft DXGI memory budgeting](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info)，[GitHub VS 2026 runner](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md)。
