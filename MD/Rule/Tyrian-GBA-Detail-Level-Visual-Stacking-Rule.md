# TyrianGbaPoc Detail Level 畫面效果累加規則

日期：2026-08-05
狀態：目前正式實作規則
適用選項：`LOW`、`NORMAL`、`HIGH`、`PENTIUM`、`CUSTOM`

## 核心結論

GBA 版的五種 Detail Level 是編譯期 profile。前四種遵循 PC profile
逐層增加；`CUSTOM` 則由 `NORMAL` 分支：

```text
LOW（386 基礎）
└─ NORMAL（486）
   + 第二背景層
   + 半透明爆炸
   + 主角／玩家子彈陰影
   + 亮暗、iced 與特殊聚光
   + blur 事件與時序（無假 Mosaic）
   ├─ CUSTOM（GBA 專屬）
   │  + wild 第二背景層平滑 50/50 Alpha
   │  + 最終 hue／brightness filtration
   │  - 特殊 code 2 三角聚光
   │  - lava／water 色相效果
   │  - lava／water 掃描線波動
   └─ HIGH（High Detail）
      + lava／water 色相效果
      + lava／water 掃描線波動
      └─ PENTIUM（Pentium）
         + wild 第二背景層平滑 50/50 Alpha
         + 最終 hue／brightness filtration
```

「累加」代表效果功能及事件 gate 逐層開放，不代表所有效果能在每一幀
同時疊上去。實際是否出現仍由關卡 event、`smoothies[]`、背景層狀態與
GBA 單一 colour-effects unit 的仲裁決定。

## 1. Profile 與 PC 原始設定對應

| GBA profile | PC `processorType` | PC profile |
|---|---:|---|
| `LOW` | 1 | 386 |
| `NORMAL` | 2 | 486 |
| `HIGH` | 3 | High Detail |
| `PENTIUM` | 4 | Pentium |
| `CUSTOM` | — | GBA 專屬：Normal + 選定的 Pentium 效果 |

目前 GBA 原始碼不再用數值大小推論所有功能，而是以：

- `>= NORMAL` 開放標準視覺效果。
- `TYRIAN_GBA_DETAIL_HAS_SPOTLIGHT` 只開放 `NORMAL`／`HIGH`／`PENTIUM`。
- `TYRIAN_GBA_DETAIL_HAS_LAVA_WATER` 只開放 `HIGH`／`PENTIUM`。
- `TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA` 開放 `PENTIUM`／`CUSTOM`。
- `TYRIAN_GBA_DETAIL_HAS_FINAL_FILTER` 開放 `PENTIUM`／`CUSTOM`。

PC 的四組初始化並非所有旗標都做位元式繼承。例如 High 會設定
`smoothScroll=false`，Pentium 則保留預設 `smoothScroll=true`。GBA 已使用
自己的 30 Hz 邏輯、60 Hz LCD 與 Drop Frame 呈現排程，因此不能把 PC
的 `smoothScroll` 直接解讀成 GBA 新增或刪除一種畫面濾鏡。

程式依據：`Configure.h`、`src/port_config.h`、
`vendor/opentyrian/src/config.c::JE_initProcessorType()`。

## 2. 五級效果總表

