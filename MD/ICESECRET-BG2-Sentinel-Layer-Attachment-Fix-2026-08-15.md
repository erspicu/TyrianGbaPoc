# ICESECRET BG2 Sentinel 與固定建築圖層修復

日期：2026-08-15

## 結論

ICESECRET 回捲後的基地、砲台與圓頂中，event 12 生成的 definition
80～83、87～90 是 PC 原始資料明確指定的 **MAP1／BG1 ground object**。
LOW 與 CUSTOM 的正確座標依附層都相同，不能因 Detail Level 改綁 BG2。

真正的缺漏是 GBA 沒有翻寫 `background2over=254/255` 的隱藏語意：

- OpenTyrian 只有 `background2over` 等於 0、1、2、3 時，才會在四個
  對應階段呼叫 `draw_background_2()`；
- Episode 4 LVL 20 在 position 11460 寫入 254，然後把事件位置回跳到
  14；254 不符合任何繪製分支，這一段的 BG2 應停止顯示與捲動；
- 舊 GBA adapter 會把未知值落到「後景」預設分支，令 BG2 繼續顯示；
- 建築物其實仍正確黏在 BG1，但錯誤殘留的 BG2 使用不同視差，因此肉眼
  會把 BG2 當成地面參考，誤認為建築隨柔性鏡頭漂移。

## 原始碼與資料證據

`vendor/opentyrian/src/tyrian2.c` 的 event 12：

```text
eventdat6 0/1 -> enemy pool 25
pool 25       -> tempMapXOfs = mapXOfs
                 tempBackMove = backMove
```

所以 event 12 的座標權威就是 MAP1。該檔的 BG2 繪製則只有四個相等判斷：

```text
background2over == 3
background2over == 0
background2over == 1
background2over == 2
```

全四章 LVL 掃描也確認，非 0～3 值只出現在 Episode 4 LVL 20：一次 255、
兩次 254；這些都是刻意不命中任何 BG2 draw site 的 sentinel。

## 修復

1. `source_background2_present()` 只接受 draw order 0～3；254/255 隱藏。
2. `background2over==3` 依 PC 原始碼無條件繪製，並可在 LOW 把初始關閉的
   BG2 重新啟用，不套用其他 draw site 才有的 smoothie/detail gate。
3. BG2 不呈現時，同步停止 `backPos2` 對應的垂直 cursor 與 delay phase；
   這符合 PC 將移動寫在 `draw_background_2()` 內的行為。
4. 不可見 BG2 不再做呈現列排程與 idle prefetch，避免為錯誤隱藏層付費。
5. layer self-test 新增 0～3 接受、254/255 拒絕的回歸條件。

## 定點驗證

條件：Episode 4 Section 50、NORMAL game speed、壓力武器、2,900 VBlank，
LOW 與 CUSTOM 各跑一次，兩者都通過第一段 Boss 並抵達回捲後 position 477。

| 項目 | 修復前 | 修復後 |
|---|---:|---:|
| `background2_over` | 254 | 254 |
| `background2_present` | **1（錯誤）** | **0（正確）** |
| BG2 camera scroll | 8357 | 8833（隱藏點起凍結） |
| ground attachment 樣本 | 538 | 538 |
| 鏡頭移動中樣本 | 216 | 216 |
| attachment 失敗 | 0 | 0 |
| 最大 X／Y 相對 BG1 誤差 | 0／0 pixel | 0／0 pixel |
| source assets valid | 1 | 1 |
| music active | true | true |

這組資料同時證明：原本建築座標沒有漂移；畫面錯覺由不該存在的第二視差
背景造成。修正後 LOW 在該段正確呈現 BG1 + BG3，CUSTOM 也遵守相同關卡
事件，而不是強行保留平滑 Alpha 的 BG2。
