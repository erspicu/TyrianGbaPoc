# GBA Detail Level 畫面特效 ARM 組語優化比較報告

日期：2026-08-07
狀態：實作、差分驗證與 A/B 量測完成

## 結論

本次把三段真正屬於 CPU 熱迴圈、且能保持逐位元一致的 Detail Effect
工作改為 ARM7TDMI 組語：

1. 256-entry effect palette LUT 建立與 BG／OBJ palette mapping。
2. 161-line spotlight `WIN0H` table 建立。
3. 161×4 halfword lava／water wave table 建立。

同一 ROM 的 microbenchmark 顯示三者分別降低約 **13.33%**、**60.98%**、
**32.93%** 的週期。LAVA EXIT 固定工作量壓力路徑的完整 render 成本下降
**0.86%**，loop 總工作量下降 **0.32%**。這是正效益，但並未跨過該路徑
目前的排程門檻，所以 300 個 display VBlank 內的 missed VBlank 仍是 59；
不能把局部 kernel 的高百分比誤寫成整個 GameLoop 同等幅度加速。

所有 release build 預設啟用 `DETAIL_EFFECT_ASM=1`；純 C reference 保留供
診斷及 differential test 使用。

## 實作內容

### Palette kernel

- 先由 ARM 建立 256 個最終 GBA 15-bit 色彩的 LUT。
- 再以四個 index 一批的 load／lookup／兩個 32-bit store 映射 BG 或 OBJ。
- 支援 `NONE`、global hue、water hue、brightness -15～15。
- 保持 gameplay OBJ palette banks 9～12、14 的原始保護規則。
- 同一條路徑供 NORMAL 的 iced、HIGH 的 lava／water、PENTIUM／CUSTOM 的
  final hue／brightness filtration 使用。

### Spotlight kernel

- 將每條 scanline 的 radius、left/right clamp 與 `WIN0H` packing 放在 ARM
  暫存器完成。
- 一次線性寫入 160 條可見 scanline，再寫入第 161 筆 DMA sentinel。
- 放在 IWRAM；CUSTOM／LOW release 不會連結這個 object section。

### Wave kernel

- 將 `strength_q8` 的 zero、full、scaled 路徑移到迴圈外分派。
- 每條 scanline 用兩個 32-bit store 寫完 BG0／BG1 的 HOFS／VOFS。
- signed Q8 rounding、負位移及 16-bit hardware wrap 均保持 C 語意。
- 放在 IWRAM；只有 HIGH／PENTIUM release 會保留。

### 建置隔離

- 新增 `DETAIL_EFFECT_ASM=0/1`，預設為 1。
- build artifact suffix 包含 `_detailasm0`／`_detailasm1`，避免 C 與 ARM
  object 因舊檔時間戳而交叉污染 A/B 結果。
- palette、spotlight、wave 分成三個 assembly object，讓 linker 能按
  Detail capability 個別移除未使用段落。

## Same-ROM microbenchmark

量測來源：HIGH、Episode 4 Section 31、LAVA EXIT 壓力測試 ROM。測量時
關閉 IRQ，C 與 ARM 依序在同一個 binary、相同輸入與相同 cycle counter
執行，避免兩次模擬器啟動造成的環境差異。

| Kernel | 呼叫次數 | C 總 cycles | ARM 總 cycles | C／call | ARM／call | 每次節省 | 改善 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Effect palette | 64 | 2,646,994 | 2,294,038 | 41,359.3 | 35,844.3 | 5,514.9 | 13.33% |
| Spotlight table | 512 | 5,254,403 | 2,050,222 | 10,262.5 | 4,004.3 | 6,258.2 | 60.98% |
| Wave table | 512 | 5,209,996 | 3,494,294 | 10,175.8 | 6,824.8 | 3,351.0 | 32.93% |

Palette 的數字包含 LUT 建立、256 BG entries 與開啟時的 256 OBJ entries，
因此比單純 161-entry scanline table 大。Spotlight 的改善最高，主因是 ARM
條件執行避開 C 版本每條 scanline 的小函式及多層 clamp 分支。

## LAVA EXIT 完整路徑 A/B

固定條件：

- HIGH、Game Speed NORMAL。
- Episode 4、Section 31、LVL 9 `LAVA EXIT`。
- 壓力武器模式。
- `active_mask_fast_wall_lazy_packed_no_adaptive`，刻意關閉 adaptive，確保
  C／ARM 兩邊做相同數量的 wave 工作。
- 300 display VBlank；唯一變數是 `DETAIL_EFFECT_ASM=0/1`。

| 指標 | 純 C | ARM | 差異 |
|---|---:|---:|---:|
| Display VBlank | 300 | 300 | 相同 |
| Logic updates | 174 | 174 | 相同 |
| Completed renders | 123 | 123 | 相同 |
| Deferred renders | 121 | 121 | 相同 |
| Water/wave frames | 239 | 239 | 相同工作量 |
| Missed VBlank | 59 | 59 | 未跨排程門檻 |
| Render cycles total | 24,683,513 | 24,470,481 | -213,032（-0.86%） |
| Render cycles／completed render | 200,679.0 | 198,947.0 | -1,732.0（-0.86%） |
| Pre-logic cycles total | 19,096,964 | 18,976,274 | -120,690（-0.63%） |
| Loop work cycles total | 65,789,426 | 65,577,332 | -212,094（-0.32%） |
| Audio frame loss | 0 | 0 | 相同 |

