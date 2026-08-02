# Tyrian GBA Pentium Detail 逐行對應與效能評估（v69）

日期：2026-08-02

## 結論

GBA 版的 `LOW`、`NORMAL`、`HIGH`、`PENTIUM` 現在都是可正式編譯的
OpenTyrian Detail profile。Pentium 不再只是保留名稱或等價猜測：本階段依
PC 原始碼逐項接通 `wild` BG2 混色、最終 `filtration`、High 的
lava／water，以及既有 Normal 的 iced／blur；事件啟用條件與合成先後順序
均跟隨來源程式。

PC 效果直接改寫 264×184 的 8-bit palette-index framebuffer，GBA Mode 0
無法用同一成本逐像素讀回已合成畫面。因此本版的「完整功能」是指完整保留
來源事件、Detail gate、顏色語意與 draw-stage 順序，再以 tile dither、
palette lookup、OBJ palette 與 HBlank DMA 做硬體適配；不能誤稱為每個輸出
像素都和 PC framebuffer 相同。

Episode 2 第一關的 60 秒極限武器測試中，Pentium 為 873／3,600 missed
VBlank（24.25%），Normal 為 865／3,600（24.03%），只差 8 個 frame、
0.22 個百分點。這是刻意讓 OAM、投射物與快取同時飽和的破壞性壓力案例，
不是一般遊玩負載；正式 drop-frame 排程仍維持相同遊戲時間、亂數與
2,096 次邏輯更新。

## PC 原始碼逐項核對

### Profile 初始化

`vendor/opentyrian/src/config.c:527` 的 `JE_initProcessorType()` 定義：

| GBA profile | PC processorType | 來源規則 |
|---|---:|---|
| Low | 1 / 386 | 關閉起始 BG2、score 與透明爆炸 |
| Normal | 2 / 486 | 保留 BG2、透明爆炸及標準效果 |
| High | 3 / High Detail | 承接 Normal，開放 processorType > 2 的效果 gate |
| Pentium | 4 / Pentium | `wild=true`、`filtrationAvail=true` |

`superWild` 只屬於 processorType 6；Pentium 不會錯誤啟用
`JE_darkenBackground()` 的 SuperWild 路徑。

### Gameplay gate 與順序

逐行比對 `vendor/opentyrian/src/tyrian2.c:1298-1400`、
`1968-1976`、`2320-2324` 及 `backgrnd.c:109-151`、`257-312`：

1. Normal 以上執行 iced／blur 與 special-light 類效果。
2. High 以上執行 lava／water；water 的 opaque BG2 會和 BG1 同步 X，
   wild blend 則刻意保留 BG2 自己的 X 視差。
3. Pentium 的 wild BG2 只在非透明禁止、非 `background2over==3` 的正常
   合成階段使用。來源像素保留 BG2 hue 高半位元，亮度低半位元取目前
   framebuffer 與 BG2 的平均。
4. event 48 將 `background2notTransparent` 設為 true，之後必須立刻回到
   opaque BG2，不能留下舊 wild cache。
5. `JE_filterScreen()` 的 hue 只在 `filtrationAvail` 生效；brightness 只在
   `explosionTransparent` 生效。它位於 world／projectile 後、boss bar 與
   compact HUD 前，HUD 不應被關卡 filter 污染。
6. 合成順序維持：pre-iced → BG2 → pre-lava／water → ground enemies →
   post-lava → iced／blur → 後段 world → final filtration → boss／HUD。

## GBA 硬體適配

### Pentium wild BG2

- BG2 tile cache key 帶有 wild 語意位元，opaque 與 wild 不會誤共用 tile。
- build/render tile 時加入固定 50% ordered dither，讓 BG1 與 BG2 在空間上
  混合；不占用唯一的 GBA colour-effects unit，因此 OBJ-window 陰影與透明
  爆炸仍能正常工作。
- event 48 或 wild 狀態切換會重建目前 BG2 ring，再於 VBlank 原子上傳，
  不顯示半張新、半張舊的圖。

