# Tyrian GBA GAMELOOP 原始碼對照與平台邊界稽核

更新日期：2026-08-01

固定來源基準：OpenTyrian `1c34d1bddac8c8f2de834229d04b5a729525c944`

## 結論

關卡內 runtime 的責任現在分成兩層：

```text
OpenTyrian LVL/HDT/MUS/SHP/PIC + PC GAMELOOP 規則與座標
                              ↓
         GBA input / audio / BG / OBJ / 240×160 presentation adapter
```

敵人、事件、碰撞、武器、掉落、死亡、重生、Boss、背景 phase、破關及
關卡跳轉均以 OpenTyrian 規則和 ROMFS 原始資料為 authority。GBA 層只做
輸入映射、硬體呈現、音訊播放與效能排程，不用 GBA 畫面座標反推遊戲
規則。

本文件所稱「來源對齊」限定為單人 Full Game 與目前開放的單人 Arcade
關卡流程，不包含雙人、network、Galaga、Destruct、鍵盤 cheat／Street
Fighter 組合鍵等未納入產品規格的模式。

## 正式 GBA 平台規格

這些差異是刻意的平台設計，不列為移植缺漏：

| PC 行為／區域 | GBA 正式規格 |
|---|---|
| 320×200 邏輯 framebuffer | gameplay state 仍使用 PC 座標，不改成 240×160 座標 |
| 264×184 戰鬥 viewport | 1:1 取樣到 240×160；不縮放 gameplay state |
| 固定中央裁切 | 保留既有柔性鏡頭，只在原 viewport 可用餘量內平滑偏移；不改碰撞、事件或瞄準位置 |
| 右側 56 px OSD | 不移植其面板與版面 |
| 下方 16 px banner | 不移植其背景框與固定欄位 |
| OSD 中的重要即時值 | 依專案規格保留右下角 `SHIELD / ARMOR / GENERATOR` 精簡數值，以及金額 |
| 關卡提示、拾取提示、WARNING、timer | 保留來源計時與文字內容，改為透明 BG3 overlay，不重建下方 banner |
| Esc 關卡內選單 | 不移植；不讓其暫停或改寫 GAMELOOP |
| Pause | GBA `Start` 暫停／繼續，保留來源暫停狀態和提示 |
| PC software framebuffer filters | 以 GBA palette、BLD、BG／OBJ 等硬體機能做最接近的 adapter |
| 來源更新與顯示同一迴圈 | 邏輯進度獨立於 60 Hz scanout；dynamic frame drop 只略過逾時 presentation，不加速或漏跑 gameplay tick |

玩家可移動邊界另依 240×160 可視範圍收窄，避免機頭或尾翼被裁掉；這是
輸入邊界 adapter。敵人、子彈、背景、碰撞框與事件仍使用原 PC 座標。

## 每個 gameplay tick 的來源 phase 對照

OpenTyrian `tyrian2.c::JE_main()` 與
`mainint.c::JE_mainGamePlayerFunctions()/JE_playerMovement()` 的關鍵順序，
在 GBA 端對照如下：

| 順序 | OpenTyrian phase | GBA 對應 |
|---:|---|---|
| 1 | textErase、玩家 energy/shield、allPlayersGone、music fade | `gameplay_overlay_logic_tick()`、`source_player_energy_update()`、`source_update_event_music_fade()` |
| 2 | `JE_eventSystem()` | `ot_level_port_advance()` 中的完整 LVL event cursor、jump、return 與 event opcode |
| 3 | background 1/2/3、starfield、pre-player enemy pools | `source_apply_background_state()`、`advance_backgrounds()`、`ot_level_port_advance()` |
| 4 | 既存玩家子彈移動、動畫、碰撞 | `update_shots()` 與 packed active-mask collision phase |
| 5 | score item／equipment／enemy contact | `source_process_player_collisions()`、`ot_level_port_collide_player()` |
| 6 | `JE_playerMovement()`、武器、sidekick、special、死亡／重生／離場 | `update_player()` 與 `source_player_weapons_update()` |
| 7 | enemy-shot update（僅 `!endLevel`） | `source_update_enemy_projectiles()` |
| 8 | top/sky over-player enemy phases；離場期間仍更新 | `ot_level_port_advance_over_player_enemies()` |
| 9 | repeating、normal、random explosions | `source_update_repeating_explosions()`、`update_effects()`、`source_update_explosions()`、`source_update_random_explosions()` |
| 10 | low-armor warning、secret notice、timer、filter、Boss bar | `ot_level_port_update_low_armor_warning()` 及 GAMELOOP tail helpers |
| 11 | returnActive、stopped-background resume、end-level gate | `ot_level_port_update_return_active()`、`source_resume_group_stopped_backgrounds()`、`source_update_end_level_condition()` |

GBA render 端反向送出同一套 PC 軟體 blit 順序，利用「較低 OAM index 在
同 priority 重疊時勝出」重建 ground、sky、player、projectile、over-player
及 effect 的前後關係。邏輯 pool 的更新順序不因 OAM 數量而改變。

## 已直接承接的 GAMELOOP 規則

### 事件、敵人與背景

- LVL event record、enemy definition、map lookup 均直接由 ROMFS 原始檔讀取。
- event opcode 1..57、60..82 的目前來源分支已接入；event jump、timer jump、
  returnActive、forceEvents、link 254 Boss jump 均使用來源 cursor／條件。
- 四個 25-entry enemy pools、pre/over-player phase、layer flags、movement、
  acceleration、animation、turret 251..255、launch、death child 與 linked group
  使用來源欄位。
- 三層背景 phase、stop/resume、map speed／delay、parallax offset 與 starfield
  RNG 保留。GBA 只把最後畫面列送入 BG cache。

### 玩家、武器與碰撞