兩邊的最終 `level_position=174`、active player shots 74、最大 OAM 128、
最大可見敵人 5，也一致。組語沒有藉由少算物件、少更新邏輯或降低特效
強度取得速度。

正式 adaptive 路徑在持續超載時會衰減或停用 wave，所以短時間整體 A/B
容易被 adaptive 的節流效果遮蔽。上表使用 no-adaptive 只為隔離固定工作量；
正式 ROM 仍保留既有 adaptive/drop-frame 機制，沒有改變最低 15 FPS 邊界。

## Differential test

測試不是只比較截圖，而是對 C 與 ARM output buffer 做 `memcmp`：

- Palette：所有 effect kind、brightness -15～15、OBJ filter 開／關，另掃過
  global/water 的 16 種 replacement hue。
- Spotlight：9 個 center（含畫面外）× 7 個 apex（含上下邊界）。
- Wave：`[-3,3]` signed profile、strength 0／1／8／159／160／255／256，
  加上三組 HOFS／VOFS 正常值與 wrap 邊界。

| Profile 測試 | Differential mask | Adapter self-test | Asset validation | Audio loss |
|---|---:|---:|---:|---:|
| LOW | 3/3 | pass | pass | 0 |
| NORMAL | 3/3 | pass | pass | 0 |
| HIGH | 7/7 | pass | pass | 0 |
| PENTIUM | 7/7 | pass | pass | 0 |
| CUSTOM | 3/3 | pass | pass | 0 |

mask bit 0/1/2 分別代表 palette、spotlight、wave。沒有 lava／water capability
的 profile 不編譯 wave reference test，所以期望值是 3，不是失敗。

## Release 資源成本

以下是相同 profile、相同工具鏈的純 C 與 ARM release ELF 比較。`text`
包含龐大的嵌入式 ROMFS，因此小幅正負變化也受 section layout／padding
影響；重點是 IWRAM/EWRAM 是否只由真正使用該功能的 profile 承擔。

| Profile | text 淨變化 | IWRAM 淨變化 | EWRAM 淨變化 | 判讀 |
|---|---:|---:|---:|---|
| LOW | 0 B | 0 B | 0 B | 三個 kernel 全部被 linker 移除 |
| NORMAL | +232 B | +112 B | +512 B | palette + spotlight |
| HIGH | +136 B | +328 B | +512 B | palette + spotlight + wave |
| PENTIUM | -40 B | +328 B | +512 B | ARM 取代的 C code 較大，淨 text 略降 |
| CUSTOM | 0 B | 0 B | +512 B | 只保留 ROM 中的 palette kernel |

512 B EWRAM 是 256-entry 16-bit final-colour LUT。HIGH ARM 的 IWRAM 使用量
由 17,136 B 增為 17,464 B；加上既有 10,276 B IWRAM `.bss` 後仍在 32 KiB
實體範圍內。這次沒有縮減既有 stack canary 門檻來換取組語空間。

## 沒有改成組語的項目

- 半透明爆炸與 wild 50/50 Alpha：真正工作由 GBA colour-effects unit 做，
  CPU 只有少數寄存器／OAM bit 寫入。
- BLDY brightness：同樣是 PPU 硬體路徑。
- HBlank DMA 啟動：每幀只有數筆暫存器設定，組語沒有實質收益。
- 玩家／子彈陰影：主要限制是 OAM、sprite cache 與仲裁，不是算術迴圈。
- blur：目前沒有錯誤的 CPU pixel blur pass，因此沒有可加速的迴圈。
- 垂直翻轉：事件稀少，最多掃過 128 筆 OAM，不值得占用更多 IWRAM。

## 重現方式

純 C／ARM 完整路徑 A/B：

```powershell
.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel high `
  -Variant active_mask_fast_wall_lazy_packed_no_adaptive `
  -Episode 4 -Section 31 -DurationVBlanks 300 `
  -DetailEffectAsm 0

.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel high `
  -Variant active_mask_fast_wall_lazy_packed_no_adaptive `
  -Episode 4 -Section 31 -DurationVBlanks 300 `
  -DetailEffectAsm 1
```

正式建置不需額外參數，Makefile 與 release script 預設使用 ARM 路徑。

## 最終判定

這次三項組語實作均有正效益且保持逐位元一致，值得納入正式版。它們是
降低 Detail presentation 負荷的局部優化，不是取代 adaptive/drop-frame、
OAM 管理或 cache 改善的萬靈丹。最合理的正式策略是保留這些 ARM kernel，
同時維持 CUSTOM profile 與既有全域負載管理。