### High lava／water

- 兩份 `161 × 4 halfword` 掃描線表由 DMA0 在 HBlank 串流
  BG0HOFS／BG0VOFS／BG1HOFS／BG1VOFS。
- CPU 只在要呈現的新 scene 準備 inactive table，LCD 使用 active table；
  drop frame 不會撕裂波紋。
- lava 使用來源 hue 7；water 依來源 low/high nibble 規則做選擇性 hue。
- Low／Normal 在編譯期完全移除這份 2,576-byte 波紋表，不保留無法觸發的
  EWRAM 資料。

### Pentium filtration

- 入關時把目前訓練後的 GBA BG／OBJ palette 反查為來源 hue＋brightness
  index。brightness 由 GBA luminance 估算，再只搜尋 16 個來源 hue，成本
  為 `256 × 16`，不是 `256 × 256`。
- 後續 fade／filter tick 只需重建 512 個顏色；iced、lava、water、最終
  filtration 共用同一條可逆 palette adapter。
- world OBJ banks 會被處理，HUD／提示 bank 保留；boss／secret palette
  後續改寫後會重新套用 active effect。
- Pentium brightness 烘入 effect palette，避免和 OBJ-window 陰影爭用
  BLDY／BLDALPHA。

## 自動驗證

壓力 ROM 在量測完成後才執行 Detail adapter self-test，因此不污染 cycle
數據。四檔都回報 `detail_adapter_self_test=1`：

- Low：所有高階要求均關閉。
- Normal：iced／blur／spotlight gate 正確，無 lava／water／wild。
- High：lava／water gate 與 palette／wave adapter 可啟用。
- Pentium：water 選擇性 hue、final filtration 覆寫、wild 及 event 48
  取消 wild 均通過。

另以 Pentium 跑完整第一關 deterministic autotest：`PASS=1`。調整 IWRAM
配置後，stack canary 初始化 2,604 bytes，最深路徑仍留下 900 bytes，guard
為 1。敵方子彈更新改留在 Thumb ROM；本案例只有 74 次 active-shot 更新，
把原本占用的 938 bytes IWRAM 留給 Sprite2、背景解碼、Maxmod 與 stack，
效益明顯較高。

## Episode 2 第一關極限壓力測試

### 固定條件

- 目標：Episode 2、section 1。
- 時間：3,600 個實際 LCD frame，約 60 秒。
- Game Speed：Normal。
- 玩家：無敵模式。
- 武器：stress loadout，主武器、左右 sidekick 與額外高密度投射物全開。
- 正式正向機制：active collision mask、packed collision、precache cull、
  lazy result、wall-clock logic、dynamic drop frame、missed-VBlank recovery。
- 執行器：專案內 mGBA headless；四檔由同一份最終 source 建置。

四檔的 gameplay invariant 完全一致：

| 指標 | 四檔共同值 |
|---|---:|
| Display frames | 3,600 |
| Logic updates | 2,096 |
| Player-shot spawns／drops | 12,529／633 |
| Max active player shots | 81 |
| Max OAM | 128 |
| RNG calls | 6,289 |
| Enemy motion updates | 17,326 |
| Collision candidate visits | 730,265 |
| Source assets valid／loadout failures | 1／0 |

### 主要結果

| Detail | Missed VBlank | 完整呈現 scene | Logic avg cycles | 完整 render avg | Render max | Audio loss |
|---|---:|---:|---:|---:|---:|---:|
| Low | 892 (24.78%) | 1,421 (39.47%) | 137,298.24 | 175,672.55 | 342,560 | 8 (0.22%) |
| Normal | 865 (24.03%) | 1,432 (39.78%) | 137,287.65 | 167,732.93 | 381,996 | 9 (0.25%) |
| High | 875 (24.31%) | 1,430 (39.72%) | 137,288.09 | 167,045.45 | 381,945 | 10 (0.28%) |
| Pentium | 873 (24.25%) | 1,426 (39.61%) | 137,287.86 | 166,474.20 | 358,123 | 6 (0.17%) |

