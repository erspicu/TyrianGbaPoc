# Tyrian GBA 關卡內 GamePad Mapping 規則

日期：2026-08-05  
適用範圍：正常關卡 gameplay；不改靜態選單，也不改 HUD。

## 定案配置

| GBA 輸入 | 關卡內功能 |
|---|---|
| D-Pad | 保留 PC 座標、加速度與慣性的八方向移動 |
| A | 同時發射 Front／Rear 主武器 |
| B | 同時控制 Left／Right Sidekick |
| L | 啟動目前裝備的 Special Weapon |
| R | 消耗一顆已拾取的 Super Bomb |
| Select | 切換支援多模式的 Rear Weapon |
| Start | PAUSED／繼續 |

這是 GBA 輸入轉接層，不改 `tyrian.hdt` 的 weapon、port、option、special
定義，也不改 power use、ammo、charge、repeat、multi-position 或效果。

## 採用建議的部分

1. Front／Rear 共用 A。OpenTyrian 單人模式本來就用同一個 main-fire
   logical button 建立兩個 port 的射擊，GBA 不需要再拆鍵。
2. Left／Right Sidekick 合併到 B。有限彈藥、蓄力與各自 cooldown 仍分開
   計算，只合併玩家的實體輸入。
3. Special 移到 L，避免按主砲時意外消耗或啟動已裝備特殊武器。
4. Rear mode 移到 Select，解決舊版 L 同時切 Rear mode、又操控某些
   front-mounted Sidekick 的衝突。
5. Super Bomb 改為 R 專用。舊版照搬 PC 的「任一 Sidekick button」規則，
   會讓玩家只想開 Sidekick 時意外消耗炸彈；GBA 有空出的 R 可明確分工。

## 保留但不強制的一鍵全自動

`Configure.h` 的 `TYRIAN_GBA_GAMEPAD_FULL_AUTO_SIDEKICKS`：

- `0`（預設）：A 是主砲，B 是雙 Sidekick。OpenTyrian 原本就會跟隨
  main fire 的無限彈藥 Sidekick 仍照原規則連動。
- `1`：A 也會控制有限彈藥 Sidekick，成為真正的主／副武器一鍵全開；
  B 仍可單獨發射 Sidekick。

預設不用 Full Auto，是為了避免有限彈藥被無意耗盡，也讓 A/B 有清楚分工。

## 沒有採用的提議

- 不加入 Side-Dash：OpenTyrian 關卡規則沒有這項動作，不能由輸入介面
  自行發明新的移動能力。
- 不把 A+B 綁成炸彈：玩家自然會同時按主砲與 Sidekick，容易誤耗炸彈。
- 不把 L+R 當成泛用「最強技」：原版沒有一個可適用所有船型的共同技能。
- 不把所有 Twiddle code 當成同一招。L 啟動的是玩家目前真正裝備的
  Special Weapon；原始 `SFExecuted` 指令、支付 Shield／Armor 的路徑
  仍是另一套規則，不能混在一起。
- 不加入 Pause 子選單，也不修改 HUD；Start 僅維持既有暫停／繼續。

## Demo 相容性

`demo.1`～`demo.5` 記錄的是 PC logical buttons，而不是 GBA 實體按鍵。
播放 Demo 時仍分別解讀 Left／Right Sidekick、Rear mode、main fire，並保留
PC 的 Special／Super Bomb 觸發關係；不會因為實機 B 合併雙 Sidekick 而
多發另一側、改變彈藥或提前消耗 Super Bomb。

## 回歸門檻

AUTOTEST 必須驗證：

- A、B、L、R、Select 六種 physical action 不互相誤觸。
- B 同時送出左右 Sidekick logical command。
- Select 能切到 stock Rear port 的第二模式。
- L 能走既有 Special Weapon 完整效果路徑。
- Demo 的 bit 4～7 仍保留 main、rear-mode、left-sidekick、right-sidekick
  四種獨立語意。