| 畫面規則／效果 | LOW | NORMAL | HIGH | PENTIUM | CUSTOM |
|---|---|---|---|---|---|
| 關卡開始即啟用第二背景層 BG2 | 否 | 是 | 是 | 是 | 是 |
| `background2over==3` 事件恢復 BG2 | 是 | 是 | 是 | 是 | 是 |
| PC 背景 draw-stage／前後層次 | 事件啟用後使用 | 完整 | 完整 | 完整 | 完整 |
| 一般爆炸與 pickup 爆炸半透明 | 不透明 | 8:8 Alpha | 8:8 Alpha | 8:8 Alpha | 8:8 Alpha |
| 主角／玩家子彈的 BG2 視差陰影 | BG2 被事件恢復後可用 | 可用 | 可用 | wild 時讓位 | wild 時讓位 |
| 關卡 brightness fade | 關閉 | GBA BLDY | GBA BLDY | palette brightness | palette brightness |
| iced 藍色色相 pass | 關閉 | palette adapter | 同 Normal | 同 Normal | 同 Normal |
| blur 事件 gate／時序 | 關閉 | 保留 | 保留 | 保留 | 保留 |
| PC 跨影格 blur 的逐像素結果 | 無 | 無硬體等價 | 無硬體等價 | 無硬體等價 | 無硬體等價 |
| 特殊 code 1：整個世界上下反轉 | 是 | 是 | 是 | 是 | 是 |
| 特殊 code 2：三角聚光區 | 否 | 是 | 是 | 是 | **否** |
| lava palette 與波紋 | 否 | 否 | 是 | 是 | **否** |
| water palette 與波紋 | 否 | 否 | 是 | 是 | **否** |
| wild BG2 混色 | 否 | 否 | 否 | 平滑 8:8 Alpha | 平滑 8:8 Alpha |
| 關卡最終 hue filtration | 否 | 否 | 否 | 是 | 是 |
| `SuperWild` darken path | 否 | 否 | 否 | 否 | 否 |

`PENTIUM` 對應 PC processor type 4，不是 processor type 6，因此絕不能
把 `SuperWild` 的 `JE_darkenBackground()` 額外塞進 Pentium profile。

## 3. LOW：所有 profile 的基礎

LOW 以 PC 386 profile 為基準，目標是減少背景 tile cache、OAM、調色盤
與合成器負荷。

### 保留的畫面功能

- 第一與第三背景層及其視差、捲動與來源 draw-stage。
- 敵人、Boss、子彈、爆炸、獎賞和 HUD 的完整遊戲邏輯。
- `starShowVGASpecialCode==1` 的完整世界上下反轉：背景 ring row、tile
  `VFLIP` 與 world OBJ 一起反轉，HUD 保持可讀。
- 關卡若觸發 `background2over==3`，仍會依 PC 規則準備並恢復第二背景層，
  而且之後 `background2` 會保持啟用。
- BG2 被事件恢復且沒有其他硬體效果衝突時，主角和玩家子彈陰影也可恢復。

### LOW 關閉的畫面功能

- 關卡開始時不建立／顯示第二背景層。
- 爆炸使用不透明 OBJ。
- 不執行 brightness、iced、blur、特殊 code 2 聚光。
- 不執行 lava、water、wild 或 final hue filtration。

LOW 不是「永遠只有兩層背景」，也不是刪除 gameplay 內容；它主要縮減
presentation path。

## 4. NORMAL：在 LOW 上增加的效果

NORMAL 是目前正式建置的建議平衡值，也是 `Configure.h` 的預設。

### 4.1 完整第二背景層

- 關卡開始便啟用 BG2。
- 保留 `background2over`、`background3over` 及敵人／地面物件插入前後層
  的來源關係。
- 依 PC 的 smoothie gate 決定特定效果期間是否暫時抑制 BG2。

這是 LOW 到 NORMAL 最明顯的畫面差異：雲層、前景遮罩、中景設施與
多層視差會在一般關卡中持續存在，不必等待特殊事件開啟。

### 4.2 半透明爆炸

- 4bpp 一般爆炸與動態 Sprite2 pickup 爆炸改成 semi-transparent OBJ。
- 使用 `BLDALPHA=8:8` 與已繪製背景做 50/50 混合。
- LOW 則使用完全不透明的相同圖形。

### 4.3 主角與玩家子彈陰影

- 依 PC 的 BG2 視差位置產生飛機與玩家子彈暗色陰影。
- GBA 以 OBJ-window mask 暗化下方背景，而不是再烘焙一套暗色 Sprite。
- spotlight、全畫面 brightness 或 Pentium wild Alpha 占用相同硬體資源時，
  局部陰影會讓位給較高順位的全畫面效果。

### 4.4 關卡亮暗效果