| Detail | Collision avg | Pre-logic avg | Commit avg | Projectile cache misses | Visible capacity drops |
|---|---:|---:|---:|---:|---:|
| Low | 58,299.03 | 72,411.11 | 10,261.90 | 4,394 | 1,665 |
| Normal | 58,298.78 | 70,551.55 | 9,990.96 | 2,883 | 345 |
| High | 58,298.62 | 70,613.54 | 10,063.26 | 2,903 | 340 |
| Pentium | 58,298.71 | 71,043.87 | 10,167.74 | 2,863 | 320 |

Pentium 在本關實際執行 2,727 個 wild-dither frame，初始 filter fade 造成
9 次 palette rebuild。此關沒有定義 lava／water／iced／blur 事件，因此
High 的新增效果成本沒有被這一關觸發；這些分支由後置 self-test 驗證，
不能把本表解讀為 lava／water 永遠零成本。

Low 在極限武器下反而略慢不是 Detail gate 失效。Low 移除 BG2 陰影與部分
結構性 OAM 後，更多武器投射物通過前段可見性篩選，令 projectile cache
miss 增至 4,394、visible capacity drop 增至 1,665；Normal 以上會較早被
OAM／結構限制裁掉。四檔 collision 與 logic 數據幾乎相同，證明差異落在
presentation/cache 壓力，不是遊戲規則漂移。

### ROM 與記憶體

| Detail | ROM bytes | ROM MiB | EWRAM heap 餘量 | IWRAM user-stack 靜態區間 |
|---|---:|---:|---:|---:|
| Low | 28,460,760 | 27.142 | 15,384 | 3,376 |
| Normal | 28,462,584 | 27.144 | 14,872 | 3,368 |
| High | 28,463,768 | 27.145 | 12,296 | 3,368 |
| Pentium | 28,465,648 | 27.147 | 12,296 | 3,136 |

Pentium 距 32 MiB cartridge 上限仍有 5,088,784 bytes（約 4.85 MiB）。
High／Pentium 的 EWRAM 差異主要是雙緩衝掃描線波紋表；Pentium 額外 code
與 palette 適配沒有再消耗一份大型 EWRAM buffer。

## 判讀與建議

1. `NORMAL` 仍建議作正式預設：已保留主要層次與透明效果，容量及 stack
   餘裕最大，且一般關卡不是此處的全武器壓力負載。
2. `HIGH` 已是正式可用 profile；遇到 lava／water 關卡時會付出額外
   palette／scanline 成本。
3. `PENTIUM` 適合最高視覺與技術展示。EP2 第一關極限測試相對 Normal
   只多 8 次 missed VBlank，證明 wild／filtration 適配本身不是主要瓶頸。
4. 目前極限瓶頸仍是 128 OAM、動態 Sprite2／projectile cache 與滿畫面
   武器組合。drop-frame 正確維持遊戲節奏，但 24% missed VBlank 代表這套
   刻意破壞性的 loadout 不應作為正式預設武器配置。
5. 若要量測 High／Pentium 的最壞濾鏡成本，下一份測試應選實際含
   lava／water 的關卡，不應用 EP2 第一關的零事件數據外推。

## 建置與重現

預設仍由 `Configure.h` 選擇 Normal。單次覆寫：

```powershell
.\Build-GBA-ROM.bat -DetailLevel low
.\Build-GBA-ROM.bat -DetailLevel normal
.\Build-GBA-ROM.bat -DetailLevel high
.\Build-GBA-ROM.bat -DetailLevel pentium
```

壓力矩陣：

```powershell
.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel pentium `
  -Variant active_mask_fast_wall_lazy_packed
```

原始 telemetry 位於 `build/`，檔名為
`tyrian_gba_full_loadout_sprite_stress_ep2_v36_active_mask_fast_wall_lazy_packed_detail_<profile>_speed_normal_telemetry.json`。
