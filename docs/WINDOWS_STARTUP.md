# Windows 啟動與網路

## 下載與辨識

從 [CornOBS Release](https://github.com/corn323/obs-studio/releases/latest) 下載 Windows x64 ZIP，完整解壓後執行 `bin/64bit/obs64.exe`。不要直接在 ZIP 內執行。

本版標題應包含 **CornOBS 32.2.2-corn3**。捷徑若仍顯示舊版，請檢查目標是否指向這次解壓的 `obs64.exe`。請保留官方 OBS 安裝；本 fork 沿用 OBS 設定目錄，測試時使用複製的設定與 portable mode。

## Smart App Control 阻止啟動

目前 CornOBS 自行編譯的執行檔沒有公開信任的程式碼簽章。Windows 可能因此阻止啟動或載入 DLL；正式 Release 標籤與 SHA-256 校驗碼不會讓 Windows 自動信任程式。

若提示明確寫 **Smart App Control**，請保留完整提示與被阻擋的檔名，等待可信簽章版本。重新解壓、改檔名或以系統管理員執行都不能補上可信簽章。此專案不提供關閉 Smart App Control、安裝自簽根憑證或移除安全標記的啟動器。

Microsoft 說明：[Smart App Control 程式碼簽章要求](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)。可信簽章仍須涵蓋實際被載入的二進位檔，並經 Windows 實機驗證。

## Windows 防火牆網路提示

這與 Smart App Control 的執行信任檢查不同。請先確認提示中的程式路徑確實是本次解壓的 CornOBS，並確認是哪個功能需要連線。若只在可信的家用網路使用需要接收入站連線的功能，僅允許私人網路；不要為了消除提示而勾選所有網路或關閉防火牆。

在 Windows 安全性 → 防火牆與網路保護 → 允許應用程式通過防火牆，可以檢查特定應用程式的允許項目。不要把官方 OBS 的既有規則當成本次 CornOBS 路徑的規則。ZIP 解壓位置改變時，舊的程式路徑規則可能不適用。

若仍無法直播，記錄錯誤訊息、連線功能與被阻擋的程式路徑，再判斷是否真的是防火牆。CornOBS 不會自動新增全域或所有網路的例外規則。

Microsoft 說明：[允許應用程式通過防火牆的風險](https://support.microsoft.com/en-US/Windows/Security/Firewall/risks-of-allowing-apps-through-windows-firewall)。