- `levelBrightness` 的淡入、淡出、變亮與變暗由 GBA `BLDY` 實作。
- 正負亮度分別選擇 brighten／darken。
- LOW 依 PC `explosionTransparent=false` 規則不開這條效果。

### 4.5 Iced 效果

- 依來源 Tyrian palette index，把場景映射到 0x80～0x8f 的藍色色階。
- 關卡已訓練的 GBA palette 會先反查來源 hue／brightness，再建立 effect
  palette，避免對已量化顏色反覆套濾鏡而累積誤差。
- 來源 draw-stage 若要求 iced 影響 world OBJ，對應 gameplay OBJ palette
  也會一起更新；HUD／提示色盤不受污染。

### 4.6 Blur 事件

- 保留 PC blur event、啟用門檻、時序與 telemetry。
- PC 是跨影格 framebuffer 亮度平均；GBA Mode 0 沒有無失真的廉價等價。
- 現行版本不再使用 2x2 Mosaic 假裝 blur，因為那會把背景及之後才畫的
  OBJ 全部變成粗顆粒。

因此 NORMAL 的 blur 是「規則已接通，但沒有偽造一個錯誤的可見模糊」，
不能在比較表中誤寫成已經像素等價完成。

### 4.7 特殊三角聚光

- `starShowVGASpecialCode==2` 只在 NORMAL／HIGH／PENTIUM 啟用；CUSTOM
  依專案規格明確關閉。
- 以 WIN0 建立隨玩家位置變化的三角光照區。
- 雙緩衝 161-line `WIN0H` 表透過 DMA0 在 HBlank 串流，避免 160 次 CPU IRQ。

## 5. HIGH：在 NORMAL 上增加的效果

HIGH 開放 PC `processorType > 2` 的 lava／water 路徑。

### 5.1 Lava

- 將場景轉向來源 hue 7 的火焰／熔岩色系。
- 依 `smoothie_data[0]` 判斷濾鏡位於 ground enemies 之前或之後；後段路徑
  會把對應 world OBJ 一起納入，保留 PC draw-stage 意圖。
- 以逐掃描線水平位移形成熔岩扭曲波動。

### 5.2 Water

- 依 `smoothie_data[1]` 指定色相。
- 只改寫符合來源 low/high nibble 規則的顏色，不把所有顏色強制染成同色。
- 使用較平緩的逐掃描線水平波動。

### 5.3 GBA 硬體適配

- 雙緩衝 `161 x 4 halfword` 表記錄 BG0／BG1 的 HOFS／VOFS。
- DMA0 在每條 HBlank 串流下一列捲動值。
- CPU 準備 inactive table，LCD 使用 active table；Drop Frame 時不會顯示
  半張舊波紋、半張新波紋。
- lava／water palette 仍走可逆的來源 hue／brightness adapter。

HIGH 並不是所有關卡永遠更花成本；只有關卡 event 真正啟用 lava／water
時，才會支付波紋表與調色盤切換成本。

## 6. PENTIUM：在 HIGH 上增加的效果

### 6.1 Wild 第二背景層混色

- 一般 BG2 不再只是 opaque layer，而是以 GBA colour-effects unit 做
  `BLDALPHA=8:8`，平滑地與底下所有可見層 50/50 混合。
- 這是第一章第一關透明雲層的主要效果。
- `background2_not_transparent`（PC event 48）或 `background2over==3`
  生效時，立即回到不透明 BG2，不殘留舊 wild cache。
- water 的 opaque 路徑依 PC 規則與 BG1 同步 X；wild 路徑保留 BG2 自己的
  視差位置。

先前版本曾用棋盤挖洞模擬 50% 透明；目前正常 PENTIUM 路徑已改為平滑
硬體 Alpha。只有特殊三角聚光同時占用唯一 colour-effects unit 時，才會
原子切換到 50% ordered-dither 相容 fallback。

### 6.2 Wild 與陰影的取捨

GBA 只有一組 colour-effects unit。平滑 wild BG2 啟用時，主角／玩家子彈
的 OBJ-window 陰影暫停送出，優先保持整片雲層的 PC 觀感。透明爆炸仍可
使用相同的 8:8 second-target 組合。

