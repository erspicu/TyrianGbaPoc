# Tyrian GBA v44：靜態選單共用快速轉場

日期：2026-07-29
狀態：完成、High／Normal 全套回歸 PASS

## 目標

將 v43 `Next Level` 已驗證有效的原則擴充到其他偏靜態設定介面：

- 靜態內容在 build 階段由 PC 原始 PIC／SHP／文字資料產生。
- runtime 不再重做 PIC/SHP 解碼、縮放與整頁合成。
- 動態內容和文字維持獨立，僅更新實際改變的區域。
- 保持 Maxmod 更新與 VBlank 節奏，避免上下層選單切換時音樂鈍住。

本階段處理：

- `Game Menu <-> Upgrade Ship`
- `Title -> Play Mode -> Episode -> Difficulty`
- `Game Menu <-> Quit Game` 對話框
- 既有 `Next Level` 硬體 OBJ 路徑回歸

`Upgrade Ship` 內會隨裝備、金錢與游標改變的武器清單不是靜態頁，
因此沒有錯誤地整頁預烘焙；它沿用局部 dirty-row 更新。靜態 chrome、
船體與右側選單 panel 則已納入共用快速管線。

## 實作

### 1. Game Menu／Upgrade Ship 共用底圖

build 端產生 14 個 `120x120` 右側 panel：

- 6 個 Game Menu 選擇狀態
- 8 個 Upgrade Menu 選擇狀態

切換上下層時保留已合成的左側船體與 chrome，只從 ROM 線性複製右側
panel，再於 VBlank 做一次 page commit。runtime 不再重新解碼四次 SHP。

### 2. 遊戲前設定選單

Title、Play Mode、Episode、Difficulty 共 12 個畫面在 build 階段完成，
runtime 直接取得完整 Mode 4 page，不再於每次進入頁面重新縮放與畫字。

### 3. Quit 對話框

Quit 需要保留 Game Menu 當下的船體和游標，不能使用固定全畫面。因此採用：

1. Game Menu 完成時快取實際 scanline。
2. build 端從 stock `tyrian.shp` table 5 sprite 35 產生精確對話框。
3. 對話框以 `156x81` 對齊矩形保存，透明 byte 保留 live 背景。
4. 陰影只處理對話框外真正可見的 1,593 pixels。
5. Yes／No 顏色差異保存成 sparse runs。
6. 取消後直接還原快取，不重新建構整個 Game Menu。

保留 source renderer 作資產驗證失敗時的 fallback；正常路徑不會使用它。

### 4. 記憶體配置

Quit 背景快取放進前端／gameplay 共用 64 KiB arena 的閒置尾端，不增加
gameplay EWRAM 常駐量：

- 使用：64,768 bytes
- arena：65,536 bytes
- 餘量：768 bytes

較大的 sparse choice parser 留在 ROM；Quit overlay／shade 的熱迴圈留在
IWRAM。正式 ROM 的 IWRAM 餘量為 6,784 bytes，維持專案至少 6 KiB 的
安全門檻。

## build-time 資產

| 資產 | Bytes |
|---|---:|
| Native 5x7 font | 315 |
| Game／Upgrade panels | 201,600 |
| 前置選單完整 pages | 460,800 |
| Quit 對話框 | 12,652 |
| Quit Yes／No patches | 1,352 |
| Quit visible shade | 624 |
| **合計** | **677,343** |

這些都是原始 PIC／SHP／文字資料的通用 build-time 呈現，不是 per-level
規則或手工 GBA-only 關卡資料。

## 效能結果

### 靜態上下層轉場

High／Normal、mGBA headless、每組 120 次：

| 路徑 | 舊 missed VBlank | v44 missed VBlank | runtime SHP | runtime Sprite2 | 音樂 |
|---|---:|---:|---:|---:|---:|
| Game Menu／Upgrade Ship | 2,758 | **0** | 0 | 0 | active |
| Play Mode／Episode | — | **0** | 0 | 0 | active |
| Game Menu／Quit | 5,940 | **0** | 0 | 0 | active |

每組皆跨 121 個 VBlank 完成，沒有 pending frame 殘留。

Quit 單次階段最大 cycle：

- 背景 capture：0（進入前已準備）
- visible shade：26,210
- dense overlay：80,678
- Yes／No patch：20,010

即使將各階段獨立最大值相加仍為 126,898 cycles，低於一個
GBA 60 Hz frame 約 280,896 cycles。

### 選單內更新

- Game Menu 游標 600 次：`0 missed VBlank`
- `Next Level` 閒置動畫 600 frames：`0 missed VBlank`
- `Next Level` 選擇切換 120 次：`0 missed VBlank`
- 以上音樂皆維持 active，runtime SHP／Sprite2 decode 皆為 0。

### 畫面一致性

Quit state 14 與先前核准的 v42 capture 逐 pixel 比較：

- changed pixels：0
- maximum channel delta：0

## 完整回歸

`build.ps1 -DetailLevel high -GameSpeed normal`：

- 一般第一關完整流程：PASS
- 死亡與一次性音樂流程：PASS
- Jukebox：PASS
- Demo：PASS
- ROMFS 62 sections：62／62 PASS
- Episode 1 四關連續 campaign：4／4 PASS
- Episode 2／3／4 route：PASS
- Arcade route 與 equipment fixture：PASS
- runtime error：0
- Sprite2 raw catalog／ROMFS audit：PASS

## 正式 ROM

| 項目 | 值 |
|---|---|
| 設定 | Detail High／Game Speed Normal |
| Bytes | 22,449,896 |
| MiB | 21.410 |
| 32 MiB 使用率 | 66.9059% |
| SHA-256 | `9665889be60228fa8ecee8b5c94fd7195595560cc58024bac51a1de27afd481e` |

`build/` 已依 release-only policy 清理，只保留此正式 ROM；其他測試 ROM
已移至 `Backup/`。
