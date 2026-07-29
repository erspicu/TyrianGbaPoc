# Tyrian GBA v20：PC 圖層順序與建物調色盤直譯

> 歷史記錄：v20 的 PC layer-order 規則仍由 v21 沿用；本文件後半的
> 4bpp structure 專用 palette／198-frame catalog 已由
> [v21 runtime raw Sprite2 pipeline](Tyrian-GBA-Runtime-Sprite2-v21.md)
> 取代，不再是目前 build 的資源路徑。

更新日期：2026-07-26

## 問題

v19 已把第一關的事件、敵人 pool、座標、圖像編號與三層背景帶到 GBA，
但 scene renderer 仍使用固定的 GBA priority：

- ground pool 固定為 OBJ priority 2；
- MAP2/MAP3 只做簡化的前後切換；
- 沒有完整採用 OpenTyrian 每幀的 software-render blit 順序。

GBA 在 OBJ 與 BG priority 相同時由 OBJ 顯示在前。因此 ground OBJ 與
雲層同為 priority 2 時，原本應在雲下的可破壞建物會錯誤蓋到雲上。
這不是遺漏 enemy pool，而是 GBA presentation adapter 沒有翻譯 PC 的
完整 draw order。

使用者標示的 48×56 結構是 LVL event type 12
`Custom 4x4 Ground Enemy`。它由四個 24×28 Sprite2 component 組成，
仍位於 PC ground pool；不應移到 sky/top pool 來規避問題。

## PC 原始繪製順序

依工作區 OpenTyrian commit
`1c34d1bddac8c8f2de834229d04b5a729525c944` 的
`src/tyrian2.c`，同一 framebuffer 中較晚的 blit 會覆蓋較早的 blit。
移植版把相關順序編成以下單調遞增 stage：

| Stage | PC 繪製內容 |
|---:|---|
| 0 | MAP1／background 1 |
| 10 | `background2over=0/3` 時的 MAP2 |
| 20 | `JE_drawEnemy(50)` ground pool 25..49，再畫 `JE_drawEnemy(100)` ground2 pool 75..99 |
| 30 | `background2over=1` 時的 MAP2 |
| 40 | `background3over=2` 時的 MAP3 |
| 50 | 一般 `JE_drawEnemy(25)` sky pool 0..24 |
| 60 | `background3over=0` 時的 MAP3 |
| 70 | 一般 `JE_drawEnemy(75)` top pool 50..74 |
| 80 | 玩家子彈 |
| 90 | 玩家 |
| 100 | 敵人子彈 |
| 110 | `background3over=1` 時的 MAP3 |
| 120 | `topEnemyOver=true` 時的 top pool |
| 130 | `skyEnemyOverAll=true` 時的 sky pool |
| 140 | 爆炸及效果 |
| 150 | `background2over=2` 時的 MAP2 |
| 160 | HUD |

事件 21、22、28、29、42、43、73 已在
`src/opentyrian_level_port.c` 逐項保存
`background3_over`、`top_enemy_over`、`background2_over` 與
`sky_enemy_over_all`，v20 不另造關卡特例。

## GBA 映射

Mode 0 只有四級 BG/OBJ priority，無法替每個 PC stage 分配一級。
v20 改用「相對於兩個透明背景的位置」：

| PC 相對關係 | GBA OBJ priority | 結果 |
|---|---:|---|
| 物件在 MAP2、MAP3 後方 | 3 | MAP2/MAP3 都會蓋住物件；OBJ 與固定 MAP1 同級時仍在 MAP1 前 |
| 物件在兩個透明背景之間 | 2 | OBJ 與後景同級而在其前；priority 1 的前景仍蓋住 OBJ |
| 物件在兩個透明背景前方 | 0 | 位於兩個背景前 |

兩個透明背景中，PC stage 較早者使用 BG priority 2，較晚者使用
priority 1。`commit_vblank_work()` 每個 VBlank 依當下 PC flags 重寫
BG1/BG2 priority，因此事件中途改層也會立即生效。

GBA 同 priority 的 OBJ 由較低 OAM index 顯示在前，方向與 PC
ascending-slot blit 相反。`src/gba_scene.inc` 因此：

