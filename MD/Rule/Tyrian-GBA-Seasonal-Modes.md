# Tyrian GBA 季節模式規則

日期：2026-08-05  
狀態：正式實作規則

## 觸發方式

GBA 沒有可供遊戲直接讀取的即時日期，因此以「載入存檔」取代 PC 的
系統月份判斷：

- 存檔名稱完全等於 `XMAS`（不分大小寫）時，啟用 Christmas Mode。
- 載入其他名稱，或選擇 `Start New Game`，恢復一般模式。
- 名稱可由既有 GBA Save UI 或 `TyrianSaveEditor` 設定；固定欄位尾端的空白
  padding 可接受，但 `XMAS1` 等名稱不會誤觸發。
- `PUMP` 不再具有特殊意義；載入既有同名存檔時會使用一般模式。

## Christmas：可驗證的 PC 原始規格

OpenTyrian `opentyr.c`／`xmas.c`／`nortsong.c` 的實際流程是：

1. PC 在 12 月令 `xmas=true`。
2. 主 shape 檔由 `tyrian.shp` 切換為 `tyrianc.shp`。
3. 語音檔由 `voices.snd` 切換為 `voicesc.snd`；一般 `tyrian.snd` SFX 不變。
4. PC 顯示 Yes／No 雪花確認頁；GBA 的 `XMAS` 存檔名稱本身就是明確選擇，
   因此不再多問一次。

二進位稽核顯示 `tyrianc.shp` 只有下列三段不同：

| SHP section | GBA logical bank | 內容 |
|---:|---:|---|
| 8 | 36 | 玩家彈幕 |
| 9 | 38 | 玩家船與 option sprites |
| 10 | 26 | power-up sprites |

Sections 1–7、11、12 與 `tyrian.shp` 完全相同。Build 因此只對三個差異 bank
做完整、無損、逐 component 的 RLE 預解壓及 round-trip 驗證，共 912 個
12×14 components（153,216 bytes）；runtime 依季節狀態切換 raw bank。

`voicesc.snd` 的 9 個 voice entries 依 `nortsong.c` 同樣移除每段尾端 100
bytes 壞資料，再獨立加入 Maxmod soundbank。Source sound 1–29 繼續使用
`tyrian.snd`，30–38 在 XMAS 下改用 Christmas voice。

完成預轉後，ROMFS 不再重複攜帶整份 `tyrianc.shp`／`voicesc.snd`。

## Halloween Ramble：一般 PC 曲目，不是季節模式

對專案內 OpenTyrian、AprCSTyrian C/C# source、完整 Tyrian data 做全文與
檔名稽核後，沒有找到 Halloween／Pumpkin 模式程式分支，也沒有南瓜、
幽靈或眼球的替換 shape bank。`Halloween Ramble` 是 `music.mus` 的一般
曲目 16（zero-based song 15），由原始資料在下列位置正常選用：

- Episode 1／Section 31／`SOH JIN` 關卡配樂。
- Episode 3／Section 8／`STARGATE` 關卡配樂。
- Episode 4／Section 12／`HARVEST` 關卡配樂。
- Episode 1／Section 11，以及 Episode 2／Section 20 的過場音樂。
- Stock `demo.3`、`demo.4` 的 Demo 配樂。
- Jukebox 選曲／隨機選曲，以及 PC 隱藏的遊戲中隨機換歌操作。

GBA 必須沿用這些原始 song ID 與腳本選擇，不以日期或存檔名稱強制改寫
其他選單／關卡音樂，也不虛構 PC data 中不存在的 Halloween 圖形與語音。

## 程式維護邊界

- 名稱解析與 active mode：`src/opentyrian_season.c`。
- 存檔載入觸發：`src/frontend/frontend_save.inc`。
- Christmas raw bank 選擇：`src/opentyrian_sprite2.c`。
- Christmas voice：`src/gba_platform.inc`。
- Build-time 無損資源：`tools/build_assets.py`。
- 相關檔案去重規則：`vfs/manifest.json`。

任何季節切換都只能改 presentation／聲音／圖形資源，不能改關卡事件、敵人
位置、碰撞、武器傷害、獎賞、RNG 或存檔進度。
