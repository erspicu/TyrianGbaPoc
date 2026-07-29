# Tyrian GBA v23：開發無敵旗標與離開關卡音樂切換

## 開發驗證旗標

`main.c` 定義：

```c
#ifndef TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
#define TYRIAN_GBA_DEV_PLAYER_INVINCIBLE 1
#endif
```

目前正式驗證 ROM 的值為 `1`（true）。玩家與敵機、敵彈的碰撞仍照常
判定並寫入 telemetry，但不再修改護盾、裝甲或觸發死亡，方便檢查完整
第一關流程。

若要測試已翻寫的受傷、爆炸與 Game Over 流程，可直接把值改為 `0`，
或在測試編譯加入：

```text
-DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0
```

旗標只作用於玩家承受傷害；敵人受傷、擊破、獎賞、Boss 與關卡事件都
不受影響。

## 選單音樂修正

關卡音樂與選單音樂都是循環播放，單純切換遊戲狀態不會讓 Maxmod 自動
換曲。v23 在離開 gameplay 的兩條路徑明確重新載入標題／選單模組：

- 玩家死亡進入 Game Over 時
- 正常破關由統計畫面回到 Game Menu 時

因此後續 Game Menu 不會再沿用第一關歌曲。

## 強制死亡回歸

另以關閉無敵旗標的 auto-test ROM 強制玩家在第一關死亡，等待 60 tick
爆炸流程完成，再於 Game Over 讀回執行狀態。SRAM 結果：

```text
magic=TGDM
state=10
selected_mus_song=29
maxmod_active=1
dev_invincible=0
```

`29` 是標題／選單來源曲；第一關來源曲為 `17`。這項測試同時確認死亡
流程仍可用，而且切換後音樂播放器保持運作。

## 完整回歸與成品

`build.ps1` 的 schema-20 完整第一關測試通過，最後狀態為 Game Menu，
`title_music_active=1`，68 個 ROMFS 檔案的 93 項自我檢查皆通過，mGBA
沒有回報 runtime error。

```text
ROM: build/tyrian_gba_level1_pc_flow_mode4_romfs_v23.gba
bytes: 11759248
SHA-256: b09ccff894ba2f59a5d6c5c34dc316d93e89a063f7d38b862f4ee4f5d60f0215
```
