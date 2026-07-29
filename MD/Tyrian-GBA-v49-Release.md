# Tyrian GBA v49：獨立專案與完整靜態前端

日期：2026-07-29

## 版本定位

本版是 TyrianGbaPoc 確認技術路線可行後的第一個整合版本。原本的
source-parity 開發分支已合併回 `main`，後續以「依原版資料與
OpenTyrian 行為，盡可能完整移植 Tyrian 到 GBA」作為主要方向，
不再把專案定位為只展示第一關的拋棄式 POC。

專案仍在持續補齊較少見的遊戲路徑與內容，因此不是官方產品，也不
宣稱所有 Episode 的每一條分支都已完成；但建置、資料層、runtime、
選單與自動回歸已整理成可長期維護的獨立專案。

## 本版整合內容

### 專案與原始碼

- 將 `opentyrian-source-parity-port` 合併回 `main`。
- 把 Tyrian 原始資料、OpenTyrian 參考 source、GBA SDK、Maxmod、
  mGBA 測試 runtime、builder 與必要 shell 工具整理到專案內。
- 所有 build script 改用專案相對路徑；不依賴原工作區的固定磁碟
  位置。
- 新增 `Build-GBA-ROM.bat`，Windows 10/11 搭配 Python 3.10+
  即可一鍵安裝固定版 ARM compiler、重建資源並產生 ROM。
- 新增根目錄 `BUILDING.md` 與精簡的公開 `README.md`。
- 把原先過度集中的 runtime 依 frontend、level port、autotest、
  combat、background、platform 等責任分檔；目前最大的 source
  module 為 76,199 bytes，沒有超過 100 KiB 的 C／INC module。

### 可配置遊戲與畫面

- 新增中英文註解完整的根目錄 `Configure.h`。
- 可切換主角無敵驗證模式與原版資料驅動的極限武器壓力配置。
- 可調整關卡 HUD、金額、三列能源、PAUSED、Secret Level、
  Boss 血條、破關統計與所有已開放靜態選單的版面座標。
- 正式版預設使用正常傷害流程與一般劇情裝備；無敵與壓力配置皆
  關閉。
- 右下角補回武器能源、機體耐久與護盾／備用能源三項重要資訊，
  沿用原本金額的低 OAM 成本數字呈現。
- 首頁、Play Mode、Episode 與 Difficulty 改用支援正確大小寫的
  專用半粗體字型。

### 靜態選單效能

把已在 Next Level 驗證的混合渲染策略套用到所有已開放路徑：

1. Game Menu ↔ Upgrade Ship
2. Title ↔ Play Mode
3. Play Mode ↔ Select an Episode
4. Select an Episode ↔ Difficulty
5. Difficulty／統計／死亡 → Game Menu
6. Game Menu ↔ Next Level
7. Upgrade Ship ↔ 各裝備子選單
8. Game Menu ↔ Quit Game 對話框

底圖、原版 source stamps、統計字型與資料方塊在 build 階段準備，
runtime 只分幀組裝必要區塊；音樂持續更新，完成後才在 VBlank 原子
換頁。8 條路徑各往返 120 次，共 960 次轉場：

- missed VBlank：0；
- runtime SHP decode：0；
- runtime Sprite2 decode：0；
- transition failures：0；
- 音樂全程 active。

### 專案網站

新增 `Website/` 本機靜態網站，包含專案介紹、GitHub Release 下載
入口、真實 mGBA 截圖，以及 rendering、Sprite2 cache、timing、
frontend、ROMFS、verification 六篇獨立技術研究頁。網站為響應式
純 HTML/CSS/JavaScript；本版只提交來源，不建立正式網站部署。

## 最終回歸

環境：

- profile：High detail / Normal speed；
- compiler：Arm GNU Toolchain 15.2.1；
- emulator：mGBA 0.11.0；
- build source：專案內 `vendor/` 與 ROMFS；
- player invincible：0；
- stress loadout：0。

| 回歸項目 | 結果 |
|---|---:|
| Episode 1 第一關完整流程 | PASS |
| Episode 1 四關 campaign | 4/4 PASS |
| Episode 2／3／4 第一關 | PASS |
| Arcade 裝備與拾取流程 | PASS |
| 死亡、返回選單、一次性哀悼音樂 | PASS |
| Boss 統計與一次性勝利音樂 | PASS |
| Demo／JukeBox | PASS |
| ROMFS 關卡 section matrix | 62/62 PASS |
| ROMFS runtime files | 68 |
| 靜態選單轉場 | 960/960 PASS |
| runtime errors | 0 |
| Release boot benchmark | 600 frames PASS |

各 Episode 第一關的 missed VBlank 都只發生在 gameplay：

| 路線 | Gameplay | Frontend／Stats／Transition |
|---|---:|---:|
| Episode 1 | 8 | 0 |
| Episode 2 | 29 | 0 |
| Episode 3 | 13 | 0 |
| Episode 4 | 22 | 0 |

## ROM

| 項目 | 值 |
|---|---:|
| Release asset | `TyrianGbaPoc-v49-high-normal.gba` |
| GBA title | `TYRIAN GBA` |
| Game code | `TYGA` |
| Size | 29,277,108 bytes |
| Size | 27.9208 MiB |
| 32 MiB 使用率 | 87.2526% |
| EWRAM free | 30,720 bytes |
| IWRAM free | 6,368 bytes |
| SHA-256 | `e3ef315873db144bafd860e5516353ebcbe596aa0249795368725bc07275ecf4` |

ROM 不提交 Git。正式資產發布在：

<https://github.com/erspicu/TyrianGbaPoc/releases/tag/v49>

自行重建請在專案根目錄執行：

```text
Build-GBA-ROM.bat
```

完成後 `build/` 只保留最新的 `TyrianGBA.gba`；本機舊 ROM 會移至
忽略版本控制的 `Backup/`。
