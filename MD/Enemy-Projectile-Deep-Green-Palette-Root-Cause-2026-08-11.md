# 敵方深綠 Sprite／子彈混入紅色：Root Cause

日期：2026-08-11
重現資料：`build/TyrianGBA.sav`，Episode 3、Section 11（FLEET）
使用者畫面：`ref/螢幕擷取畫面 2026-08-11 003049.png`

## 結論

問題是 GBA OBJ palette bank 的所有權衝突，不是 Tyrian 原始素材含紅色，
也不是 Sprite2 RLE／raw 解碼錯誤。

- 敵人與敵方子彈的 8bpp Sprite2 動態色盤使用 bank
  `1, 2, 3, 4, 5, 8, 13, 15`。
- 畫面中的綠色彈體對應 primary shot graphic 118。原始 raw frame 的
  主色碼為 `0xC0..0xCF`，經八階亮度映射後落在 OBJ palette bank 13。
- Boss 血條卻也被 build 資產指定為 `OBJ_PAL_BOSS_BAR = 13`。
- `commit_vblank_work()` 在每關初始化，以及 Boss 受擊使血條閃色時，
  直接覆寫 bank 13 的色槽 4、5、6。
- 這三格原本正是綠色彈體中央高亮所用的綠色階；被血條的紅／橘色階
  取代後，就形成截圖中垂直的紅色核心。

## 為何具有偶發性

關卡初始化一定會把血條色寫入一次；之後 detail palette 重新提交時可能
暫時把正確 Sprite2 色盤寫回，而 Boss 受擊／血條閃動又會再次覆寫。因此
同一素材可能一度正常、之後再變紅，看起來像 cache 隨機污染。

## 靜態證據

1. `src/source_runtime.inc` 的 `source_enemy_dynamic_palette_banks[]`
   明確包含 bank 13。
2. `res/asset_meta.h` 定義 `OBJ_PAL_BOSS_BAR 13u`。
3. `src/gba_platform.inc::commit_vblank_work()` 以
   `SPRITE_PALETTE + OBJ_PAL_BOSS_BAR * 16` 為基址，改寫 `[4..6]`。
4. `res/sprite2_raw_components.bin` 中 table 36、graphic 118 的原始像素
   沒有畫面所見的紅色帶；主要使用 `0xC0..0xCF` 綠色色階。

## 修復方向

把 Boss 血條搬到不屬於 Sprite2 動態 8bpp 色盤的保留 bank，並加入
build-time／compile-time 所有權檢查，禁止日後 HUD 或狀態提示再次配置到
Sprite2 動態 bank。修復後用 FLEET 相同 route 對照 frame 118 的硬體色盤
與畫面輸出。

修復與驗證結果見
[`Enemy-Projectile-Deep-Green-Palette-Fix-2026-08-11.md`](Enemy-Projectile-Deep-Green-Palette-Fix-2026-08-11.md)。
