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
- 12,168 display frames 中 missed VBlank：17；
- ROM 約 25.47 MiB，仍低於 32 MiB。

Data／Ship Specs 的 campaign parser 與 staged transition 修正會在同一
v56 分支的下一階段完成。
