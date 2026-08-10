# Upgrade Ship 左右 Sidekick 連續切換異常：Root Cause

日期：2026-08-10
狀態：Root Cause 已確認；修復與同條件 A/B 驗證已完成
修復結果：[Upgrade-Sidekick-Preview-Fix-2026-08-10.md](Upgrade-Sidekick-Preview-Fix-2026-08-10.md)

## 問題範圍

以 `build/TyrianGBA.sav` 第一個存檔槽的高負荷裝備重現：

- Episode 3、Section 7
- Front Weapon 8、Power 11
- Rear Weapon 30、Power 11
- Left／Right Sidekick 皆為 10
- Generator 6

在 Upgrade Ship 的 Left Sidekick 與 Right Sidekick 清單持續上下切換時，會出現卡頓、整體畫面跳動、長時間操作後可能重置，以及退出後音訊偶發異常。

## 修復前聚焦量測

測試以左右兩頁各切換 300 次，共 600 次，使用 CUSTOM detail、Normal game speed，關閉動態掉幀以直接量出前端工作量。

| 指標 | 修復前結果 |
|---|---:|
| 選擇切換 | 600 次 |
| VBlank IRQ | 3,165 |
| missed VBlank | 2,412 |
| 每次切換平均 CPU | 約 544,031 cycles |
| 單次切換最大 CPU | 544,242 cycles |
| GBA 每畫面預算 | 約 280,896 cycles |
| Dirty commit | 600 次 |
| Dirty bytes | 20,736,000 bytes |
| 每次 dirty bytes | 34,560 bytes |
| Mode-4 DMA 呼叫 | 86,400 次 |
| 每次輸入 DMA 呼叫 | 144 次 |
| 完整 loadout 重載 | 602 次 |
| 主機 32×32 tile 重建 | 602 次 |
| Projectile tile DMA | 10,756 次／2,753,536 bytes |
| 預覽武器音效啟動 | 622 次 |
| 預覽最大有效子彈 | 44 |
| 預覽最大 OAM | 80 |

單次切換的主執行路徑已接近兩個完整 GBA frame budget；下一次 VBlank 又要負擔最多約 96k cycles 的提交工作，因此不是單一貼圖或單一音效造成，而是多個不必要的重工作同時對齊在一次輸入事件上。

## Root Cause 1：相鄰 dirty rect 被合併成全寬更新

`frontend_render_upgrade_submenu_update()` 每次切換都把左側 `0..119 × 16..159` 標成 dirty，右側舊／新選擇列則從 `x=120` 開始。

dirty 合併規則把「邊界相接」也視為重疊，所以左右矩形合併成 `240 × 144 = 34,560 bytes`。Mode 4 畫面每列 stride 為 240 bytes，既有 `frontend_commit_rect()` 又逐列發動 DMA，剛好形成每次輸入 144 次 DMA。

這些 DMA 在 VBlank IRQ 中執行；當它與 OBJ tile、OAM、調色盤更新一起超過 VBlank 時段，後段更新進入 active scanout，視覺上就是整體畫面跳動或橫向撕裂。

## Root Cause 2：Sidekick 變更卻重建整個預覽與全部裝備

一次 Sidekick 選擇變更目前會：

1. 清除並重畫整個左側模擬器底圖；
2. 重新讀取 generator、前砲、後砲、左右 Sidekick、ship；
3. 重建與 Sidekick 無關的 32×32 主機 tile；
4. 將模擬器等待計時強制清為 0，使同一輸入立即觸發高負荷 tick；
5. 逐格上傳 projectile cache 的 pending tiles。

量測中的 600 次輸入實際造成 602 次完整 loadout reload 與 602 次 ship tile rebuild，證明不是必要成本，而是 invalidation 粒度過粗。

## Root Cause 3：GBA 音效 adapter 遺失 PC 固定聲道語意

OpenTyrian 使用八個固定 sample channel。`soundQueue[channel]` 每幀只保留該邏輯聲道最後一個音效，`multiSamplePlay(..., channel, ...)` 會直接取代同一聲道先前的 sample。

GBA 預覽雖然保留了 `sound_queue[6]`，但 drain 時以 `mmEffect()` 啟動新的獨立 Maxmod effect，未保存 handle、未取代同邏輯聲道的舊 effect，離開模擬器時也未取消其所屬 effect。高負荷武器因此持續增加 mixer voice churn，會搶占背景音樂的混音時間，也能解釋退出後偶發音效或音樂異常。

## Root Cause 4：IWRAM stack 已沒有安全餘裕

本次映像的 `__iheap_start` 為 `0x03007A30`，使用者 stack 頂端為 `0x03007F00`，靜態邊界到 stack 頂端只有 1,232 bytes；主函式既有 frame 後可填 canary 更少。聚焦測試結束時 stack canary remaining 為 0。

因此「長時間後偶發 reset」不只是負荷造成的表象；深層的預覽重建／解碼呼叫鏈有機會越過 IWRAM 靜態邊界，造成記憶體污染。這是必須一併修復的可靠性問題。

## 預定修復

1. 靜態模擬器 chrome 只在進入子選單時建立，切換時只更新現金與舊／新清單列。
2. dirty rect 不再跨越左右面板；全寬連續區域改成單次 DMA。
3. loadout refresh 改為逐欄位 diff，只重載真的改變的 Sidekick；ship 未變就不重建 tile。
4. 不再把 `frame_wait` 強制歸零，選擇變更在來源既有的三畫面 cadence 上生效。
5. 合併相鄰 projectile tile DMA。
6. 為六個預覽邏輯聲道保存 Maxmod handle；新聲音先取代同聲道舊聲音，離開頁面時只取消預覽自己建立的 handles。
7. 將只在靜態選單轉場使用的冷函式移出 IWRAM，恢復可驗證的 stack margin，不犧牲遊戲關卡 hotpath。

上述修復均已完成，並以同一份 SAV 與 600 次切換測試驗證；完整數據及正式 ROM 資訊見修復結果報告。