- 以 PC draw stage 的反向順序產生 OAM；
- pool 內也從高 slot 往低 slot 產生；
- ground2 pool 75..99 先於 ground pool 25..49 寫入 OAM；
- 玩家彈、敵彈、效果與 reward pool 同樣反向走訪。

這樣保留原始四個 enemy pool 的 identity、更新及碰撞語意，只在最後的
GBA presentation 階段翻譯遮擋關係。

## 紅框建物的顏色

舊版把 shape table 1 的 76 個 exact frame 全塞進同一組 15 色 palette。
紅框結構需要的棕色、金色與灰色被其他機體顏色擠掉，所以畫面偏灰紫，
與 PC Sprite2 差異明顯。

v20 對 event type 12 結構實際會使用的八個 exact frame 建立專屬 palette：

```text
(1,77,1)  (1,79,1)  (1,81,1)  (1,83,1)
(1,115,1) (1,117,1) (1,119,1) (1,121,1)
```

GBA OBJ palette bank 5 在 source-parity 第一關期間交給這八張圖；進入
position 5400 的簡化 Boss 時，再於 VBlank 恢復原本的 Boss bank 5。
兩者不會同時出現在同一 scene，因此沒有增加 palette bank 壓力。

`tools/build_assets.py` 會以所有不透明 PC source pixels 驗證色差。
8-bit RGB 每色頻 RMSE：

| Palette | RMSE |
|---|---:|
| 舊 table-1 共用 palette | 12.4863 |
| v20 結構專屬 palette | 6.7365 |

誤差降低 46.05%。建置若發現專屬 palette 沒有優於舊共用方案會直接失敗；
數值也保存於 `res/enemy_frame_audit.csv`。

## 畫面回歸

新增 `AUTOTEST_SCREENSHOT_POSITION`，可直接以 PC `curLoc` 擷取固定畫面，
不依賴 host frame 數。

手動檢查結果：

- `curLoc=240`：event time 216、source X=234 的 2×2 建物位於 MAP2/MAP3
  之後；白色雲層會正確遮住它。畫面左側在透明區域中的同類建物仍可見，
  且使用新的棕金色 palette。
- `curLoc=5000`、`5050`：Boss 前伸縮機關位於 sky stage 50。當時
  MAP2 stage 30、MAP3 stage 110，因此機關在 MAP2 前、MAP3 後，
  不再被壓到最底層，也不會錯蓋 MAP3。

## 自動驗證

`source_layer_priority_self_test()` 窮舉：

- 4 種 `background2_over`；
- 3 種 `background3_over`；
- 10 種代表性 OBJ stage；
- 每個 OBJ 對 MAP2、MAP3 的前後關係，以及兩個 BG 的相對順序。

總計 252 項關係全部通過。Telemetry schema 18 把檢查數、失敗數、最終
四個 PC layer flags 與兩個 GBA BG priority 寫入 SRAM。

| 項目 | v20 結果 |
|---|---:|
| ROM internal／host verifier | PASS／PASS |
| Layer priority checks／failures | 252／0 |
| 最終 `background2_over`／`background3_over` | 1／1 |
| 最終 `top_enemy_over`／`sky_enemy_over_all` | 0／0 |
| 最終 MAP2／MAP3 GBA priority | 2／1 |
| Logic updates／display frames | 7,093／12,239 |
| Missed VBlank | 54 |
| Peak OAM | 43 / 128 |
| Stream／effect／reward／projectile／cache drops | 0／0／0／0／0 |
| Exact frame catalog miss | 0 |

正式 ROM：

```text
build/tyrian_gba_level1_source_parity_layerorder_palette_romfs_v20.gba
10,885,064 bytes
SHA-256 274250bdd49f626cac89e7732f826357171a98e641ccce93e2a0000b3e487984
```

## 仍保留的界線

- position 5400 後仍是簡化 Boss，尚未完成 source-parity Boss lifecycle。
- GBA 仍以硬體 BG/OBJ priority 近似 PC 任意數量的 software blit stage；
  本關兩個透明背景的相對關係可以完整表示，但未宣稱能一般化到任意
  四層以上同時交錯的關卡。
- 這次只修正 presentation order 與 palette，沒有修改事件、enemy pool、
  座標、碰撞、傷害、獎賞或 RNG 順序。
