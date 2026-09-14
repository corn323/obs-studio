# CornOBS 多硬體效能基準與驗收

**目前四組硬體都沒有本變更的實測結果。Gaming Stream 待驗收，不能宣稱保護遊戲或輸出已成功。** 基準固定為官方 **OBS 32.2.2 release**，不能用舊 CornOBS 或只用本地 source build 替代。

## 客戶硬體矩陣

| Profile | CPU / RAM | GPU / 顯示器 | 工作負載與目標 | 必測 |
|---|---|---|---|---|
| A | Ryzen 7 9800X3D / 32 GB DDR5 | RX 9070 XT / 3840×2160 160 Hz | 遊戲 + OBS + Discord；無 VTS；1080p60 stream | AMD Hardware H.264 與 AV1 分開測；single-CCD 無預設 placement；高 GPU 負載只犧牲本地畫面 |
| B | Ryzen 7 9800X3D / 64 GB DDR5 | RTX 5070 Ti / 3840×2160 160 Hz | 遊戲 + OBS + Discord；無 VTS；偶發 congestion；1080p60 | NVENC、4K 遊戲 GPU 負載；沿用官方 Dynamic Bitrate/TCP pacing/network optimizations |
| C | Ryzen 9 9900X / 64 GB DDR5 | RTX 3070 / 2560×1440 | 遊戲 60 FPS + VTS 60 FPS/Spout2 + OBS + Discord；偶發 congestion；1080p60 | mmcss 正式預設；auto/LLC placement 僅獨立實驗；avatar source 不降頻 |
| D | i7-14700K / 64 GB DDR4 | RTX 4060 Ti / 1920×1080 240 Hz | VALORANT/FPS 遊戲 **240 FPS** + VTS 60 FPS/Spout2 + OBS + Discord；1080p60 | 1% low / p99 優先；不綁 P-core；mmcss；UI visual refresh 可降但操作必須流暢 |

A/B 的遊戲不限，但每次比較必須是同一個可重播場景、相同版本與設定；不能把不同遊戲的結果混成平均。沒有 VTS 的 A/B 不啟動 VTS，確認不存在 VTuber 專用成本。A 的 AV1 只使用支援 AV1 的服務或接收端，與 H.264 各自比較；其他設定保持一致。

## 每個 Profile 的四組 build/mode

為免與硬體 Profile 混淆，下表稱 Run A–D。

| Run | Build / 模式 | Preview | Scheduler |
|---|---|---|---|
| A | Official OBS 32.2.2 / Administrator | Disabled | 官方原有行為 |
| B | CornOBS Compatibility / Administrator | Enabled，補測 Disabled 對齊 baseline | off（保留官方 Audio MMCSS） |
| C | CornOBS Balanced / Administrator | Enabled | mmcss，無 CPU Sets |
| D | CornOBS Gaming Stream / Administrator | Enabled，直播應為 15/10/8/5 FPS | mmcss，無 CPU Sets |

全部輸出真正 1920×1080 @ 60 FPS。C/D 使用 VTS 的硬體維持 VTS 60 FPS + Spout2；A/B 硬體無 VTS。固定 encoder、bitrate、keyframe、Lookahead、preset、scene collection、Browser Sources/alerts、Discord 工作負載。

**GPU priority confound：** 官方 CI 提供私有 `GPU_PRIORITY_VAL`；本 CornOBS CI 不提供，clean build 顯示 `CornOBS GPU priority: unavailable`。Administrator 不會彌補未編譯 path。記錄每個 artifact commit、hash、build status/log；`available` 只表示編譯進去，不代表 Windows API 成功。此限制不能省略，也不能以未知 magic number 解決。

## 執行手順

