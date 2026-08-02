# GBA Detail Level：Low／Normal 原始碼對應與建置切換（v68）

日期：2026-08-02

> 後續狀態：High／Pentium 的 lava、water、wild 與 filtration 已在 v69
> 完成來源規則核對、GBA 硬體適配及四檔效能矩陣；請參閱
> `Tyrian-GBA-Pentium-Source-Parity-Performance-v69.md`。本文件以下內容保留
> v68 當時的 Low／Normal 里程碑紀錄。

## 結論

`LOW` 與 `NORMAL` 已按 OpenTyrian 的 `JE_initProcessorType()`、
`JE_main()`、`JE_playerMovement()`、`JE_drawPlayerShot()`、
`JE_starShowVGA()` 條件逐項接通。兩者是編譯期 profile，不是只改一個
名稱的 runtime flag；未選用的效果分支會由編譯器移除。

PC 版部分效果是對 264×184 之 8-bit framebuffer 做逐像素運算。GBA
沒有相同 framebuffer／palette-index 合成方式，因此本版保留相同事件、
啟用門檻與畫面層次語意，改由 BG、OBJ alpha、OBJ window、mosaic、
palette lookup 與 HBlank DMA 實作。這代表 Low／Normal 的功能規則已完整
對應，但不能把硬體適配誤稱為每個輸出像素都與 PC 演算法相同。

## PC profile 規則

依 `vendor/opentyrian/src/config.c`：

| GBA profile | PC processorType | PC 初始化規則 |
|---|---:|---|
| `LOW` | 1 / 386 | `background2=false`、`displayScore=false`、`explosionTransparent=false` |
| `NORMAL` | 2 / 486 | BG2、score、透明爆炸與標準 effect gates 開啟 |
| `HIGH` | 3 / High Detail | Normal 基礎上設 `smoothScroll=false` |
| `PENTIUM` | 4 / Pentium | `wild=true`、`filtrationAvail=true` |

其他直接寫在 gameplay 的條件：

- `background2over==3` 即使在 Low 也會畫 BG2，並把 `background2` 永久
  恢復為 true。
- BG2 開啟時，玩家飛機與一般玩家子彈會依 BG2 視差產生暗色陰影。
- `starShowVGASpecialCode==1` 的全畫面上下反轉不受 Detail Level 限制。
- `starShowVGASpecialCode==2` 只有 Normal 以上顯示三角光照區。
- smoothie 3／4／5 類 iced／blur pass 只有 Normal 以上執行。
- lava／water 需要 High 以上；Pentium 另有 hue filter 與 wild BG2 blend。
- `displayScore` 在這份 OpenTyrian snapshot 中只有宣告與初始化，沒有後續
  gameplay read；GBA compact HUD 因此沒有一條可逐行搬移的 score 分支。

## Low 完整對應

| 規則 | GBA 實作 |
|---|---|
| 起始 BG2 關閉 | 不配置／更新／顯示第二背景層，降低 tile cache、VRAM 與 row streaming 成本 |
| `background2over==3` | 事件發生時才準備完整 BG2 cache，在 VBlank 原子上傳後恢復顯示 |
| 爆炸不透明 | 普通爆炸與 Sprite2 pickup 爆炸使用 opaque OBJ |
| 亮暗 filter 關閉 | 不啟用 BLDY；對應 PC `explosionTransparent=false` |
| smoothie 3／4／5 與 code 2 | 完全略過；對應 `processorType==1` |
| code 1 上下反轉 | 仍執行；BG ring row 反向＋tile VFLIP，world OBJ 同步反向，HUD 保持可讀 |
| BG2 被事件恢復後的陰影 | 依 PC 的全域 `background2` 狀態恢復，而非永久被 Low profile 禁止 |

## Normal 完整對應