- 玩家慣性、速度、banking、source boundary、weapon energy、shield recharge、
  armor、invulnerability、死亡爆炸及 Arcade respawn 已接入。
- front、rear、sidekick、special 1..18、super bomb、Zinglon、Astral、flare、
  repeat/multi-position、guided/chain/iced/delayed shot 行為使用 HDT 定義。
- 玩家彈先於當 tick 新生成子彈更新，維持 PC phase；敵彈只在
  `!endLevel` 更新。
- enemy shot impact、玩家與敵人接觸、weapon-sized AABB、damage transition、
  linked kill、Boss link flash 與穿透剩餘傷害均在 source coordinate 執行。

### 掉落、獎賞與路線

- score item、直接 cash、data cube、weapon power、armor、super bomb、hot dog、
  orbiting asteroid、bonus portal／secret route 及 equipment pickup 已接入。
- Full Game 的 cash／power／equipment 規則與普通 Arcade 的回饋分支分開。
- Super Arcade `evalue 30001..30005` 的 ship-specific `SAWeapon[7][5]` 分支已
  按來源順序補入；持有同武器時的 cash/power 與 purple-ball progression
  不再落入一般 pickup 等價處理。
- pickup 造成的 event jump 會立即同步 `curLoc`，不會被 GBA 的舊
  `level_position` 在下一 tick 覆寫。

### 死亡、Boss 與破關

- 玩家死亡後背景和關卡物件繼續前進；GAME OVER cue 為 finite playback，
  自然停止後才接受退出流程。
- Boss 擊敗後的 end-flight、trail、敵人 over-player phase、勝利 cue、voice、
  statistics stage 與返回 Game Menu 使用來源狀態機。
- victory/death 音樂只播放一次，不使用一般關卡曲的 loop mode。
- stopBackgrounds 且畫面敵人清空時的恢復與 end-level gate 已放回 GAMELOOP
  tail，而非以每關 hard-code 位置離場。

## GBA 呈現 adapter 與效能界線

### Sprite2／爆炸快取

來源的 256 色 Sprite2 frame 在 build 時無損預解 RLE，但不預先套用關卡
palette；runtime L2 miss 才按當前 palette 上色。要顯示哪張圖仍由 LVL/HDT
和 GAMELOOP 決定，沒有每關專用 frame list。

Boss 爆炸場景實測可同時要求 28 種不同 16×16 effect frame。正式配置保留
原 Boss bar OBJ，並把 `GAME OVER / SECRET LEVEL / INSERT COIN` runtime
字形區的下半 8 tiles，在提示未顯示時分時作為兩個 effect slots。提示開始
時會先使 cache entry 失效，再於同一 VBlank 從 cartridge backing 還原對應
字形，因此沒有以少畫爆炸來換效能。

### Detail、filters 與超像素效果

- Low／Normal／High／Pentium detail flag 可保留；GBA 高階 profile 是來源
  規則加硬體近似，不代表逐像素複製 PC software framebuffer filter。
- `JE_doSP()` 的 persistent software superpixel framebuffer 粒子沒有建立
  101-entry PC framebuffer pool；現有爆炸、Sprite2、starfield、warning 與
  special 視覺走 GBA 硬體 adapter。這是已知的純呈現差異，不影響敵人、
  傷害、掉落或關卡流程。
- OAM、VRAM cache 或 frame-drop budget 只允許 presentation 退化，不可刪除
  邏輯 entity、碰撞、RNG 或事件更新。

## 明確不移植的關卡內功能

- PC 右側 OSD 面板與下方 banner 背景。
- Esc 叫出的關卡內設定選單及其 keyboard navigation。
- 雙人、network、Galaga、Destruct、鍵盤 cheat 與隱藏輸入組合。
- 只服務上述模式的 player-2 special／network synchronization 分支。

這些排除項不應用空函式混入 source gameplay core；GBA `Start` pause、透明
提示與精簡 HUD 是唯一對應 adapter。

## 2026-08-01 針對性驗證

組態：High Detail、Normal Game Speed、mGBA headless。

| 驗證 | 結果 |
|---|---:|
| Episode 1 Arcade 完整路徑／TGRS schema 3 | PASS |
| 實際 high-value pickup + Arcade equipment fixture | PASS |
| effect logic pool drop | 0 |
| effect VRAM cache drop | 0 |
| Sprite2／projectile／layer rule failure | 0 |
| effect 最大同畫面不同 frame | 28 |
| 最大 OAM | 109 / 128 |
| 全路徑 missed VBlank | 89 / 12,043（0.739%） |
| Boss 區間 missed VBlank | 4 / 245 |
| GAME OVER overlay、finite death cue、返回 Game Menu | PASS |
| Demo／INSERT COIN 五條資料流、返回首頁 | PASS |
| mGBA runtime error | 0 |

上述 0.739% 在專案已確認的「1% 內搭配 dynamic frame drop 可接受」範圍，
且 drop frame 不改變 gameplay tick 數或關卡節奏。

## 後續 GAMELOOP 維護規則

1. 遇到新關卡問題，先沿 `JE_main()` phase 與 LVL/HDT 欄位找缺漏，不建立
   episode/section 專用修補表。
2. 規則與資料修正放在 `level_port`／source runtime；GBA 座標裁切、VRAM、
   OAM、palette 和音訊放在 adapter。
3. 新呈現機制可以 drop presentation frame，但不得 drop GAMELOOP update。
4. 新功能若屬右 OSD、下 banner 或 Esc 選單，依本文件的正式排除規格處理，
   不再反覆列為缺漏。
5. 每次 cache 分時必須同時具備 reserve、失效、VBlank 還原與針對性測試，
   不可讓 OAM 指向被其他用途覆寫的 tile。
