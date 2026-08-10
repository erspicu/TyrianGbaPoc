# DREAD-NOT Boss 組件閃爍：Root Cause 與修復

日期：2026-08-08  
測試來源：`build/TyrianGBA.sav`，Episode 4、main section 17、LVL 14
`DREAD-NOT`

## 結論

Boss 組件消失／交替閃爍的主因不是 128 OAM 用完，也不是畫面後方的
藍色流星／星空。真正瓶頸是 8bpp Sprite2 OBJ-VRAM L1 快取：一張
32×32 畫布要 1 KiB，原配置只能同時容納約 24 張；DREAD-NOT 第一段
Boss 畫面卻有 42 個可見 enemy candidates，其中 41 個是 32×32，約
36 張是不同圖幀。舊版因而在同一幀已有 17 個部件拿不到 tile。

舊版還只把 event 79 的直接血條 link 當 Boss。實際組件由 link
14–19、121、141、161、181 等多個群組組合；只有 link 18 的四個核心
部件受到 Boss 優先權保護，其餘部件會與一般 Sprite2 互相逐出。

同一段追蹤的 OAM 高峰為 120／128，而且 enemy OAM cull 為 0，證明
不是 OAM 容量問題。藍色星空由 BG3 sparse-tile overlay 繪製，不占
OBJ OAM 或 Sprite2 L1 格，因此沒有為此降低星星密度。

## 修復設計

1. 從 event 79 的血條 link 當種子，以原始 PC 敵人座標、ground 屬性、
   link 與 32×32 畫布相接關係做 bounded connected-component walk，辨識
   完整多 link Boss 結構；不寫死某一關的 link 清單。
2. 大型 Boss 場景啟用 GBA presentation-only compact cache。將原本
   21 個 1 KiB 8bpp 實體格拆成 42 個 512-byte 4bpp 畫布。
3. 進入 Boss 場景時，用當前關卡 palette 與實際可見 Sprite2 索引訓練
   兩組 15 色 OBJ palette；仍讀同一份無損 ROMFS／Sprite2 raw 資料，
   沒有新增 per-level 圖形表。
4. admission 順序固定為 Boss 核心、完整 Boss 結構、一般敵人。Boss
   佔用後仍有空位時，一般 32×32 敵人也進 compact cache，避免被大型
   Boss 間接擠掉。爆炸與裝飾效果是最先降級／輪替的類別。
5. palette 訓練的 39-byte shape-table scratch 移到 EWRAM，並隔離較深的
   palette helper stack frame，避免 Boss 第一次出現時和既有 render
   stack 疊加而越過 GBA stack 邊界。

這些修改只處理 presentation resource admission；敵人 pool、碰撞、傷害、
事件順序與 Boss gameplay 狀態均未改寫。

## 針對性驗證

第一段 Boss、120-frame trace：

- 可見 enemy candidates：42
- 完整 Boss 結構：41
- 4bpp compact 部件：40（另一個 Boss 部件為較小畫布）
- 已選取：42／42
- `tile == 0xffff`：0

後段 Boss、完整路線 watchdog 截點：

- 可見 enemy candidates：37
- Boss structure：28
- compact-rendered 32×32 objects：36（包括剩餘一般敵人）
- Boss 缺圖：0
- 一般敵人缺圖：0
- 觀測 OAM 高峰：98／128

在把剩餘一般 32×32 敵人一併納入 compact cache 後，同一路線累積
enemy-cache prime failures 由 8,678 降到 5,808（約 33.1%）；殘餘數字
來自此前各階段／切換瞬間的累積值，後段取樣幀內已沒有 Boss 或一般敵人
缺 tile。

## 證據位置

本機診斷輸出保留在 `.toolchain/test-staging/`：

- `savara-boss-resource-trace/boss.png`：修復前，可見 Boss 大塊缺件。
- `savara-boss-compact-stackfix-trace120/final.png`：第一段修復後。
- `savara-boss-late-phase-all-enemy-compact/final.png`：後段與一般敵人
  一併使用 compact cache 的結果。

上述 staging 檔不是 release 資源，不會打包進正式 ROM。

## 追加發現：Boss 移動時仍會失去組件（2026-08-08）

前述 compact cache 解決了 Boss 靜止、組件彼此相接時的容量問題，但
「完整 Boss 結構」仍以**當前畫面座標的相鄰關係**每幀重新推測。這不是
可靠的物件分類：原始 LVL event script 會讓同一個 Boss 的多個 link 群組
分別移動，零件在動畫途中刻意分開，停止時才重新拼合。

移動風險點的 SRAM 診斷快照如下：

- tick：2574；level position：2245；event index：368
- 可見 enemy candidates：31；其中 31 個皆為 32×32 Sprite2
- event 79 直接核心：4
- 當幀空間相鄰法辨識出的 Boss structure：**4**
- moving candidates：7
- compact cache：**未啟用**；compact-rendered：0
- 已選取敵人物件：31；其中 **5 個拿不到 tile**
- 前一畫面 OAM：91／128

