# TyrianGbaPoc v56：逐關色盤與柔性鏡頭邊界修正

日期：2026-07-30  
分支：`palette-camera-data-v56`

## 逐關色盤

背景仍由 runtime 直接讀取 ROMFS 的 stock LVL／MAP／SHP；build 工具
只是自動重建每關真正會出現的 8×8 runtime keys，再對平台必要的
8bpp→4bpp adapter 做感知色差訓練。沒有手工 tile 修正或關卡專用圖。

資料配置：

- 保留五份 shape-profile palette 作未知關卡 fallback；
- 62 個 stock LVL sections 各有 512-byte palette 與 4,096-byte
  nearest LUT；
- 全部 62 關實際只有 202 種 hue masks，因此使用一份 64 KiB
  `mask -> compact ID`，每關只保存 202-byte bank assignments；
- 總增量 363,756 bytes，而不是 62 份 64 KiB dense tables。

build gate 對每關每個 runtime key 分別要求 OKLab 與 CIEDE2000
不得退步。62 關全部通過。Episode 4 第一關：

- 1,957 unique runtime keys；
- 43 active masks；
- OKLab weighted MSE 改善 43.485578%；
- CIEDE2000 weighted MSE 改善 17.043786%；
- 9 個 active masks 安全切換至該關重訓 bank；
- 兩種 metric 的 regression key 都為 0。

## 柔性鏡頭修正

原本 BG VOFS 會加上 `source_camera_offset_y`，但背景 cache 仍只依
未偏移 map scroll 保留 21 rows。鏡頭抵達 Y=0 或 Y=24 時，PPU 因而
讀到沒有安裝／已釋放的 ring rows。

修正後：

- 用與硬體 VOFS 相同的 signed presentation scroll 計算 cache window；
- map scroll 或 camera scroll 任何一方跨 8-pixel 邊界，都會載入新露出
  的 top／bottom row；
- 所有 active layers 先 preflight；若該層已有不同 VBlank row upload，
  camera Q8 target 暫停一個 logic tick，不會先顯示尚未準備的 row；
- top／bottom 使用 signed clamp，不會在 0 附近 unsigned wrap；
- dynamic-frame-drop 保存的 held window 也改存 camera-adjusted full
  scroll，避免 deferred presentation 提前釋放仍在顯示的 row。

## 階段驗證

High detail／Normal speed：

- release ROM 編譯成功；
- 主 autotest：PASS；
- camera origin Y 真實走滿 `0..24`；
- camera origin X 真實走滿 `24..48`；
- background stream drops：0；
- background approximation：0；
- 12,168 display frames 中 missed VBlank：11；
- Episode 2 High／Normal：10,475 frames、41 missed VBlank、0.391%；
- Episode 2 Low／Normal：10,475 frames、21 missed VBlank、0.200%；
- ROM 27,770,076 bytes，約 26.48 MiB，仍低於 32 MiB。

## 後續效能定案

初版 camera-aware scheduler 雖然修掉破圖，但 21-row ownership 在
Episode 2 會因上下折返反覆釋放／重建相同列：

| 方案 | 總換列 | 同步換列 | missed VBlank |
|---|---:|---:|---:|
| 柔性鏡頭、21-row ownership | 4,474 | 833 | 73 |
| cooperative camera prefetch 實驗 | 4,482 | 738 | 84 |
| 柔性鏡頭、25-row hysteresis | 4,149 | 175 | 41 |
| 固定置中 crop | 4,145 | 350 | 41 |

cooperative 實驗因單一 prefetch buffer 在 map-leading 與 camera-reverse
兩個方向間反覆取消而退步，已完整撤回。25-row 方案保留 21 個可見列
及上下各 2 個已用列，既沒有增加 ROM／EWRAM buffer，也把柔性鏡頭的
VBlank 結果降到與固定 crop 相同，因此定案保留。

另外用 `gemini-3.1-pro-preview` 進行兩輪審查。模型提出 pixel-phase
預取與雙向 buffer；但在 25-row 方案已達固定 crop 基線、且雙 buffer
會新增 reference ownership 與 upload queue 競爭的前提下，沒有繼續
增加複雜度。模型建議玩家 source Y 擴到 `5..164`，經直接核對
OpenTyrian `JE_playerMovement()` 後未採用；單人原始規格是
`10..160`，release 已恢復這組精確邊界。鏡頭只依玩家座標單向跟隨，
不把 camera state 回饋至碰撞座標。

32×32 玩家 container 的有效像素在兩個極端仍完整可見：

- `playerY=10`、camera origin Y=0：第一個有效像素為 screen Y=5；
- `playerY=160`、camera origin Y=24：最後有效像素為 screen Y=155。

編譯期 `_Static_assert` 會阻止後續 layout 變更破壞這個安全條件。
