# ASTEROID 1 ARM 熱路徑效能比較（2026-08-07）

> 本文件保留第一階段數據。色距與 packed collision ARM 完成後的最新
> 差分與 A/B 結果請以
> [ARM-Colour-Packed-Collision-Differential-2026-08-07.md](ARM-Colour-Packed-Collision-Differential-2026-08-07.md)
> 為準。

## 測試條件

- Episode 1 / Section 5：ASTEROID 1
- Detail Level：CUSTOM
- Game Speed：Normal
- 壓力武器模式、持續射擊、測試無敵、adaptive/drop-frame：開啟
- 診斷組合：`active_mask_fast_wall_lazy_packed`
- 固定 600 VBlank；兩版皆完成 349 次邏輯更新，並到達相同地圖位置 698
- 修改前：`HOTPATH_ASM=0`，使用原 C 熱路徑
- 修改後：`HOTPATH_ASM=1`，使用 ARM/IWRAM 熱路徑

選擇 600 VBlank 是為了取得嚴格相同的 A/B 視窗。當時觀察到的長時間
卡點後續確認為 IWRAM／stack 配置問題；最新 C、ARM 版均已通過
3,600 VBlank、map position 4,192，修復說明見新版報告。

## 採用的改善

1. MT19937 單步核心改為零 stack 的 ARM/IWRAM 實作，C wrapper 保留
   telemetry 計數與可切換參考路徑。
2. Sprite2 raw palette/filter packing 一次處理四個 pixel，以 32-bit
   回傳並沿用既有對齊寫入。
3. 靜態前端 320x200 → 240x160 的每列縮放，改用 ARM 的
   `LDMIA/STMIA` SWAR 搬移與重組。
4. 新增 bit-exact 自測：全 256 個 pixel/filter 組合、4096 次 RNG
   序列，以及完整 320→240 row 逐 byte 比對。

沒有採用文件中不完整的整段 Sprite2/碰撞 ASM 草稿，也沒有把約
2.5 KiB RNG state 搬入 IWRAM；前者會漏掉既有語意，後者會傷害
stack 安全，風險大於收益。

## 結果

| 指標 | 原 C 版 | ARM 版 | 差異 |
|---|---:|---:|---:|
| RNG 10,000 次平均 cycles/call | 206.67 | 195.63 | **-5.34%** |
| 完成渲染平均 cycles | 364,570.05 | 363,846.72 | **-0.20%** |
| Pre-logic 平均 cycles | 130,352.54 | 129,906.08 | **-0.34%** |
| Prefetch 平均 cycles | 9,222.96 | 9,178.60 | **-0.48%** |
| Missed VBlank | 454 | 449 | **-1.10%** |
| Audio frame loss | 84 | 82 | **-2.38%** |
| 完成 render 數 | 163 | 163 | 不變 |
| Cache capacity drop | 148 | 144 | **-2.70%** |
| 整體 loop 平均 cycles | 259,164.49 | 259,359.40 | +0.08%（雜訊範圍） |
| Logic 平均 cycles | 172,614.67 | 172,770.87 | +0.09%（雜訊範圍） |

ARM 版 `.iwram` 從 `0x5998` 降到 `0x57f0`，釋出 **424 bytes**，
使用者 stack 靜態餘量由 2,160 增至 2,584 bytes。ROM text 亦縮小
432 bytes。

## 定案

改善是局部、可量測且 bit-exact，但 ASTEROID 1 的總負載主要仍在
碰撞、場景與 OAM/cache 壓力，因此整體加速幅度溫和。保留 ARM 版
作為正式預設，同時保留 `HOTPATH_ASM=0` 作精確回歸與除錯用途。
