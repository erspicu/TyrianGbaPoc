# Upgrade Ship Sidekick 預覽修復與效能比較

日期：2026-08-10
狀態：已完成修復、聚焦壓力驗證與 CUSTOM 正式建置

## 結論

`build/TyrianGBA.sav` 的高負荷裝備可在 Upgrade Ship 的 Left／Right
Sidekick 清單連續切換。聚焦測試各頁 300 次、共 600 次，完整結束且
沒有重置；背景音樂在退出預覽後仍為 active，模擬器與其專屬聲音
handle 均走完清理流程。

切換核心平均成本由約 544k cycles 降至約 86.7k cycles，下降約
84.1%；missed VBlank 由 2,412 降至 12，下降約 99.5%。相鄰切換與
長距離捲動兩種操作模式的剩餘值都固定為 12，沒有隨游標路徑增加；
由此判定它是測試自動進出兩個子選單的固定轉場成本，而非 600 次連續
游標更新造成的累進掉幀。長距離捲動測試的最大切換成本約 90.8k
cycles。

## 同條件 A/B 數據

條件：CUSTOM detail、Normal game speed、同一份 SAV、Left／Right
Sidekick 各 300 次；測試刻意關閉 gameplay drop-frame，直接觀察前端
是否能在原生 VBlank 節奏內完成。

| 指標 | 修復前 | 修復後 | 差異 |
|---|---:|---:|---:|
| 選擇切換 | 600 | 600 | 相同 |
| VBlank IRQ | 3,165 | 775 | 不再因工作堆積拉長測試 |
| missed VBlank | 2,412 | 12 | -99.5% |
| 每次切換平均 CPU | 544,031 cycles | 86,731 cycles | -84.1% |
| 單次切換最大 CPU | 544,242 cycles | 86,874 cycles | -84.0% |
| Dirty bytes（邏輯變更區） | 20,736,000 | 3,484,800 | -83.2% |
| Mode-4 DMA 呼叫 | 86,400 | 1,200 | -98.6% |
| Loadout refresh | 602 | 2 | -99.7% |
| Ship tile rebuild | 602 | 2 | -99.7% |
| Projectile tile DMA | 10,756 | 36 | -99.7% |
| 預覽武器音效啟動 | 622 | 22 | -96.5% |
| Stack canary remaining | 0 bytes | 1,064 bytes | 未越界 |
| ARM 換色差分測試 | 無 | 256／256 × 2 通過 | selected／normal 全值域 |

長距離捲動清單的補充測試：600 次、最大 90,792 cycles、missed VBlank
仍為 12、正常回到 Upgrade Ship 主畫面。

## 實作內容

1. Sidekick／Generator 選擇只更新現金文字與舊、新游標列，不再重建
   整個 120×144 左側預覽。
2. Upgrade item 名稱與價格在進入子選單時快取，游標移動不再重讀
   HDT 與重排字串。
3. 游標色階轉換改為小型 ARM/IWRAM 核心；一次處理四個 Mode-4
   pixel，使用 ARM conditional execution 避免逐像素分支。
4. ARM 核心用全部 0..255 輸入值，分別對 selected 與 unselected
   路徑和 C reference 做 differential test。
5. 寬且多列的 dirty rect 提升為完整 scanline 的單次連續 DMA，避免
   每列重新啟動 DMA；600 次操作由 86,400 次降至 1,200 次。
6. Loadout refresh 改成逐欄位 diff；只讀取實際改變的 Sidekick，船型
   未變時不重建 32×32 ship tile。
7. 連續游標輸入使用兩個 physical frame 的 settle/coalesce；中間選項
   不生成稍後立即被取代的武器彈幕或聲音，選單畫面仍逐幀回應。
8. Projectile cache 的相鄰 pending slot 合併為 DMA run。
9. 依 OpenTyrian 固定 sample-channel 語意保存 Maxmod handles；同一
   邏輯聲道的新聲音會取代舊聲音，離開預覽時只取消預覽自身 handles。
10. 六個僅供靜態轉場的冷函式移出 IWRAM，回收 stack 邊界；遊戲關卡
    hotpath 的 ARM/IWRAM 配置不變。

## 驗證輸出

- 修復前資料：`temp/upgrade_sidekick_v89_baseline/`
- 最終相鄰切換：`temp/upgrade_sidekick_v89_fixed4/`
- 最終長距離捲動：`temp/upgrade_sidekick_v89_scroll/`
- 最終畫面：`temp/upgrade_sidekick_v89_fixed4/sidekick_fixed4.png`

## 正式 ROM

- 檔案：`build/TyrianGBA.gba`
- Detail：CUSTOM
- Game speed：Normal
- 大小：28,050,844 bytes（26.75 MiB）
- SHA-256：`b3e9cbc465a10149a7985ea58680b1c374476e066a245f9f9bd523a90db1779d`
- mGBA smoke：600 frames 正常，無 stderr
- `build/TyrianGBA.sav` 已依原 SHA-256 完整還原
