# ICESECRET 背景停住 Root Cause 與修復

日期：2026-08-12

## 結論

`build/TyrianGBA.sav` 會進入 Episode 4、Section 50、LVL 20
`ICESECRET`。這一關第一組 Boss 前後的背景速度不是由 Detail Level
決定，而是 LVL 事件共同控制；LOW、NORMAL、HIGH、CUSTOM 理應使用完全
相同的關卡時間軸。

真正問題是 GBA 已解析 PC 的 Event 81（`WRAP2`），卻沒有把它套用到
實際 BG2 捲動游標。長時間 Boss 迴圈會讓有限的 GBA BG2 游標一路降到
0；即使關卡仍要求 `backMove2=15`，下限夾制仍會把每次位移吃掉，因此
看起來像背景黏死。

## PC 原始規格

OpenTyrian `tyrian2.c` 在每個 gameplay frame、繪製背景前執行：

```c
if (mapY2Pos <= BKwrap2)
    mapY2Pos = BKwrap2to;
```

Event 81 則直接設定兩個 MAP2 指標：

```c
BKwrap2   = &megaData2.mainmap[0][0] + eventdat / 2;
BKwrap2to = &megaData2.mainmap[0][0] + eventdat2 / 2;
```

`ICESECRET` 在 time 0 設定：

- 回捲門檻：MAP2 cell offset 100；
- 回捲目的：MAP2 cell offset 8287；
- GBA 像素游標換算：`208 -> 16588`。

PC 在到達上方門檻時會回到接近 MAP2 尾端並保留 `backPos2` 的 28-pixel
相位，所以 Boss 戰可以持續很久，背景仍不會耗盡。

## GBA 修復

1. 將 Event 81 的兩個 flat MAP2 pointer offset 一次換算為 GBA 像素游標。
2. 在 `advance_backgrounds()` 前套用與 PC 相同的門檻判斷與回捲。
3. 回捲時保留 PC `backPos2` 相位，不造成 28-pixel 跳格。
4. 清除舊的 BG2 pending row／prefetch 狀態，讓下一次畫面提交建立正確視窗。
5. MAP2 保留 source row 0 起的完整範圍；MAP3 仍從 row 14 開始，避免無關改動。

增加的前 14 列只改變 MAP2 的邏輯座標範圍。初始游標同步由 16196 調整成
16588，兩者剛好相差 `14 * 28`，因此一般關卡起始畫面仍指向同一個 PC
source row。背景 cache 容量固定，沒有增加 VRAM／EWRAM 常駐用量，也沒有
建立重複 ROM 資源。

## 修復前後證據

同一個 `ICESECRET` 位置 11460、同一套壓力武器條件：

| 狀態 | `backMove2` | BG2 游標 | 回捲次數 |
|---|---:|---:|---:|
| 修復前 | 15 | **0** | 0 |
| 修復後 | 15 | **8821** | 1 |

修復後 Event 81 轉換值：

- `background2_wrap_threshold_scroll = 208`
- `background2_wrap_to_scroll = 16588`
- `source_assets_valid = 1`
- `detail_adapter_self_test = 1`

## 四種 Detail Level 定點驗證

| Detail | BG2 啟用 | 門檻 | 目的 | 回捲次數 | 位置 11460 的 BG2 游標 |
|---|---:|---:|---:|---:|---:|
| LOW | 1 | 208 | 16588 | 1 | 8821 |
| NORMAL | 1 | 208 | 16588 | 1 | 8821 |
| HIGH | 1 | 208 | 16588 | 1 | 8821 |
| CUSTOM | 1 | 208 | 16588 | 1 | 8821 |

四種模式的 `backMove=0`、`backMove2=15`、Boss started/completed 與關卡位置
也完全一致。Detail Level 只決定畫面效果堆疊，不再改變此關的背景事件
語意；LOW 與 PENTIUM 先前看似不同，是缺少共通 PC pointer-wrap adapter
所造成的呈現差異，不應用 per-detail 特判修補。

## 正式建置

- Detail Level：CUSTOM
- Game Speed：NORMAL
- ROM：`build/TyrianGBA.gba`
- 大小：28,057,420 bytes（26.76 MiB）
- SHA-256：`ebfa0e08afdf7cb7587bdcff0473e068d5d4dbbeaae261d963f1e191407966e0`
- 原測試 `build/TyrianGBA.sav` 已原樣保留。
