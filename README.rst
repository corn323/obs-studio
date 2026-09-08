CornOBS
=======

.. image:: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml/badge.svg?branch=cornobs
   :alt: CornOBS Build Status - GitHub Actions
   :target: https://github.com/corn323/obs-studio/actions/workflows/cornobs.yaml?query=branch%3Acornobs

**CornOBS 是 OBS Studio 的非官方、個人用、以效能為目標的 fork。**
與 OBS Project 無任何隸屬關係,也未獲其背書。要用官方版請到
https://obsproject.com 。
*(Unofficial, personal, performance-focused fork of OBS Studio. Not affiliated
with or endorsed by the OBS Project.)*

這是一個「原地替換」版本:設定資料夾(``%APPDATA%\obs-studio``)、bundle
識別碼、虛擬攝影機 GUID 都維持與上游相同,所以你現有的 OBS 設定檔與場景集
會直接沿用。只有顯示名稱與執行檔屬性做了改名。

- 基底 / Base:**OBS Studio 32.1.2**(最後一個能用 Visual Studio 2022 與免費
  CI runner 編譯的版本)
- 授權 / License:**GNU GPL v2 或更新版本**,未更動 —— 見 ``COPYING``
- 完整說明與後續規劃:`docs/CORNOBS.md <docs/CORNOBS.md>`_
- 如何量測差異:`docs/PERF_BASELINE.md <docs/PERF_BASELINE.md>`_


這個 fork 改了什麼(What this fork changes)
===========================================

目標:在**不犧牲直播畫質與流暢度**的前提下降低直播時的 CPU 核心佔用、改善
即時執行緒的排程、並讓長時間直播的記憶體維持有界。

1. **以 Release 組態編譯**,啟用 LTO / IPO(上游開發預設的
   ``RelWithDebInfo`` 會關掉它)。→ 全域碼生成最佳化。
2. **提高延遲敏感執行緒的排程優先級** —— 合成執行緒、video-io 輸出執行緒、
   GPU 編碼送出執行緒(above normal),以及音訊混音執行緒(high)。為此新增了
   跨平台的 ``os_set_thread_priority()``。→ 遊戲吃滿 CPU 時,這些執行緒比較不會
   被搶走時間片。
3. **Windows 即時媒體排程。** 上述執行緒會註冊 MMCSS(「Pro Audio」)並關閉
   per-thread 電源節流 / EcoQoS。→ 遊戲搶到前景時,系統不會把 OBS 執行緒丟到
   E-core 或降頻。
4. **更多 GPU 預設使用硬體編碼器。** 上游只自動選 NVENC;CornOBS 在新設定檔
   的「簡易」輸出也會在偵測到時預設 Intel QuickSync 或 AMD AMF,再退回 x264。
   當遊戲已把核心吃滿時,把編碼搬離 CPU 是最大的一項節省。x264 仍在設定裡一鍵
   可切回。(只用 H.264,對應 Twitch 接收;「進階」輸出的預設值刻意不動。)
5. **更省的音量表。** 音量表重繪頻率由 60 Hz 降到 30 Hz(肉眼無差),看不到時
   完全不重繪。
6. **選用式 CCD 綁核。** 設定環境變數 ``CORNOBS_SCHED=ccd`` 後,會把合成與
   GPU 編碼執行緒集中到核心數最多的那顆 CPU die(適用雙 CCD 的 Ryzen,例如
   9900X / 9950X),讓它們共用同一組 L3,而遊戲跑在另一顆 die。單 die 的型號
   (例如 9800X3D)以及未設此變數時,一律不作用。

核心 A/V 佇列(video-io 影格快取、GPU 編碼貼圖池、RTMP 輸出的丟幀與動態碼率
邏輯)經檢視確認**本來就有界**,刻意保持不動。


適合誰(Who it is for)
======================

- 在**同一台 PC** 上直播**吃重 CPU 的遊戲**、遊戲一忙就會卡頓的人。
- 用 **AMD 或 Intel 顯示卡**、希望系統自動幫你選硬體編碼器的人。
- 用**雙 CCD Ryzen**、想試 die 感知執行緒配置(``CORNOBS_SCHED=ccd``)的人。
- Windows x64。macOS / Linux 仍可編譯,但只會拿到跨平台子集(優先級);
  MMCSS、電源節流、CCD 這幾項是 Windows 專屬。

如果你直播時 CPU 沒有瓶頸,官方 OBS Studio 是更好的選擇 —— 這裡多數改動對你
沒有幫助,而且你能留在最新版。


風險與注意事項(Risks and caveats)
==================================

- **基底是 OBS 32.1.2**,比最新的 OBS 版本舊幾個月,之後的修正與新功能不含在內。
- **硬體編碼畫質因 GPU 世代而異。** 覺得畫質不夠好,就在設定裡切回 x264。
- **CCD 綁核是實驗性的。** 綁錯可能比不綁更慢。預設關閉;移除該環境變數即可
  停用。合成執行緒會在 log 印出實際套用的遮罩。
- **沒有內建 Twitch 帳號連結的 OAuth client ID**(fork 無法附帶官方金鑰)。用
  **串流金鑰**照常可以推流。
- 優先級 / MMCSS / 節流相關呼叫皆為 best effort —— 失敗時略過,不會報錯。
- 未經 OBS Project 審閱。請勿向他們回報這個 build 的問題。


致謝(Credits)
==============

CornOBS 是 **OBS Studio** 的修改版;OBS Studio 由 **OBS Project** 與
Lain Bailey 開發,並有 ``AUTHORS`` 檔中列出的眾多貢獻者參與。這裡的軟體全部
是他們的成果,本 fork 只加上以上所述的改動。OBS Project 標誌與「OBS」/
「OBS Studio」名稱為 OBS Project 的商標,此處不用於暗示任何背書。

- 上游專案:https://github.com/obsproject/obs-studio
- 上游網站:https://obsproject.com
- 支持 OBS Project:https://obsproject.com/contribute

若本 fork 日後更廣泛散布,名稱中的「OBS」字樣應依 OBS Project 的商標指引移除。


上游快速連結(Upstream quick links)
====================================

- 網站:https://obsproject.com
- 說明 / 文件 / 指南:https://github.com/obsproject/obs-studio/wiki
- 編譯說明:https://github.com/obsproject/obs-studio/wiki/Install-Instructions
- 開發者 / API 文件:https://obsproject.com/docs
- 問題追蹤(僅限上游):https://github.com/obsproject/obs-studio/issues