1. 備份設定，使用獨立 portable 測試目錄與複製場景。關閉其他 OBS instance；不要上傳含串流金鑰、OAuth 或 WebSocket 密碼的設定。清除兩個 CornOBS debug overrides，透過 GUI 切 mode、儲存並重啟，再確認 log。
2. 記錄 Windows build、driver、BIOS/CPU 設定、HAGS、Game Mode、Game DVR、電源模式、RAM、VRAM 容量、螢幕 refresh rate、遊戲版本與 cap、OBS/plugin 版本與 encoder。所有 Run 維持相同外部設定；不改遊戲 affinity/priority、timer resolution 或 registry。
3. 固定 replay/內建 benchmark/可重現路線，warm up 5 分鐘。每個 Run 至少 **3 次**固定 10 分鐘收集區間，輪替順序避免溫度與 cache 偏差。原始 frame-time trace 與 OBS counter 起訖時間必須對齊；完成後再跑至少 1 小時 continuity soak。
4. 分別測「有 GPU headroom」與「實際遊戲高 GPU 負載」。使用相同場景與固定畫質/cap，不拿 process-local VRAM ratio 代替 GPU utilization。Profile D 240 FPS + 60 FPS stream 是必要 workload；C 維持 60 FPS 遊戲目標。
5. 使用 PresentMon 或同類 frame trace 收集**遊戲 process**的有效 present 間隔；工具版本與欄位定義固定。排除 warmup，報 average FPS、1% low、p95/p99 frametime、>33.3 ms spike count。定義 1% low 為最慢 1% frame intervals 平均值的倒數；若工具定義不同，明列定義，不能混用。
6. OBS 記錄 render missed/total 與 encode skipped/total 起訖差，百分比以該段 denominator 算；同時記實際輸出 FPS、平均 render time、output frame/packet counters、dropped network frames、reconnect、congestion。Counter reset/device restart 必須分段，不能得到負差仍算成功。WebSocket active FPS 僅為內部指標，另用接收端錄影/PTS/畫面 frame counter 核對真正 60 FPS，避免只檢查標頭 60 FPS 或重複畫面。
7. 系統記 CPU、各 GPU engine utilization、VRAM、OBS Working Set；GPU engine busy 與 VRAM budget 分欄。CornOBS encode host call peak 是 CPU 呼叫時間，不是 GPU execution time。可用既有 `tools/cornobs/collect-runtime.ps1` 輔助收集它支援的欄位；缺遊戲 frametime/接收端指標時必須另補，不能以腳本 summary 代替全部驗收。
8. 接收端聽音與觀察畫面：audio crackle、A/V drift（起終點 clap/同步標記）、freeze、avatar update、網路掉幀與 reconnect。包含直播 + 錄影同時進行；輸出與來源更新必須維持 60 FPS。記錄關閉 preview、最小化、還原、停止/重新開始直播前後的行為。
9. B/C 在穩定網路與可重現的受限頻寬接收端各跑同一矩陣；保留同一組官方 Dynamic Bitrate/TCP pacing/Network Optimizations 設定。把 congestion/network drops 與 render/encoder lag 分開報告，網路 drops 不得算 GPU/scheduler pressure，也不能宣稱 UI shedding 修復網路。

## GUI 與 failure 驗收

- 三個模式儲存/重啟持久化；取消不生效；未知 config 值安全 fallback。Debug override 的有效值在 log 可核對。無 override 的預設為 Balanced/mmcss。
- Compatibility 保留官方 Audio MMCSS、正常 meter 頻率、無 preview cap；不套 CPU placement。對照官方時仍揭露 GPU/OAuth/updater/fresh-profile 預設差異。
- Gaming 直播在 NORMAL 即約 15 FPS preview；四個 policy state 的單元測試驗證 15/10/8/5。沒有可重現 HIGH/CRITICAL 實機事件時，不宣稱已測到。
- 手動 disable preview 不被重新啟用；minimize 不 render/present 主 preview；還原沿用使用者選擇。停止直播恢復正常 preview/meter，來源、compositor、錄影不停頓。
- 重複 hotkey、menu、scene switch、Start/Stop、source properties、drag/drop、Studio mode、隱藏/顯示 docks。記錄操作 latency 與 freeze；可用螢幕錄影對齊輸入。**低 preview FPS 不等於允許操作卡死。** 縮圖低頻時來源 properties 與 output 仍正常。
- Telemetry 無效、DXGI 無 budget、API 失敗與既有 CPU constraints 的單元測試/實機條件分別記錄。不要為測試而修改遊戲或系統權限。

## 記錄模板

每次 run 一行；A 的 H.264/AV1 分別建表，B/C 的 network cases 分開。

| Profile / Run / repetition | Commit / GPU priority compiled+runtime | Game avg / 1% low | p95 / p99 ms / >33.3ms count | render missed/total (%) | encode skipped/total (%) | actual receiver FPS | CPU/GPU/VRAM/Working Set | audio / drift / freeze / network drops | GUI |
|---|---|---|---|---|---|---|---|---|---|
| 待測 | | | | | | | | | |

每個 Profile 報三次各別數據與跨 run 變異，不只報平均值；提前定義有意義的改善門檻，必須超過同 workload 的 run-to-run noise。沒有數據就寫未驗證。

## 正式推薦與停止條件

1. 四個 Profile 都沒有可重複的遊戲效能退步，輸出真正 1080p60，無新增 audio crackle/A/V drift/freeze/encoder instability。
2. AMD H.264/AV1 與 NVIDIA NVENC、Intel Hybrid/AMD Single-CCD/AMD Multi-CCD 都有結果。不得只靠一台 Intel/NVIDIA 成功推薦全部客戶。
3. `auto` 獨立做三次以上實驗；只有同時改善 Game 1% low、p99 frametime、OBS rendering lag 才考慮該硬體 opt-in。雙 CCD 不代表更快，不能做全域預設。
4. 若某項只改善一台卻使其他機器退步，移除或改成有證據的 hardware-specific opt-in。mmcss 保持保守正式排程；Gaming Stream 全域推薦仍待四組驗收。
5. 若正確設定 Administrator、Preview Disabled、hardware encoder、必要時 VTS 60/Spout2 與合理 game cap 後，官方已穩定且 CornOBS 沒有可重複有意義的收益，該 Profile 結論寫：**「此目標硬體不需要 CornOBS，建議使用官方 OBS。」** 不再堆 optimizer。

目前結論：四個 Profile 全部未驗證，沒有足夠證據證明 CornOBS 值得作正式效能推薦，也沒有證據斷言官方已足夠。只完成軟體工作不能回答硬體收益問題。