因此這次缺件同樣不是 OAM 128 格用完。失效鏈為：Boss 零件移動分離 →
空間 connected-component 由完整身體退化成四個血條核心 → structure 數量
低於 compact-cache 啟用門檻 → 快取立刻停用 → 31 張 1 KiB 畫布重新擠入
普通 8bpp cache → 5 個 Boss／敵人物件缺 tile。Boss 停止、零件重新相接後，
空間分類又恢復，所以使用者看到「移動時消失，停下後出現」。

### 分類原則修正

玩家、玩家子彈、敵方子彈、普通爆炸、拾取爆炸分屬不同的固定資料池，
可以直接由 ownership 做確定分類；不應用座標、圖形編號或 OAM 狀態猜測。
真正共用同一個 pool 的只有普通敵人與 Boss 零件。Boss 的可靠分類必須改為：

1. event 79 的血條 link 僅作權威種子。
2. Boss 拼合時辨識出的成員，綁定到 enemy slot 的**實例 generation**，跨幀
   持久保存，不因座標分離而失去身分。
3. 已確認的 Boss link 可讓後續同 link spawn 繼承身分；slot 被釋放再重用時，
   generation 不同，不能誤繼承舊身分。
4. Boss scene／compact cache 使用生命週期與遲滯，不因單一移動幀的可見數量
   下降而關閉；只有 Boss 流程確實結束才清除。
5. OAM 排程只使用這個持久身分決定優先權；爆炸仍由獨立 effect pool 判定，
   不會被誤升級成 Boss。

移動風險截圖與 SRAM 位於
`.toolchain/test-staging/boss-class-moving-risk-20260808b/`。

## 持久分類修復與同點驗證

正式修復將物件分類拆成兩層：

- 玩家、玩家子彈、敵方子彈、普通爆炸、拾取爆炸直接由 owning pool
  決定，彼此不會因 graphic、座標或 OAM 壓力而換類。
- 普通敵人與 Boss 零件雖共用 `enemy[100]`，每次 slot 由 free 轉 active
  時會取得新的 runtime-only instance generation。Boss membership 保存
  `(slot, generation)`；slot reuse 不會繼承前一隻物件的 Boss 身分。

event 79 的血條 link 仍是權威種子；空間相接只用於第一次辨識組裝完成的
Boss。辨識到的 instance 與 link 在本關持久保存，event 39 改 link 時也由
instance 身分把新 link 納入。換關由 `source_runtime_reset()` 一次清除，不會
跨關污染。compact cache 另加 60 個 presentation frame 的退出遲滯；只要
Boss bar 或持久 Boss 成員仍在，就不會因移動中一幀的數量下降而切回 8bpp。

使用同一份 SAV、同一路線及幾乎相同的移動位置重測：

| 指標 | 修復前 | 修復後 |
|---|---:|---:|
| level position | 2245 | 2246 |
| 可見 enemy／32×32 enemy | 31／31 | 31／31 |
| event 79 core | 4 | 4 |
| Boss structure | 4 | **28** |
| moving candidates | 7 | 7 |
| compact active／compact-rendered | 0／0 | **1／31** |
| selected missing tile | **5** | **0** |
| 前一畫面 OAM | 91／128 | 88／128 |

修復後證據位於
`.toolchain/test-staging/boss-class-persistent-pos2245-20260808/`。數據證明
移動不再撤銷 Boss 身分，並且不需降低星空、爆炸量或 OAM 上限即可消除
該風險幀的組件缺失。

## 血條出現前的全關卡通用身分（2026-08-08）

持久 `(slot, generation)` 解決「辨識過後移動分離」的問題，但第一次種子
仍來自 Event 79。部分關卡在 Boss 血條顯示前已有很長的組裝／登場動畫，
這段期間仍可能使用普通 enemy cache admission。

新增的 `tools/build_boss_manifest.py` 不針對 DREAD-NOT 或任何 Boss 寫死
規則，而是掃描 Episode 1–4 全部 62 個 LVL，追蹤每次 Event 79 activation
之前的 link alias 與 spawn cohort。當前 stock data 自動導出 36 個 Boss
LVL、726 個 spawn events，0 個 unresolved bar links。runtime 在 event spawn
當下就標記 component，Event 79 仍保留作權威血條資料與 fallback。

Episode 4 LVL 14 的針對性 pre-intro 驗證在 level position 104 已有 40 個
active manifest members，而第一個 Event 79 在 position 349；該截點
`enemy_cache_drops=0`。完整生成、驗證與 slot reuse 規則記錄於
`MD/Rule/Tyrian-GBA-Boss-Identity-Manifest-Rule.md`。