### 6.3 最終 Filtration

- PC `JE_filterScreen()` 位於 world／projectile 之後、Boss bar／HUD 之前。
- PENTIUM 才開放 `levelFilter` 的全畫面 hue replacement。
- `levelBrightness` 不再占用 BLDY，而是烘入 effect palette，因此可與
  wild BG2 Alpha 並存。
- BG palette 與 gameplay world OBJ palette 會處理；HUD、提示文字等保留
  原色。
- 入關時以目前訓練後 GBA BG／OBJ palette 反查來源 palette index；後續
  filter tick 只需重建 512 個顏色。

### 6.4 Pentium 仍不是逐像素 PC framebuffer

PC wild、lava、water、iced、blur 與 filtration 會直接讀寫 264x184 的
8-bit framebuffer。GBA Mode 0 版本保留：

- 相同事件與 profile gate。
- 相同 hue／brightness 語意。
- 相同主要 draw-stage 先後關係。
- 以 BG Alpha、palette lookup、OBJ palette 及 HBlank DMA 適配。

但它不宣稱每個輸出像素都與 PC framebuffer 完全一致，尤其 PC 的跨影格
blur 沒有直接硬體等價。

## 7. 同時觸發時的硬體仲裁

各 profile 是「可用功能組合」，不是無條件把所有寄存器效果疊在一起。
目前 full-screen register 仲裁大致為：

1. 特殊三角聚光。
2. NORMAL／HIGH 的關卡 brightness。
3. PENTIUM／CUSTOM 平滑 wild BG2 Alpha。
4. 主角／玩家子彈 OBJ-window 陰影。
5. 半透明 OBJ 的一般 second-target 設定。

palette 型 iced／lava／water／Pentium filtration 依 PC draw order 決定最後的
hue、brightness 與是否影響 world OBJ；scanline wave 可以和 palette
效果並存。特殊聚光使用 DMA0 時，會壓過同時要求的 lava／water wave，
對應程式中較晚的 special-code raster pass。

CUSTOM 不會建立 wave table，也不會啟動 lava／water hue，因此不進入上述
scanline-wave／palette 仲裁；它只保留 Normal 的 iced 等效果及最終 filter。

## 8. 畫面差異的實際判讀

### LOW 與 NORMAL

最容易看出：

- NORMAL 多一層持續存在的雲／中景／前景背景。
- NORMAL 的爆炸有透明感。
- NORMAL 可見飛機與子彈陰影。
- 特定關卡會有藍色 iced、亮暗 fade 與三角聚光。

### NORMAL 與 HIGH

一般關卡可能完全相同；只有帶 lava／water event 的區段才會看見：

- 色相轉換。
- 背景逐掃描線波動。
- 依 draw stage 決定地面物件是否一起受濾鏡影響。

已知實際測試點：Episode 4、Section 31、LVL 9 `LAVA EXIT`。

### HIGH 與 PENTIUM

最容易看出：

- PENTIUM 的第二背景層／雲層使用平滑半透明混色。
- 關卡 filter 可同時改變 hue 與 brightness，且不污染 HUD。
- PENTIUM 在 High 會暫時抑制 BG2 的部分 smoothie 狀態中，依 PC type 4
  規則保留對應背景合成路徑。

### NORMAL 與 CUSTOM

- CUSTOM 的雲／第二背景層取得與 Pentium 相同的平滑 50/50 Alpha。
- CUSTOM 套用關卡最後的 hue／brightness filter。
- CUSTOM 不顯示 `starShowVGASpecialCode==2` 的全畫面暗化與三角聚光。
- 即使關卡資料帶有 lava／water smoothie，CUSTOM 也不啟用其 hue 或掃描線
  波動；這正是用來避開高負荷場景橫條紋的設計邊界。

## 9. CUSTOM：GBA 正式版客製規格