| 規則 | GBA 硬體適配 |
|---|---|
| BG2 與 draw-stage 規則 | 保留完整第二背景層、`background2over` 與 smoothie 1／2 抑制條件 |
| 透明爆炸 | 4bpp 與動態 8bpp 爆炸 OBJ 使用 semi-transparent mode，BLDALPHA 8:8 |
| 飛機／玩家子彈陰影 | 以 OBJ-window mask 暗化其下方畫面；飛機陰影套用來源 `30-mapX2Ofs` 視差 |
| 關卡亮暗 filter | 以 GBA BLDY brighten／darken 承接 `levelBrightness` fade |
| iced pass | 由目前關卡訓練後 BG palette 的亮度，查回來源 palette 0x80～0x8f 藍色 ramp |
| blur pass | 對 world BG／OBJ 啟用 2×2 hardware mosaic；HUD 不被模糊 |
| code 1 | 與 Low 相同，完整場景上下反轉 |
| code 2 | WIN0 三角光照；雙緩衝 161-line WIN0H 表由 DMA0 在 HBlank 串流，避免每幀 160 次 CPU IRQ |

OBJ window 與全畫面 brightness／spotlight 會競爭同一組 GBA blend/window
資源。仲裁順序為 spotlight、關卡 brightness、普通陰影、alpha；遇到互斥
狀態時保留來源中較晚的全畫面效果，避免半套 register 組合。離開 gameplay
時會停止 DMA0、恢復基礎 BG palette 並清除 mosaic/window/blend，防止狀態
污染統計或選單畫面。

## High／Pentium 邊界

本階段要求是完整 Low／Normal。High／Pentium 仍可編譯，並完整繼承
Normal；既有 BG2 抑制、事件與 telemetry 也保留。但 lava、water、
Pentium hue filter 及 PC wild 的逐像素 palette-index 結果仍屬研究性硬體
近似／待辦，不應把 High／Pentium 宣稱為 pixel-identical。

## 建置方式

一般建置由根目錄 `Configure.h` 決定，預設是 Normal：

```c
#define TYRIAN_GBA_CONFIG_DETAIL_LEVEL TYRIAN_GBA_CONFIG_DETAIL_NORMAL
```

可改成 `TYRIAN_GBA_CONFIG_DETAIL_LOW` 或
`TYRIAN_GBA_CONFIG_DETAIL_HIGH`，再執行：

```text
Build-GBA-ROM.bat
```

若只想暫時覆寫一次，不修改 `Configure.h`：

```powershell
.\Build-GBA-ROM.bat -DetailLevel low
.\Build-GBA-ROM.bat -DetailLevel normal
.\Build-GBA-ROM.bat -DetailLevel high
```

## 建置驗證

2026-08-02 已分別以 Low／Normal／High、Game Speed Normal 完成
`-O3 -Wall -Wextra` 編譯與實際 link：

| Profile | ROM bytes | 相對 Normal code | EWRAM 靜態餘量 | IWRAM user-stack 區間 |
|---|---:|---:|---:|---:|
| Low | 28,460,432 | -1,696 bytes | 16,408 bytes | 2,328 bytes |
| Normal | 28,462,128 | baseline | 16,408 bytes | 2,328 bytes |
| High | 28,462,176 | +48 bytes | 16,408 bytes | 2,328 bytes |

Low 的 code size 明確縮小，證明透明爆炸、Normal filters 與特殊光照不是
只在 runtime 關閉，而是未編入該 profile。新增的 spotlight table 使用
644 bytes EWRAM；DMA0 版本沒有增加 IWRAM hot code，也不占用 Maxmod 的
DMA1／DMA2 或既有通用 DMA3。

同一個第一關完整 deterministic golden（關卡事件、敵人生成、碰撞、
Boss、離場、音樂自然停止、統計與返回 Game Menu）也分別跑過 Low 與
Normal：

| Profile | 結果 | Logic／Display frames | Missed VBlank | Max OAM | Sprite2 cache drops |
|---|---|---:|---:|---:|---:|
| Low | PASS | 7,096／12,246 | 74（0.60%） | 93 | 40 |
| Normal | PASS | 7,096／12,246 | 98（0.80%） | 108 | 40 |

兩種 profile 的 gameplay golden 完全相同，差異只落在 presentation
負荷；兩者也都低於正式版 1% missed-VBlank 門檻。舊測試曾把 Sprite2
cache drop 固定要求為 41，但 Detail profile 會因較早趕上 VBlank 而少
送一次純顯示請求，因此已改成「不得高於歷史上限 41」；遊戲邏輯相關
計數仍維持 exact golden，並未放寬。
