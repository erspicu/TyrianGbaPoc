# Tyrian GBA Boss 身分 Manifest 通用規則

日期：2026-08-08

## 目的

Tyrian 的 Boss 並不是每隻各寫一套 C 程式。絕大多數 Boss 都由
`tyrianN.lvl` 的一般 enemy spawn／control events，加上 `tyrian.hdt` 的
enemy definitions，交給共用 game loop 組裝。Event 79 只在較晚時間指定
血條 link；它不是 Boss 出生事件。因此只靠目前血條或畫面座標分類，會在
Boss 登場、分離移動或血條切換階段失去組件身分。

本規則要求 Boss presentation 身分在 build 時從原始 LVL 資料導出，並在
runtime 的 spawn 時套用。不得建立 Episode、關卡或 Boss 名稱專用清單。

## Build-time 工具

`tools/build_boss_manifest.py` 逐一掃描 `vendor/tyrian/data/tyrian1.lvl` 到
`tyrian4.lvl` 的全部 62 個可玩 LVL：

1. 解析所有 11-byte event records。
2. 對每一次非零 Event 79 health-bar activation 個別追蹤；同一 link 在同一
   關重複用於兩次 Boss 戰也不會合併錯誤。
3. 向前解析 Event 39 link alias，反向找到該次血條之前最近的實際 spawn。
4. 以同一套全專案 construction-cohort 規則收集同批多 link 組件，並以
   同時發生的 group-control events 作 graph audit。
5. 輸出排序過的 `(episode, LVL number, event index) -> spawn mask`；Event
   12 的四次 spawn 以 bit mask 分別表示，其餘 spawn 使用 bit 0。

目前 stock corpus 的結果是 36 個含 Boss 血條的 LVL、726 個 spawn event、
0 個無法追溯的 Event 79 link，四個 Episode 全部有涵蓋。數字是輸入資料的
結果，不是 runtime 常數；替換合法資料後會在下一次 build 自動重算。

生成物位於 `res/boss_manifest.h` 與 `res/boss_manifest_audit.json`。`res/`
可重建且不提交 Git；真正應提交的是工具、Makefile 規則與本文件。

## Corpus-wide 驗證

工具每次執行都對全資料集檢查：

- 每個非零 Event 79 link 必須能解析到較早的 spawn anchor。
- 每次 activation 最近的相符 spawn 必須存在於 manifest。
- manifest key 必須唯一且只能指向真正的 spawn event。
- 沒有 Event 79 的 LVL 不得得到任何 manifest entry。
- Episode 1–4 都必須具有有效涵蓋；任何一章資料退化會直接使 build 失敗。

這些是全域不變條件。個別關卡只可作回歸樣本，不可成為分類條件。

## Runtime 規則

1. 進入關卡時，以實際選定的 Episode 與 LVL number 設定 manifest identity。
2. 每個 event spawn 在完成原始 PC 欄位初始化後，以 event key 作二分查找；
   命中才把該 enemy slot 標記為 Boss component。
3. 標記只改變 GBA presentation admission／OAM／Sprite2 cache 優先權，不得
   變更敵人 AI、傷害、碰撞、link、事件、RNG、掉落或關卡流程。
4. slot 釋放或重用時立即清除 manifest bit；另以 instance generation
   保存已確認的 Boss membership，避免新敵人繼承舊 slot 身分。
5. Event 79 仍是血條與傷害群組的權威資料，也是未命中資料的 runtime
   fallback；已組合的鄰接關係只可用來擴充辨識，不可用來撤銷既有身分。
6. 換關時完整 reset，不得讓 Episode／palette／cache／Boss 身分跨關污染。

## 效能與容量

目前 header 約由 726 個 32-bit key 與 726 個 8-bit mask 組成，ROM 成本約
3.6 KiB（未計編譯器對齊）。查找只在 enemy spawn 時進行，使用排序陣列
二分搜尋；不在每幀或每個 OAM candidate 上掃完整表。

每幀的 membership 查詢是 100-slot bitset O(1)。它讓 Boss 在血條顯示前就
取得 structural priority，也讓既有 compact Sprite2 cache 能在登場動畫開始
時啟動，而不是等到零件停止並靠座標重新拼回。

## 已驗證案例

- Episode 4、LVL 14：位置 104 時已標記 40 個 Boss components；第一個
  Event 79 要到位置 349 才發生。該 pre-intro 取樣的 enemy cache drops 為 0。
- 另一 Episode 的實際路線亦能依其選定 LVL 取得 manifest entries，證明
  runtime key 不是固定 Episode 4。
- build audit 掃描全部 62 個 LVL；沒有 Event 79 的關卡維持零 entries。

壓力測試結尾會執行 differential tests，部分測試會刻意清空
`source_parity_level`。Manifest telemetry 必須和其他 gameplay counters 一樣
在 self-test 前先快照，否則會得到假的全零結果。