CUSTOM 不是比 Pentium 更高一級，也不是把所有效果全部打開。它是從
NORMAL 分出的 GBA 專屬正式版 profile，目標是在保留大部分高品質畫面
特徵的同時，排除目前容易造成橫條紋、硬體資源競爭或不符合正式版觀感的
特殊效果。

### 9.1 從 NORMAL 繼承

- 關卡開始即使用完整第二背景層及 PC draw-stage 前後層次。
- 一般爆炸與 pickup 爆炸使用 8:8 Alpha。
- 保留主角與玩家子彈的 BG2 視差陰影；若與 wild Alpha 衝突則依硬體
  仲裁規則讓位。
- 保留關卡 brightness、iced palette adapter 與 blur 事件時序。
- 世界上下反轉 `starShowVGASpecialCode==1` 仍依原版事件生效。

### 9.2 從 PENTIUM 選入

- wild 第二背景層使用 GBA colour-effects unit 做平滑 50/50 Alpha，不使用
  棋盤挖洞模擬透明。
- 啟用最終 hue／brightness filtration，並保護 HUD 與提示文字的原色。

### 9.3 明確排除

- 關閉 `starShowVGASpecialCode==2`：不將全畫面壓暗，也不顯示跟隨主角的
  三角聚光範圍。
- 關閉 lava／water palette hue 與逐掃描線波動；關卡事件及 gameplay state
  仍照來源資料推進，只省略這兩種 presentation pass。
- 不啟用只屬於 PC processor type 6 的 `SuperWild` darken path。
- 不以 Mosaic 假裝 PC 跨影格 blur。

### 9.4 硬體與效能邊界

CUSTOM 不會為 spotlight 建立 WIN0 HBlank 串流，也不會為 lava／water 建立
wave table。DMA0 因此不被這兩條效果占用，主要 colour-effects 資源優先
提供 wild BG2 Alpha、半透明 OBJ 與必要的亮暗仲裁。這是編譯期 capability
選擇，不是進入關卡後才把效果旗標關掉。

Detail Level 只改變呈現方式；敵人、Boss、子彈、碰撞、RNG、獎賞、關卡
事件、音訊與 GameLoop 結果不得因 CUSTOM 而改變。

## 10. 建置方式

預設值位於根目錄 `Configure.h`：

```c
#define TYRIAN_GBA_CONFIG_DETAIL_LEVEL TYRIAN_GBA_CONFIG_DETAIL_CUSTOM
```

單次建置可不修改設定檔，直接覆寫：

```powershell
.\Build-GBA-ROM.bat -DetailLevel low
.\Build-GBA-ROM.bat -DetailLevel normal
.\Build-GBA-ROM.bat -DetailLevel high
.\Build-GBA-ROM.bat -DetailLevel pentium
.\Build-GBA-ROM.bat -DetailLevel custom
```

未選用的 Detail 分支會由條件編譯移除，因此 LOW／NORMAL 不會保留
High／Pentium 專用的雙緩衝 wave table；CUSTOM 的 ROM 也不保留該表，這
不是 runtime 只把效果旗標關閉。

## 11. 維護規則

後續修改 Detail Level 時必須遵守：

1. 先核對 OpenTyrian 的 `processorType`、`smoothies[]` 與 draw order，不能
   只依畫面印象自創效果。
2. 新增效果必須放入正確的功能 gate；禁止再以 `CUSTOM > PENTIUM` 的數值
   關係推論 High 功能，否則會誤開 lava／water。
3. LOW 的 `background2over==3`、全畫面上下反轉等來源例外不得誤刪。
4. Blur 沒有像素等價前，不可再用 Mosaic 冒充已完成效果。
5. PENTIUM／CUSTOM wild 正常路徑必須維持平滑 Alpha；checkerboard 只可
   作硬體衝突 fallback。CUSTOM 已關閉 spotlight，因此不得再為 spotlight
   衝突啟用 checkerboard fallback。
6. `SuperWild` 不屬於 Pentium，不得意外啟用。
7. Detail Level 只能改變 presentation；敵人、碰撞、關卡事件、RNG、獎賞
   與 GameLoop 結果必須保持一致。
