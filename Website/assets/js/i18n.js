(() => {
  "use strict";

  const STORAGE_KEY = "tyrian-gba-site-language";
  const SOURCE_LANGUAGE = "zh";
  const DEFAULT_LANGUAGE = "en";

  /*
   * Text is authored once in Traditional Chinese in the HTML files.  The
   * English catalog below replaces individual DOM text nodes, preserving all
   * links, code spans and document structure.  Switching back restores the
   * exact source strings captured at startup.
   */
  const english = Object.freeze({
    "選單": "Menu",
    "主要導覽": "Primary navigation",
    "頁尾導覽": "Footer navigation",
    "麵包屑": "Breadcrumb",
    "本頁目錄": "Table of contents",
    "跳至主要內容": "Skip to main content",
    "首頁": "Home",
    "專案": "Project",
    "技術": "Engineering",
    "畫面": "Screenshots",
    "研究": "Research",
    "研究筆記": "Research notes",
    "技術研究": "Technical research",
    "下載": "Download",
    "全部研究": "All research",
    "實測結果": "Measured results",
    "壓力測試": "Stress test",
    "驗證": "Verification",
    "完整性": "Integrity",
    "視覺 QA": "Visual QA",
    "效能基線": "Performance baseline",
    "聲道校準": "Channel calibration",
    "鼓聲瞬態": "Percussion transients",
    "有限提示曲": "Finite cues",
    "無爆音轉場": "Click-free transitions",
    "來源與 soundbank": "Sources and soundbank",
    "SFX 與語音": "SFX and voices",
    "上一篇：Frontend": "Previous: Frontend",
    "上一篇：Rendering": "Previous: Rendering",
    "上一篇：ROMFS": "Previous: ROMFS",
    "上一篇：Sprite cache": "Previous: Sprite cache",
    "上一篇：Timing": "Previous: Timing",
    "上一篇：Palette training": "Previous: Palette training",
    "下一篇：Frontend": "Next: Frontend",
    "下一篇：ROMFS": "Next: ROMFS",
    "下一篇：Sprite2 cache": "Next: Sprite2 cache",
    "下一篇：Timing": "Next: Timing",
    "下一篇：Verification": "Next: Verification",
    "下一篇：Audio pipeline": "Next: Audio pipeline",
    "下一篇：多圖層與裁切": "Next: Layers and crop",

    /* Home */
    "TyrianGbaPoc — 把 Tyrian 帶進 GBA": "TyrianGbaPoc — Bringing Tyrian to the GBA",
    "TyrianGbaPoc：以原版資料與 OpenTyrian 行為為規格，盡可能忠實移植 Tyrian 到 Game Boy Advance。": "TyrianGbaPoc is a source-faithful port of Tyrian to the Game Boy Advance, guided by the original data and OpenTyrian behavior.",
    "TyrianGbaPoc 首頁": "TyrianGbaPoc home",
    "把 Tyrian 的": "Bring Tyrian's ",
    "滿天彈幕": "bullet-filled skies",
    "，帶進 16.78 MHz 的 GBA。": " to a 16.78 MHz GBA.",
    "TyrianGbaPoc 以原版 Tyrian 2.1 資料與 OpenTyrian 程式行為為規格， 在 GBA 的 CPU、記憶體、VRAM 與 OAM 限制內，重建多圖層關卡、 敵人、武器、音樂與完整遊戲流程。": "TyrianGbaPoc treats the original Tyrian 2.1 data and OpenTyrian behavior as its specification, rebuilding layered stages, enemies, weapons, music, and the complete game flow within the GBA's CPU, memory, VRAM, and OAM limits.",
    "取得最新 ROM": "Get the latest ROM",
    "閱讀移植研究": "Read the porting research",
    "GBA 上的 Tyrian 第一關 Boss 與最強武器壓力配置彈幕畫面": "Tyrian's first-stage boss on GBA under the maximum-weapon stress loadout",
    "硬體重點": "Hardware highlights",
    "從可行性測試，走向完整移植。": "From a feasibility test to a full port.",
    "專案起初只想回答一個問題：PC 版 Tyrian 能否在 GBA 上保留原本的節奏、 多層背景與高密度戰鬥？克服資源格式、繪圖順序、快取與時序問題後， 答案逐漸變得明確，因此目標也從單關技術展示，改為可維護、 盡可能完整且忠於原始程式的 GBA 移植版。": "The project began with one question: could the PC version of Tyrian retain its pace, layered backgrounds, and dense combat on GBA? After overcoming asset formats, draw order, caching, and timing, the answer became clear. The goal then grew from a one-stage technology demo into a maintainable, source-faithful, and as-complete-as-possible GBA port.",
    "這不是拿 Tyrian 美術做一款相似射擊遊戲。關卡事件、敵人定義、武器、 碰撞、linked destruction、獎賞、Boss 與破關流程，盡量沿著 OpenTyrian 的原始控制流翻寫；GBA 專屬修改集中在顯示、輸入、聲音、 儲存與效能 adapter。": "This is not a look-alike shooter built from Tyrian art. Level events, enemy definitions, weapons, collision, linked destruction, rewards, bosses, and stage completion follow OpenTyrian's original control flow wherever possible. GBA-specific changes stay concentrated in display, input, audio, storage, and performance adapters.",
    "原始 PC 戰鬥視窗不做整體縮放。264×184 gameplay space 維持原座標， GBA 以 240×160 柔性視窗利用兩軸各 24 pixels 的裁切餘量；敵人與碰撞 公式不必背負縮放成本，玩家靠近邊界時也能平順取回原版可見範圍。": "The original PC combat viewport is not scaled as a whole. Its 264×184 gameplay space keeps the original coordinates, while a 240×160 soft viewport uses the 24-pixel crop margin on each axis. Enemy and collision formulas pay no scaling cost, and the camera smoothly restores the source view near its edges.",
    "目標不是證明 GBA「毫無限制」，而是找出在真實硬體預算內， 忠實度可以被推到什麼位置。": "The goal is not to pretend that the GBA has no limits. It is to discover how far fidelity can be pushed within a real hardware budget.",
    "目前核心": "Current core",
    "目前實作摘要": "Current implementation summary",
    "資料來源": "Data source",
    "關卡事件": "Level events",
    "圖層": "Layers",
    "音訊": "Audio",
    "時序": "Timing",
    "不只是一張能動的畫面。": "More than a moving screenshot.",
    "目前已打通開頭、設定選單、Game Menu、Upgrade Ship、Next Level、 Demo、JukeBox、關卡、死亡與破關統計等主要流程；campaign 與少見分支 仍持續往完整版本推進。": "The intro, setup menus, Game Menu, Upgrade Ship, Next Level, Demo, JukeBox, gameplay, death, and end-of-level statistics are already connected. Campaign progression and rarer branches continue toward full coverage.",
    "原始資料直接驅動": "Driven directly by source data",
    "MUS、SHP、PIC、HDT、LVL 與 levelsN.dat 收進 ROMFS，runtime 仍以原始編號與規則決定內容。": "MUS, SHP, PIC, HDT, LVL, and levelsN.dat live in ROMFS; runtime content is still selected by the original IDs and rules.",
    "完整戰鬥要素": "Complete combat elements",
    "敵人移動、子彈、碰撞、damage transition、linked death、掉落、金錢、Boss 與關卡結束流程。": "Enemy motion, projectiles, collision, damage transitions, linked death, drops, money, bosses, and stage completion.",
    "多圖層與原版順序": "Layers in source draw order",
    "三個獨立捲動背景，搭配 GBA BG/OBJ priority 還原地面、中層、雲層與前景敵人的遮擋。": "Three independently scrolling backgrounds combine with GBA BG/OBJ priorities to restore terrain, middle layers, clouds, and foreground-enemy occlusion.",
    "128 OAM 與雙層快取": "128 OAM entries and two cache levels",
    "Sprite2 依 palette 即時映射，build-time 無損預解壓，再以 EWRAM L2 與 VRAM L1 承接高密度動畫。": "Sprite2 graphics are losslessly expanded at build time, mapped through the active palette at runtime, and carried by EWRAM L2 plus VRAM L1 caches.",
    "音樂、音效與語音": "Music, sound effects, and voices",
    "原版曲目轉入 tracker soundbank，關卡歌曲、爆炸與打擊音效、Boss／Data Cube 等語音依流程播放。": "Source music becomes a tracker soundbank; stage songs, explosions, hit effects, and Boss/Data Cube voices play from the original flow.",
    "可用而流暢的設定介面": "A usable, responsive frontend",
    "靜態底圖、文字與動態 OBJ 分離，轉場分幀組裝後原子換頁，選單切換不再拖慢音樂。": "Static backgrounds, text, and dynamic OBJs are separated. Transitions assemble over bounded frames and flip atomically, so menu changes no longer stall the music.",
    "限制沒有消失；它們被逐一量化。": "The limits did not vanish; each one was measured.",
    "GBA 無法用 PC 的方式直接畫完整場景。專案把每個瓶頸拆成可觀測的資料流、 cache miss、DMA、VBlank 與 cycle budget，再選擇對原始語意侵入最小的解法。": "The GBA cannot render a complete scene the way a PC does. Each bottleneck is reduced to observable data flow, cache misses, DMA, VBlank, and cycle budgets before choosing the solution that changes source semantics the least.",
    "Sprite2 解碼風暴": "Sprite2 decode storms",
    "不可變 RLE 與 tile 重排移到 build；runtime 只依當前 palette 上色。64-slot EWRAM L2 讓 Boss 多部件動畫暖身後只需 VRAM DMA。": "Immutable RLE expansion and tile rearrangement move to the build; runtime only applies the active palette. After warm-up, a 64-slot EWRAM L2 reduces multi-part boss animation to VRAM DMA.",
    "背景工作集超過 VRAM": "Background working sets larger than VRAM",
    "以 source row 為單位分區、預取與回收，保留 layer identity；沒有用「最近圖塊」猜測缺少素材，因此不會以錯圖掩蓋 cache miss。": "Source rows are partitioned, prefetched, and reclaimed while preserving layer identity. Missing data is never guessed from a nearest tile, so a cache miss cannot be hidden with the wrong art.",
    "高負荷場景超時": "Over-budget high-load scenes",
    "遊戲邏輯採固定時間步進；必要時只延後 presentation，不讓遊戲時鐘與音樂變慢。IWRAM、ARM hot path、active mask 與 cache telemetry 持續受測。": "Game logic uses a fixed timestep. When necessary only presentation is deferred, never the game clock or music. IWRAM placement, ARM hot paths, active masks, and cache telemetry remain under regression tests.",
    "靜態選單也會卡": "Even static menus can stall",
    "完整原始圖章在 build-time 預備，ship panel 用 19.2 KiB EWRAM cache，頁面工作拆成有界 phase；960 次壓力轉場為 0 missed VBlank。": "Complete source stamps are prepared at build time, the ship panel uses a 19.2 KiB EWRAM cache, and page work is split into bounded phases. A 960-transition stress run records zero missed VBlanks.",
    "資料不能每關手工補": "Data cannot be patched one stage at a time",
    "原版 ROMFS 是單一真實來源。通用 loader、section offset index 與完整 bank 轉換取代 per-level Python 對照表，才能把同一套方法延伸到後續章節。": "The source ROMFS is the single source of truth. General loaders, section-offset indexes, and complete-bank transforms replace per-level Python tables so the same method extends to later episodes.",
    "AI 加速探勘，人負責方向與驗收。": "AI accelerates exploration; people own direction and acceptance.",
    "本專案使用 Codex 協助長期原始碼對照、實作、測試與文件整理，也曾以 Gemini 3.1 Pro 諮詢選單渲染、GBA hot path 與 frame-drop 策略。 建議必須回到 OpenTyrian 原始碼、GBA 硬體規格與實測 telemetry 驗證； AI 不是新的遊戲規格來源，也不取代人工試玩與視覺 QA。": "Codex assists with long-running source comparison, implementation, testing, and documentation. Gemini 3.1 Pro was also consulted on menu rendering, GBA hot paths, and frame-drop strategy. Every suggestion must be verified against OpenTyrian, GBA hardware rules, and measured telemetry. AI is not a new game specification and does not replace hands-on play or visual QA.",
    "目前實機畫面。": "Current in-game captures.",
    "下列影像由本專案目前的 GBA ROM 在 mGBA 擷取，維持 240×160 原始像素； 網頁只做整數感的 pixelated 顯示，不重新繪製遊戲內容。": "These images were captured from the current GBA ROM in mGBA at the native 240×160 resolution. The site displays those pixels directly and does not redraw or crop the game content.",
    "目前版本第一章第一關的多圖層岩地與冰層場景": "Current Episode 1 stage-one layered rock-and-ice scene",
    "Episode 1 多圖層場景": "Episode 1 layered scene",
    "目前版本第四章 SURFACE 關卡的沙地與樹木場景": "Current Episode 4 SURFACE sand-and-tree scene",
    "目前版本第一關 Boss、最強主副武器與全畫面特效壓力場景": "Current first-stage boss with maximum primary, secondary, and sidekick fire",
    "Boss 壓力配置": "Boss stress loadout",
    "目前版本 GBA Tyrian 首頁選單": "Current GBA Tyrian title menu",
    "目前版本 Game Menu、玩家飛船與 Data Cube 資訊": "Current Game Menu with ship and Data Cube information",
    "目前版本 Next Level 星球與路線畫面": "Current Next Level planet and route screen",
    "目前版本 Upgrade Ship 武器與裝備選單": "Current Upgrade Ship weapons and equipment menu",
    "目前版本四顆 Data Cube 與資料閱讀畫面": "Current four-Data-Cube reader screen",
    "目前版本 Boss 擊敗後在持續捲動場景上的統計摘要": "Current post-boss statistics over the still-scrolling stage",
    "破關統計": "End-of-level statistics",
    "不只展示結果，也留下方法。": "Sharing the method, not only the result.",
    "技術研究區用獨立頁面整理最具通用性的取捨。內容聚焦 GBA 資料流與硬體 障礙，不要求讀者先理解本專案所有歷史文件。": "The research section gives each broadly useful tradeoff its own article. It focuses on GBA data flow and hardware constraints without requiring the project's full development history.",
    "4bpp 受限色盤訓練": "Constrained 4bpp palette training",
    "用真實關卡資料、感知色差與反例約束，讓 15 色 tile 保住原版觀感。": "Use real level data, perceptual color error, and counterexamples to preserve the source look in 15 visible colors.",
    "多圖層、柔性鏡頭與繪圖順序": "Layers, soft camera, and draw order",
    "把 264×184 PC gameplay space 轉成平滑的 240×160 視窗，並對應 GBA BG/OBJ priority。": "Turn the 264×184 PC gameplay space into a smooth 240×160 viewport and translate draw order into GBA BG/OBJ priorities.",
    "Sprite2 預解壓與二級快取": "Sprite2 pre-expansion and a two-level cache",
    "為什麼 build-time raw + EWRAM L2 是視覺忠實度與速度的交集。": "Why build-time raw data plus an EWRAM L2 is the intersection of visual fidelity and speed.",
    "固定時序與動態 frame drop": "Fixed timing and dynamic frame drop",
    "畫面來不及時延後 presentation，不改變遊戲進度節奏。": "Defer presentation when a frame is late without changing game progression.",
    "靜態選單的 GBA 渲染策略": "GBA rendering for static menus",
    "底圖、文字、動態 OBJ 與分幀轉場如何各自使用適合的硬體路徑。": "How backgrounds, text, dynamic OBJs, and staged transitions each use the right hardware path.",
    "音樂與音效的生成、校準及轉場": "Generating, calibrating, and transitioning music and SFX",
    "從 TYM、SND 到 Maxmod soundbank，保留聲道比例、抑制刺耳瞬態並避免切歌爆音。": "From TYM and SND to a Maxmod soundbank: preserve channel balance, control harsh transients, and avoid transition clicks.",
    "查看全部技術文章": "View all research articles",
    "在模擬器或真實硬體上試玩。": "Play it in an emulator or on real hardware.",
    "ROM 以 GitHub Release 資產提供，不存進原始碼 Git。專案仍在開發， 請先閱讀版本說明與已知限制；也可以直接從獨立建置環境自行產生 ROM。": "ROMs are published as GitHub Release assets and are not stored in source Git. The project is still in development; read the release notes and known limitations, or build the ROM yourself from the self-contained environment.",
    "前往下載與建置": "Go to downloads and build instructions",
    "瀏覽 GitHub 原始碼": "Browse the source on GitHub",
    "非官方、持續開發中的 GBA 移植專案。Tyrian 與原始素材權利屬各自權利人。": "An unofficial GBA port in active development. Tyrian and its source assets remain the property of their respective rights holders.",

    /* Download */
    "下載 TyrianGbaPoc GBA ROM，或使用專案內的獨立工具鏈自行建置。": "Download the TyrianGbaPoc GBA ROM or build it with the repository's self-contained toolchain.",
    "下載與建置 — TyrianGbaPoc": "Download and build — TyrianGbaPoc",
    "下載版本，或自己重建。": "Download a release or rebuild it yourself.",
    "可玩 ROM 只發布為 GitHub Release 資產，不提交到原始碼版本庫。 專案也包含固定版本的 SDK、工具與 source data 路徑，Windows 上可一鍵產生 ROM。": "Playable ROMs are published only as GitHub Release assets, never committed to the source repository. The project includes pinned SDK, tool, and source-data paths so Windows can produce a ROM in one step.",
    "選擇最新版本，下載附加的": "Choose the latest release, download the attached",
    "檔與閱讀該版本的變更、測試結果及已知限制。": "file, and read that version's changes, test results, and known limitations.",
    "開啟最新 Release": "Open the latest release",
    "開發中：": "In development:",
    "這是一個仍在逐步完整化的非官方移植版。不同 Episode、 裝備與少見路徑的成熟度可能不同；請以 Release notes 為準。": "This unofficial port is still being completed. Episodes, equipment combinations, and uncommon routes may differ in maturity; consult the release notes.",
    "如何遊玩": "How to play",
    "使用支援 Game Boy Advance 的模擬器或相容硬體載入 ROM。 開發回歸主要使用專案內固定版本的 mGBA；正式試玩也建議從 mGBA 開始。": "Load the ROM in a Game Boy Advance emulator or compatible hardware. Development regression uses the repository's pinned mGBA build, which is also the recommended starting point for play.",
    "移動／選擇": "Move / select",
    "射擊／確認／返回": "Fire / confirm / back",
    "確認／遊戲內暫停": "Confirm / pause in game",
    "Windows 一鍵建置": "One-step Windows build",
    "需求為 Windows 10/11 與 Python 3.10 或更新版本。Clone 專案後， 在根目錄執行：": "Requires Windows 10/11 and Python 3.10 or newer. After cloning the repository, run this from its root:",
    "複製": "Copy",
    "第一次執行會把固定版 ARM GNU Toolchain 安裝到專案內的": "The first run installs the pinned ARM GNU Toolchain inside the project's",
    "。不需要在系統層安裝 devkitPro。 完成後只保留：": ". No system-wide devkitPro installation is required. When complete, the build directory retains only:",
    "先前的本機 ROM 會搬到": "Previous local ROMs are moved to",
    "，建置中間物與 ROM 都由": "; intermediate build files and ROMs are excluded by",
    "排除。完整環境、路徑與回歸測試請閱讀根目錄": ". For the complete environment, paths, and regression workflow, read the root",
    "可調整設定": "Configurable settings",
    "根目錄": "The root",
    "集中管理開發用無敵模式、極限武器負荷、 HUD 與選單座標。Detail Level 可選 Low、Normal、High、Pentium； Game Speed 提供 Low、Normal。Release 預設值以每版說明為準。": "centralizes development invincibility, maximum-weapon stress mode, HUD positions, and menu coordinates. Detail Level supports Low, Normal, High, and Pentium; Game Speed supports Low and Normal. Release defaults are documented per version.",
    "資料與權利": "Data and rights",
    "本專案非 Tyrian 官方產品，也不代表原作者或任何平台持有人。 Tyrian 名稱與原始素材權利屬各自權利人；OpenTyrian、GBA SDK、 Maxmod 與 mGBA 的來源和授權列於專案": "This is not an official Tyrian product and does not represent its authors or any platform holder. The Tyrian name and source assets belong to their respective rights holders. Sources and licenses for OpenTyrian, the GBA SDK, Maxmod, and mGBA are listed in",
    "。 請依所在地法規與各項授權使用資料、ROM、模擬器與硬體。": ". Use data, ROMs, emulators, and hardware according to local law and each applicable license.",
    "TyrianGbaPoc contributors. 非官方 GBA 移植專案。": "TyrianGbaPoc contributors. Unofficial GBA port.",
    "一鍵建置": "One-step build",

    /* Research landing */
    "TyrianGbaPoc 技術研究：GBA 受限色盤訓練、多圖層、Sprite2 快取、ROMFS、固定時序與靜態選單最佳化。": "TyrianGbaPoc technical research: constrained GBA palettes, layers, Sprite2 caches, ROMFS, fixed timing, audio, and static-menu optimization.",
    "技術研究 — TyrianGbaPoc": "Technical research — TyrianGbaPoc",
    "在 GBA 限制內，保留原版語意。": "Preserving source semantics within GBA limits.",
    "這裡整理移植中最有通用價值的設計。它不是效能魔法清單，而是說明每個 瓶頸如何被量測、資料不變性的邊界如何切分，以及何時寧可保留限制， 也不以錯誤的畫面或規則換取表面流暢。": "These notes collect the port's most reusable designs. They are not a list of performance tricks; they explain how each bottleneck was measured, where immutable and mutable data were separated, and when a real limitation is preferable to apparent smoothness built on incorrect graphics or rules.",
    "PC 264×184 gameplay space 如何成為 240×160 的 1:1 平滑視窗，以及 layer priority 的翻譯方式。": "How a 264×184 PC gameplay space becomes a smooth, 1:1 240×160 viewport, including the translation of layer priorities.",
    "Sprite2 預解壓與 EWRAM L2": "Sprite2 pre-expansion and EWRAM L2",
    "將不可變 RLE 移到 build，保留 runtime palette/filter，解決 Boss 多部件動畫風暴。": "Move immutable RLE work to the build while retaining runtime palette/filter behavior, eliminating multi-part boss decode storms.",
    "固定時序、VBlank 與 frame drop": "Fixed timing, VBlank, and frame drop",
    "ARM7TDMI 趕不上場景時，如何延後畫面而不拖慢遊戲與音樂時鐘。": "How to defer presentation when ARM7TDMI cannot finish a scene without slowing the game or music clocks.",
    "靜態選單的混合渲染管線": "Hybrid rendering for static menus",
    "build-time pages、source stamps、局部文字、硬體 OBJ 與分幀原子換頁。": "Build-time pages, source stamps, localized text, hardware OBJs, and staged atomic page flips.",
    "ROMFS 與可擴充資料層": "ROMFS and an extensible data layer",
    "不為每關建立專用轉換表，仍能讓 C 移植碼低成本讀取 MUS/SHP/PIC/HDT/LVL。": "Let ported C code read MUS/SHP/PIC/HDT/LVL cheaply without per-level conversion tables.",
    "Telemetry 與自動回歸": "Telemetry and automated regression",
    "用 mGBA SRAM 契約追蹤 route、cache、OAM、VBlank、一次性音樂與 source parity。": "Use an mGBA SRAM contract to track routes, caches, OAM, VBlank, finite music, and source parity.",
    "從真實 LVL 與 tile 曝光量訓練 mixed palette，讓 15 個可見色保住原版色相與物件關係。": "Train mixed palettes from real LVL data and tile exposure so 15 visible colors preserve source hues and object relationships.",
    "音樂與音效的生成及校準": "Generating and calibrating music and sound effects",
    "把 TYM／SND 轉為 Maxmod soundbank，校準聲道與鼓聲，並讓有限提示曲及切歌行為符合來源流程。": "Convert TYM/SND data into a Maxmod soundbank, calibrate channels and percussion, and preserve finite-cue and transition semantics.",
    "深入的歷史調查、逐版本數據與 parity 報告仍保存在原始碼庫": "Detailed historical investigations, per-version data, and parity reports remain in the repository's",
    "。 網站文章是面向一般技術讀者的整理版。": ". The website presents them for a general technical audience.",
    "TyrianGbaPoc contributors. 技術內容以目前 main 實作為準。": "TyrianGbaPoc contributors. Technical content follows the current main implementation.",

    /* Shared research footer */
    "TyrianGbaPoc 技術研究 ·": "TyrianGbaPoc technical research ·",

    /* Frontend */
    "Tyrian GBA 靜態選單的底圖、文字、硬體 OBJ、EWRAM cache 與分幀轉場策略。": "Background, text, hardware OBJ, EWRAM cache, and staged-transition strategies for Tyrian's GBA frontend.",
    "靜態選單混合渲染 — TyrianGbaPoc 技術研究": "Hybrid rendering for static menus — TyrianGbaPoc research",
    "每一種內容，交給適合的硬體路徑。": "Give each kind of content the right hardware path.",
    "設定選單看似比關卡簡單，但整頁縮放、SHP 解碼與文字重畫會讓音樂明顯鈍住。解法不是只換一個 video mode，而是分離靜態與動態工作。": "Configuration menus look simpler than gameplay, yet full-page scaling, SHP decoding, and text redraws can audibly stall the music. The solution is not merely another video mode; it is separating static and dynamic work.",
    "為什麼按一下就卡": "Why one button press caused a stall",
    "原始瓶頸": "Original bottleneck",
    "早期 Game Menu 會在每次進入、游標移動或上下層切換時，重新從 stock PIC/SHP 解碼、組合 320×200 source page、縮放到 240×160，再把文字畫進 bitmap。CPU 在同一輪做完所有工作，Maxmod 沒有足夠時間更新。": "The early Game Menu decoded stock PIC/SHP data, composed a 320×200 source page, scaled it to 240×160, and redrew text whenever the menu opened, the cursor moved, or a submenu changed. Doing all of that in one pass left Maxmod too little time to update.",
    "三種內容，三種策略": "Three kinds of content, three strategies",
    "內容分流": "Content routing",
    "完全靜態": "Fully static",
    "Chrome、設定頁與固定 panel 在 build-time 從原版資料準備為 Mode 4 page／rows，runtime 線性複製。": "Chrome, setup pages, and fixed panels are prepared from source data as Mode 4 pages/rows at build time and copied linearly at runtime.",
    "狀態依賴": "State-dependent",
    "船體、裝備、金錢依 HDT 與 player state 組合；完成後放進 19.2 KiB EWRAM ship-panel cache。": "Ship, equipment, and cash are composed from HDT plus player state, then retained in a 19.2 KiB EWRAM ship-panel cache.",
    "持續動態": "Continuously dynamic",
    "Next Level 旋轉星球用硬體 OBJ；文字在最終 240×160 座標獨立畫，避免跟著背景縮糊。": "The rotating Next Level planet uses hardware OBJs; text is drawn independently in final 240×160 coordinates so it is never blurred with the background.",
    "通用 source stamp catalog": "General source-stamp catalog",
    "Upgrade Ship 的裝備圖不能把每個組合都烘成完整頁面。build 端因此產生 14,925 個 stock source stamps，涵蓋必要 tyrian.shp、Sprite2 banks 與 4/5 scale 的 25 個 sub-pixel phase。runtime 仍由原版資料 ID 決定圖形， 只是不再做 RLE 與逐像素 resize。": "Upgrade Ship cannot bake every equipment combination into a complete page. The build therefore creates 14,925 stock source stamps covering required tyrian.shp art, Sprite2 banks, and 25 sub-pixel phases at 4/5 scale. Source data IDs still choose the graphics at runtime, but RLE and per-pixel resize work are gone.",
    "轉場不是一次 function call": "A transition is not one function call",
    "分幀轉場": "Staged transitions",
    "新頁面在 scratch page 分階段準備：每次最多 80 rows page copy、60 rows panel、單一 Next Level 選項或單一 Upgrade row。舊畫面保持可見，": "A new page is assembled in a scratch page over bounded phases: at most 80 rows of page copy, 60 panel rows, one Next Level option, or one Upgrade row per phase. The old page remains visible and",
    "每個 VBlank 都會執行；只有最後才排程一次 Mode 4 page flip。": "runs every VBlank; only the final phase schedules one Mode 4 page flip.",
    "Quit Game 對話框再細分成背景 capture、shade、overlay 與 choices。 取消時直接還原 cache，不重新建構 Game Menu。": "The Quit Game dialog is further split into background capture, shade, overlay, and choices. Cancel restores the cache directly instead of rebuilding the Game Menu.",
    "960 次壓力轉場": "960 transition stress passes",
    "八條已開放雙向路徑各做 120 次，共 960 次。High／Normal 結果為 0 missed VBlank、0 runtime SHP、0 runtime Sprite2、music active。 最大單 phase 118,465 cycles，低於 180,000-cycle 回歸門檻。": "Eight enabled bidirectional paths run 120 times each, for 960 transitions. High/Normal records zero missed VBlanks, zero runtime SHP decodes, zero runtime Sprite2 decodes, and continuous music. The largest phase costs 118,465 cycles, below the 180,000-cycle regression limit.",
    "以快取與靜態 panel 組成的目前版本 Game Menu": "Current Game Menu assembled from cached and static panels",

    /* Sprite cache */
    "Tyrian Sprite2 build-time 無損預解壓、runtime palette mapping、EWRAM L2 與 VRAM L1 快取。": "Lossless build-time Sprite2 expansion, runtime palette mapping, EWRAM L2, and VRAM L1 caching for Tyrian.",
    "Sprite2 預解壓與二級快取 — TyrianGbaPoc 技術研究": "Sprite2 pre-expansion and two-level caching — TyrianGbaPoc research",
    "把不會變的解碼，留在 build。": "Leave immutable decoding in the build.",
    "Boss 不是一張 sprite，而是多部件、動畫與 palette 的 miss storm。解法必須保留原版視覺，同時把首次與重複成本切開。": "A boss is not one sprite; it is a miss storm of components, animation, and palettes. The solution must preserve source visuals while separating first-use cost from repeated cost.",
    "原始 runtime 路徑": "Original runtime path",
    "Tyrian Sprite2 frame 由多個壓縮 component 組成。早期版本在 cache miss 時 查檔、清除 buffer、做 RLE 解碼、組成 32×32 frame，再逐 pixel 依 GBA palette pack。Boss 同時出現多部件與動畫幀時，一個 tick 可能連續觸發 五次以上 miss，足以跨過單幀預算。": "A Tyrian Sprite2 frame is assembled from several compressed components. On a cache miss the early implementation looked up the file, cleared a buffer, decoded RLE, composed a 32×32 frame, and packed every pixel through the GBA palette. A multi-part animated boss could trigger five or more misses in one tick, enough to exceed a frame budget.",
    "依資料不變性切分": "Split at the boundary of data mutability",
    "切分邊界": "Split boundary",
    "RLE 解壓與 component layout 對同一份 stock shape bank 永遠不變； palette、iced/filter 等映射則會隨關卡與事件變化。因此 build 階段只產生 原始 8-bit index，不烘焙 GBA palette：": "RLE expansion and component layout never change for a stock shape bank, while palette and iced/filter mappings vary by level and event. The build therefore emits only original 8-bit indices, without baking a GBA palette:",
    "build：完整 bank RLE 無損解壓，保留原始 pixel index。": "Build: losslessly expand every bank and retain source pixel indices.",
    "L2 miss：ROM raw → 當前 palette/filter 查表 → EWRAM。": "L2 miss: ROM raw → active palette/filter lookup → EWRAM.",
    "L2 hit、VRAM miss：1 KiB DMA 上傳。": "L2 hit, VRAM miss: upload 1 KiB by DMA.",
    "L1 hit：直接沿用已在 VRAM 的 OBJ tiles。": "L1 hit: reuse OBJ tiles already in VRAM.",
    "這不是 per-level 圖形表。gameplay 仍從 HDT/LVL/egr 決定": "This is not a per-level graphics table. HDT/LVL/egr still decide",
    "哪一張": "which",
    "graphic；raw catalog 只提供完整且無損的通用表示。": "graphic gameplay needs; the raw catalog only provides a complete, lossless general representation.",
    "Key、palette 與失效": "Keys, palettes, and invalidation",
    "L2 key 為": "The L2 key is",
    "。 L2 保存已上色 tile，因此關卡重新設定 enemy palette 時必須 flush； 若只清 VRAM L1，舊關卡色彩仍可能從 EWRAM 被重新上傳。": ". L2 stores colorized tiles, so resetting the enemy palette for a level must flush the cache. Clearing only VRAM L1 could upload old-level colors from EWRAM again.",
    "這個失效規則很小，卻是 cache 正確性的核心：速度來自重用， 視覺 parity 則來自知道何時不能重用。": "This invalidation rule is small but central to cache correctness: speed comes from reuse, while visual parity comes from knowing when reuse is no longer valid.",
    "Boss 段落結果": "Boss-section results",
    "Episode 1 第一關完整 trace 中，Boss performance window 的 Sprite2 L2 為 100 hits、21 misses、0 fallback；L1/VRAM 仍因動畫 frame 產生 121 uploads，但不再重做 RLE。正式回歸同時要求：": "In a complete Episode 1 stage-one trace, the boss performance window records 100 Sprite2 L2 hits, 21 misses, and zero fallbacks. Animation still causes 121 L1/VRAM uploads, but RLE is never repeated. Release regression also requires:",
    "raw catalog CRC 與 bytes 正確；": "raw-catalog CRC and byte count match;",
    "L2 raw build 次數必須等於 L2 misses；": "L2 raw builds equal L2 misses;",
    "RLE fallback、cache drop、projectile drop 必須為 0；": "RLE fallbacks, cache drops, and projectile drops remain zero;",
    "Boss missed VBlank 維持在明確預算內。": "boss missed VBlanks remain inside an explicit budget.",
    "目前版本多部件 Boss 與最強武器壓力配置彈幕": "Current multi-part boss under the maximum-weapon stress loadout",
    "多部件 Boss 動畫": "Multi-part boss animation",

    /* Timing */
    "GBA 固定時間步進、VBlank recovery、presentation defer 與動態 frame drop 的設計。": "Design of the GBA fixed timestep, VBlank recovery, presentation defer, and dynamic frame drop.",
    "固定時序與 Frame Drop — TyrianGbaPoc 技術研究": "Fixed timing and frame drop — TyrianGbaPoc research",
    "可以少畫一幀，不能讓世界變慢。": "A frame may be skipped; the world may not slow down.",
    "極端彈幕、爆炸與背景 streaming 同時出現時，GBA 可能無法每個 VBlank 都準備新畫面。遊戲時鐘、音樂與輸入仍必須按真實時間前進。": "When extreme projectile density, explosions, and background streaming coincide, the GBA may not prepare a new image for every VBlank. Game time, music, and input must still advance in real time.",
    "一個 frame 的預算": "One frame's budget",
    "GBA CPU 約 16.78 MHz，螢幕接近 59.73 Hz，一個 frame 約有 280,896 CPU cycles。這個數字包含遊戲邏輯、背景準備、Sprite cache、render、 audio update、input 與 VBlank commit，不只是「畫 sprite」。": "The GBA CPU runs near 16.78 MHz and the display near 59.73 Hz, leaving about 280,896 CPU cycles per frame. That budget includes game logic, background preparation, sprite caching, rendering, audio updates, input, and the VBlank commit—not merely drawing sprites.",
    "邏輯累加器以 wall-clock VBlank 推進。Normal speed 保留來源節奏； 如果上一輪工作跨過 VBlank，主迴圈會消耗已發生的 IRQ，執行必要的 catch-up logic，而不是再次睡到下一個 VBlank。這避免「一次超時， 額外再停一幀」的連鎖遲滯。": "The logic accumulator advances from wall-clock VBlanks. Normal speed preserves source pacing. If work crosses a VBlank, the main loop consumes the IRQs that already occurred and runs required catch-up logic instead of sleeping through another VBlank. This prevents one overrun from cascading into an extra frozen frame.",
    "場景趕不及時，當前完整畫面可以再呈現一次；新 OAM、tile row 與 page flip 等到真正的 VBlank 再 commit。邏輯位置、敵人生命、武器 cooldown、 Maxmod 更新與背景 source position 不因重複畫面而停住。": "When a scene is late, the current complete frame can be presented again; new OAM, tile rows, and page flips wait for a real VBlank commit. Logic positions, enemy health, weapon cooldowns, Maxmod updates, and background source positions keep advancing.",
    "Frame drop 不是把所有邏輯改成 30 Hz，也不是凍結背景來掩蓋成本。 它只改變「何時交付下一個完整 presentation」。": "Frame drop does not move all logic to 30 Hz or freeze backgrounds to hide cost. It changes only when the next complete presentation is delivered.",
    "先優化，再允許 drop": "Optimize first, then permit a drop",
    "Drop frame 是安全網，不是第一個答案。專案仍針對實測 hot path 使用：": "Frame dropping is a safety net, not the first answer. Measured hot paths still use:",
    "Game Pak prefetch 與 3/1 waitstate；": "Game Pak prefetch and 3/1 waitstates;",
    "IWRAM／ARM code 放置；": "IWRAM/ARM code placement;",
    "active bitmask 跳過無效碰撞候選；": "active bitmasks that skip inactive collision candidates;",
    "Sprite2 raw catalog、EWRAM L2 與 VRAM upload queue；": "a Sprite2 raw catalog, EWRAM L2, and VRAM upload queue;",
    "背景 row prefetch 與 bounded cache work；": "background-row prefetch and bounded cache work;",
    "移除 hot loop 的軟體除法與不必要的 memory write。": "removal of software division and needless memory writes from hot loops.",
    "Branchless 並不自動更快。ARM7TDMI 沒有現代 branch predictor， 但額外乘法、全部路徑的 memory access 也可能比簡單短路分支昂貴； 每個改動都以 cycle counter 與完整 route golden 驗證。": "Branchless code is not automatically faster. ARM7TDMI lacks a modern branch predictor, but extra multiplies and memory accesses on every path can still cost more than a simple short-circuit branch. Every change is verified with cycle counters and complete route goldens.",
    "把漏幀分到真正狀態": "Attribute missed frames to their real state",
    "總 missed VBlank 會分成 gameplay、stats、game-over、frontend transition 與其他 frontend。這能分辨「Boss 計算真的重」和「選單誤做 runtime SHP 解碼」。目前 High／Normal 的 Episode 1–4 路線， frontend／stats／transition deterministic misses 全部為 0。": "Total missed VBlanks are split among gameplay, stats, game over, frontend transitions, and other frontend work. This distinguishes a genuinely expensive boss from an accidental runtime SHP decode in a menu. Current High/Normal Episode 1–4 routes record zero deterministic frontend, stats, or transition misses.",

    /* ROMFS */
    "TyrianGbaPoc ROMFS、原始 MUS/SHP/PIC/HDT/LVL loader 與可擴充資料層設計。": "TyrianGbaPoc ROMFS, source MUS/SHP/PIC/HDT/LVL loaders, and extensible data-layer design.",
    "ROMFS 與可擴充資料層 — TyrianGbaPoc 技術研究": "ROMFS and an extensible data layer — TyrianGbaPoc research",
    "一套 loader，不能只服務第一關。": "One loader cannot serve only the first stage.",
    "若每遇到錯圖就建立一份 GBA-only 對照表，Episode 越多，維護成本越接近無限。資料層必須把原始檔當作真實來源。": "If every wrong graphic creates another GBA-only lookup table, maintenance approaches infinity as episodes grow. The data layer must treat source files as the truth.",
    "為 C 移植碼提供熟悉的 I/O": "Familiar I/O for ported C code",
    "I/O 目標": "I/O goal",
    "GBA ROM 是 memory-mapped，但 OpenTyrian 原始碼習慣以檔名、offset、 record 與 loader 讀資料。專案的 ROMFS 在 build 階段把 stock data 連同 path metadata 打包；runtime": "GBA ROM is memory-mapped, but OpenTyrian code reads data through filenames, offsets, records, and loaders. At build time the project ROMFS packs stock data with path metadata; at runtime",
    "取得直接指向 ROM 的 span，避免把整個檔案搬到 RAM。": "returns a span directly into ROM, avoiding whole-file copies into RAM.",
    "原始格式各自解讀": "Interpret each source format",
    "曲目 offset、LDS pattern 與 patch 資訊。": "Song offsets, LDS patterns, and patch data.",
    "壓縮圖形 bank 與 frame/component layout。": "Compressed graphics banks and frame/component layouts.",
    "前端背景、palette 與 source presentation。": "Frontend backgrounds, palettes, and source presentation.",
    "武器、port、ship、option、shield 與 enemy 定義。": "Weapon, port, ship, option, shield, and enemy definitions.",
    "事件、enemy pool、map shapes 與三層 map。": "Events, enemy pools, map shapes, and three map layers.",
    "加密 episode script、跳轉、難度、路線與選項。": "Encrypted episode scripts, jumps, difficulty, routes, and choices.",
    "邏輯先問 loader「原版資料是什麼」，再由 GBA adapter 決定如何顯示。 這也是修正 Episode 2／4 錯素材時的重要界線：問題應在通用 bank/index 規則解決，不在關卡中加入一個特殊 if。": "Logic first asks the loader what the source data says; the GBA adapter then decides how to present it. This boundary also matters when fixing Episode 2/4 art: solve the general bank/index rule, never add a special case inside one stage.",
    "允許的 build-time 轉換": "Permitted build-time transforms",
    "Build 轉換": "Build transforms",
    "「讀原始資料」不等於所有工作都必須在 16.78 MHz runtime 做。 可接受的轉換以資料不變性與完整覆蓋為原則：": "\"Read source data\" does not mean all work must happen at a 16.78 MHz runtime. Acceptable transforms follow immutability and complete-coverage rules:",
    "完整 Sprite2 bank 的無損 RLE 展開，而非只列第一關看過的 frames；": "losslessly expand complete Sprite2 banks, not just frames seen in stage one;",
    "前端完整 source stamp 集合，而非每個選單狀態手繪替代圖；": "build a complete frontend source-stamp set, not hand-drawn substitutes per menu state;",
    "字型由 stock glyph 無損轉為 GBA tile layout；": "losslessly rearrange stock glyphs into GBA tile layout;",
    "音樂轉成 Maxmod soundbank，但曲目與流程仍由原始 song ID 驅動。": "convert music into a Maxmod soundbank while source song IDs still drive selection and flow.",
    "Build-time 資產是原始資料的通用表示；關卡事件仍決定「用什麼」， 不會被預轉換工具接管 gameplay 規格。": "Build-time assets are general representations of source data. Level events still decide what to use; a conversion tool never takes over gameplay rules.",
    "加密腳本的 O(1) section seek": "O(1) section seek in encrypted scripts",
    "是 encrypted Pascal-string stream。舊 seek 每次從開頭逐行解密到第 N 個": "is an encrypted Pascal-string stream. The old seek decrypted every line from the beginning until the Nth",
    "。新版直接掃 record length， 只解 encrypted first character 來辨識 section marker，保存 ROM offset。": ". The new path scans record lengths, decrypts only the first character to recognize section markers, and stores ROM offsets.",
    "四個 stock scripts 實際有 24–51 sections；快取約 272 bytes， 不需要 generated route catalog。跳轉、Full Game／Arcade 與 difficulty 判定仍由原 parser 執行。": "The four stock scripts contain 24–51 sections each. The index needs about 272 bytes and no generated route catalog. Jumps, Full Game/Arcade, and difficulty rules still use the original parser.",
    "完整性契約": "Integrity contract",
    "ROMFS build 產生 entry count、metadata CRC、payload CRC、manifest CRC 與 image SHA-256。ROM 啟動時做 probe/self-test；完整 matrix 再走過四個 Episode 共 62 個 playable LVL sections，確保資料架構不是只對示範關卡成立。": "The ROMFS build emits entry count, metadata CRC, payload CRC, manifest CRC, and image SHA-256. The ROM probes and self-tests at boot; a complete matrix then visits 62 playable LVL sections across four episodes to prove the architecture is not demo-stage-specific.",

    /* Verification */
    "TyrianGbaPoc 使用 mGBA、SRAM telemetry、route golden 與資源 audit 驗證移植正確性與效能。": "TyrianGbaPoc validates correctness and performance with mGBA, SRAM telemetry, route goldens, and asset audits.",
    "Telemetry 與自動回歸 — TyrianGbaPoc 技術研究": "Telemetry and automated regression — TyrianGbaPoc research",
    "「看起來能玩」不是完成條件。": "\"It looks playable\" is not a completion criterion.",
    "長關卡、隨機掉落、cache 與一次性音樂很難只靠肉眼覆蓋。ROM 在 mGBA 中跑真實流程，再用 SRAM 回傳可機器判讀的契約。": "Long levels, random drops, caches, and finite music are hard to cover by sight alone. The ROM runs real flows in mGBA and returns a machine-readable contract through SRAM.",
    "SRAM 契約": "SRAM contract",
    "每個 focused AUTOTEST ROM 在完成後，把 magic、schema、pass flag 與 counters 寫到 SRAM，再以測試 SWI 結束。PowerShell host 讀回 little-endian 欄位，任何 golden、budget 或 accounting 不符就讓建置失敗。": "Each focused AUTOTEST ROM writes magic, schema, pass flag, and counters to SRAM before exiting through a test SWI. The PowerShell host reads the little-endian fields; any golden, budget, or accounting mismatch fails the build.",
    "這比解析 emulator 畫面或 log 更穩定：ROM 本身知道目前 level position、 event cursor、敵人數、cache miss、music state 與 pending frame。": "This is more stable than parsing emulator images or logs: the ROM itself knows the level position, event cursor, enemy count, cache misses, music state, and pending frame.",
    "不是一支超長測試": "Not one enormous test",
    "測試分層": "Layered tests",
    "完整第一關": "Complete first stage",
    "來源事件、Boss、獎賞、聲音、破關飛行、一次性歌曲、統計與回 Game Menu。": "Source events, boss, rewards, sound, exit flight, finite music, statistics, and return to Game Menu.",
    "Episode 2／3／4 第一關與 Episode 1 四關 campaign，驗證 script 轉場與不同資產 bank。": "Episode 2/3/4 stage one and a four-stage Episode 1 campaign verify script transitions and different asset banks.",
    "四個 Episode 共 62 個 playable sections，讀事件、enemy pool、背景與 Sprite2 catalog。": "Sixty-two playable sections across four episodes read events, enemy pools, backgrounds, and the Sprite2 catalog.",
    "Death、Demo、JukeBox、Arcade equipment、frontend、Next Level camera 與 960 次靜態轉場。": "Death, Demo, JukeBox, Arcade equipment, frontend, Next Level camera, and 960 static-menu transitions.",
    "用 accounting 找出「少了什麼」": "Use accounting to discover what went missing",
    "只檢查 final state 會放過中間偷漏。專案額外要求：": "Checking only final state can miss losses along the way. The project also requires:",
    "VRAM uploads = cache misses，upload bytes 符合 tile size；": "VRAM uploads = cache misses, with upload bytes matching tile size;",
    "效能也要有狀態來源": "Performance needs state attribution too",
    "Missed VBlank 分為 gameplay、stats、game over、transition 與 frontend other。High／Normal 現行完整 route：": "Missed VBlanks are split into gameplay, stats, game over, transitions, and other frontend work. Current complete High/Normal routes record:",
    "這些數字不是宣稱任何情境永不 drop frame；它們是固定輸入下的 regression baseline。若程式改動讓同一路徑多出 deterministic miss，建置立即失敗。": "These numbers do not claim that every possible situation is frame-drop-free. They are regression baselines under fixed input; if a change adds a deterministic miss to the same route, the build fails immediately.",
    "自動測試仍不能取代視覺 QA": "Automation still cannot replace visual QA",
    "CRC 可證明資產沒變，不能判斷 priority 是否符合人眼預期；event count 也不能判斷字體好不好看。專案同時保留 deterministic screenshot ROM、 pixel comparison 與人工試玩。AI 建議亦必須經這三層證據交叉確認。": "A CRC proves an asset did not change, not that priority looks correct; an event count cannot judge typography. The project retains deterministic screenshot ROMs, pixel comparisons, and hands-on play. AI suggestions must also pass these three layers of evidence.",
    "Mature port 的意思不是「不再出 bug」，而是每次修復都盡量留下能阻止 同類問題回來的測試契約。": "A mature port is not one that never has another bug. It is one where each fix leaves a test contract that helps prevent the same class of bug from returning.",

    /* Rendering */
    "Tyrian PC gameplay space 到 GBA 240×160 的 1:1 柔性裁切鏡頭、多圖層、背景串流與繪圖順序移植策略。": "Porting strategy for Tyrian's PC gameplay space: a 1:1 soft-crop GBA viewport, multiple layers, background streaming, and draw order.",
    "多圖層與柔性裁切鏡頭 — TyrianGbaPoc 技術研究": "Layers and a soft-crop camera — TyrianGbaPoc research",
    "不縮放 gameplay，讓裁切範圍成為鏡頭。": "Do not scale gameplay; turn the crop into a camera.",
    "原版座標與關卡演算法保留在 PC gameplay space；GBA 以 240×160 柔性視窗使用被裁掉的邊界，不參與敵人位置或碰撞重算。": "Source coordinates and level algorithms remain in PC gameplay space. A 240×160 GBA soft viewport uses the cropped margins without participating in enemy placement or collision calculations.",
    "兩個座標空間": "Two coordinate spaces",
    "座標空間": "Coordinate spaces",
    "Tyrian 的完整 PC 畫面是 320×200，但戰鬥視窗不是整個畫面。右側儀表板與 下方狀態區移除後，專案使用 264×184 的原始 gameplay space。GBA 顯示為 240×160，因此水平與垂直各捨去 24 pixels：": "Tyrian's complete PC screen is 320×200, but combat does not occupy all of it. After removing the right dashboard and lower status strip, the project uses the source 264×184 gameplay space. The GBA displays 240×160, leaving a 24-pixel difference on each axis:",
    "玩家、敵人、子彈、碰撞與 level event 仍使用 source coordinates。 只有 presentation 把 world coordinate 減去裁切 origin。這避免逐物件縮放、 rounding drift 與碰撞規格分叉，也讓畫面密度保持原版感。": "Players, enemies, projectiles, collision, and level events still use source coordinates. Only presentation subtracts the crop origin from world coordinates. This avoids per-object scaling, rounding drift, and divergent collision rules while preserving source visual density.",
    "`(36, 12)` 是鏡頭的中立原點，不再是永遠固定的 crop。柔性鏡頭只改 presentation；關卡事件、敵人 AI、子彈與碰撞從不讀取 camera state。": "`(36, 12)` is the camera's neutral origin, no longer a permanently fixed crop. The soft camera changes only presentation; level events, enemy AI, projectiles, and collision never read camera state.",
    "把隱藏的 24 pixels 變成柔性鏡頭": "Turn the hidden 24 pixels into a soft camera",
    "柔性鏡頭": "Soft camera",
    "264×184 與 240×160 在兩軸都相差 24 pixels。固定置中裁切會把每一側 12 pixels 永久藏起來；現行實作則把這段 slack 當作有限的鏡頭行程。 玩家待在中央區域時畫面穩定不動，接近來源視野邊緣後才讓 crop origin 平順跟上。": "The 264×184 and 240×160 spaces differ by 24 pixels on both axes. A fixed center crop permanently hides 12 pixels per side; the current implementation turns that slack into bounded camera travel. The view stays still while the player is central and follows smoothly only near a source-view edge.",
    "水平 source origin X": "Horizontal source origin X",
    "垂直 source origin Y": "Vertical source origin Y",
    "中央 stationary dead zone": "Central stationary dead zone",
    "從 source player position 求目標。": "Derive the target from source player position.",
    "中心點為 `(156, 92)`；玩家超出 dead zone 的部分才轉成 camera target。": "The center is `(156, 92)`; only movement beyond the dead zone becomes a camera target.",
    "把目標限制在原本的裁切餘量。": "Clamp the target to the original crop slack.",
    "每軸最多移動 ±12 pixels，所以不會顯示到 264×184 gameplay space 之外。": "Each axis travels at most ±12 pixels, never revealing space outside the 264×184 gameplay area.",
    "以 Q8 fixed point 做一階平滑。": "Apply first-order smoothing in Q8 fixed point.",
    "每個 30 Hz logic tick 前進剩餘距離的 1/4，再四捨五入成硬體 scroll pixel； 小幅操作不會讓畫面顫動，快速靠邊也不會突然跳 12 pixels。": "Every 30 Hz logic tick advances one quarter of the remaining distance and rounds to a hardware scroll pixel. Small inputs do not jitter the view, while a fast move to the edge never jumps 12 pixels at once.",
    "所有 presentation 共用同一原點。": "All presentation shares one origin.",
    "三層 BG、玩家、敵人、子彈、爆炸與拾取物一起平移，原有 parallax 仍在各背景層自己的 source offset 上運作。": "All three BGs, the player, enemies, projectiles, explosions, and pickups move together. Source parallax still runs on each background layer's own offset.",
    "增加的是 GBA 可用活動範圍，不是改大關卡": "The GBA view gains range; the level does not grow",
    "活動範圍": "Movement range",
    "固定中央 crop 的早期版本為避免船體被切掉，曾把玩家 source Y 從 OpenTyrian 的 `10..160` 收窄為 `17..152`。柔性鏡頭抵達上下邊界後， release 可以恢復原始 `JE_playerMovement()` 的 `10..160`： 最上方時船體第一個有效 pixel 位於 screen Y=5，最下方時最後一個有效 pixel 位於 Y=155，兩端仍完整可見。": "To keep the ship from being clipped under a fixed center crop, an early build narrowed OpenTyrian's source Y range from `10..160` to `17..152`. Once the soft camera can reach both vertical limits, release gameplay restores `JE_playerMovement()`'s original `10..160`: the first visible ship pixel sits at screen Y=5 at the top, and the last at Y=155 at the bottom.",
    "因此「活動範圍增加」是相對於固定 GBA crop：玩家重新取得原版 source viewport 的邊界空間。PC 關卡尺寸、座標、碰撞與事件觸發範圍都沒有擴張， 也不會因鏡頭位置產生另一套遊戲規則。": "The added range is relative to the fixed GBA crop: the player regains the edge space of the source viewport. PC level dimensions, coordinates, collision, and event-trigger ranges do not expand, and camera position never creates a second rule set.",
    "鏡頭移動也必須納入背景串流": "Camera motion must participate in background streaming",
    "背景串流": "Background streaming",
    "Tyrian 背景是持續捲動的 tile map，不是 264×184 靜態圖片。只改 GBA `VOFS` 會讓鏡頭在 Y=0 或 Y=24 時讀到尚未安裝的 ring rows，形成上下 邊界直條破圖。現行 scheduler 使用與硬體相同的 camera-adjusted presentation scroll；map scroll 或 camera scroll 任一方跨越 8-pixel 邊界，都會排程新露出的 top／bottom row。": "Tyrian backgrounds are continuously scrolling tile maps, not static 264×184 images. Changing only GBA `VOFS` lets the camera sample ring rows that are not installed at Y=0 or Y=24, producing broken stripes at the borders. The current scheduler uses the same camera-adjusted presentation scroll as the hardware; whenever map or camera scroll crosses an 8-pixel boundary, the newly exposed top/bottom row is scheduled.",
    "所有 active layers 會先 preflight。若該 VBlank 暫時無法完成必要 row DMA，Q8 camera target 只延後一個 logic tick，不會先顯示 stale tile。 背景 ownership 另保留 21 個可見 rows，加上上下各 2 個 hysteresis rows； 這個 25-row window 避免玩家在邊界折返時反覆釋放、重建相同資料。": "All active layers preflight first. If the required row DMA cannot finish in that VBlank, the Q8 camera target waits one logic tick rather than showing stale tiles. Background ownership retains 21 visible rows plus two hysteresis rows above and below; this 25-row window avoids releasing and rebuilding the same data when the player reverses near an edge.",
    "21 visible + 上下各 2": "21 visible + 2 above and below",
    "Episode 2 的 High／Normal 壓力路線中，25-row 柔性鏡頭與固定 crop 都是 41 missed VBlanks；也就是增加視野活動範圍後，背景換列成本仍回到 固定鏡頭基線。dynamic frame drop 保存的 held window 也使用 camera-adjusted scroll，延後 presentation 時不會提前釋放仍在畫面上的 row。": "In the Episode 2 High/Normal stress route, both the 25-row soft camera and fixed crop record 41 missed VBlanks. Added viewing range therefore returns background-row cost to the fixed-camera baseline. Dynamic frame drop also stores its held window using camera-adjusted scroll, so deferred presentation never releases a row that is still visible.",
    "三層不是三張裝飾圖": "Three layers are not three decorative images",
    "三個圖層": "Three layers",
    "原版每層具有獨立捲動速度、水平 phase、是否位於敵人前後方等狀態。 GBA 使用三個 tile background 保存 layer identity，不能把缺少的 tile 用「看起來最接近」的圖補上，否則岩壁、雲與可破壞物件會在後續事件中錯位。": "Each source layer has its own scroll speed, horizontal phase, and position before or after enemy groups. Three GBA tile backgrounds preserve those identities. A missing tile cannot be replaced with the nearest-looking one, or walls, clouds, and destructible objects will diverge in later events.",
    "底層地形：通常最慢或作為固定地面。": "Bottom terrain: usually slowest or treated as fixed ground.",
    "中層：可依 source flag 移到部分 enemy group 前方。": "Middle layer: source flags may place it in front of some enemy groups.",
    "上層／雲層：需要與 sky enemy、top enemy 分開排序。": "Upper/cloud layer: ordered separately from sky and top enemies.",
    "把 PC draw order 翻成有限 priority": "Translate PC draw order into finite priorities",
    "繪圖順序": "Draw order",
    "PC renderer 可以依序畫很多群組；GBA 只有有限 BG priority 與 OBJ priority。 專案沒有為某張錯圖硬寫例外，而是把 OpenTyrian 的": "A PC renderer can draw many groups sequentially; the GBA has only finite BG and OBJ priorities. Instead of hard-coding an exception for one bad frame, the project turns OpenTyrian's",
    "等規則整理成 可枚舉的 priority policy。": "rules into an enumerable priority policy.",
    "回歸測試會跑完整組合，現行契約包含 252 個 layer-rule checks。這讓修正 Episode 1 的雲層時，不會無意破壞 Episode 4 的前景物件。": "Regression exercises every combination; the current contract contains 252 layer-rule checks. Fixing Episode 1 clouds therefore cannot silently break an Episode 4 foreground object.",
    "為何 1:1 更便宜": "Why 1:1 is cheaper",
    "運算成本": "Compute cost",
    "ARM7TDMI 沒有硬體除法器。若每個 sprite、子彈與碰撞 box 都做 240/264、 160/184 的比例換算，除了 CPU cost，也會引入不同的四捨五入結果。 1:1 crop 把成本收斂成整數加減、OAM clipping 與 background offset， 將有限 cycle 留給敵人 AI、碰撞與資源快取。": "ARM7TDMI has no hardware divider. Scaling every sprite, projectile, and collision box by 240/264 and 160/184 would cost CPU time and introduce divergent rounding. A 1:1 crop reduces the work to integer addition/subtraction, OAM clipping, and background offsets, preserving cycles for enemy AI, collision, and asset caches.",
    "柔性裁切後的目前版本第一關多圖層場景": "Current first-stage layered scene through the soft crop",

    /* Palette training */
    "TyrianGbaPoc 如何在 GBA 4bpp、每塊 tile 只有 15 個可見色的限制下，以 build-time 感知色盤訓練保留 PC 原版觀感。": "How TyrianGbaPoc uses build-time perceptual palette training to preserve the PC look under GBA 4bpp's 15-visible-color limit per tile.",
    "受限色盤訓練 — TyrianGbaPoc 技術研究": "Constrained palette training — TyrianGbaPoc research",
    "15 色之內，找回沙地與樹的關係。": "Recover the relationship between sand and trees in 15 colors.",
    "GBA 不能直接顯示 Tyrian 的 256 色背景。本專案把色盤選擇變成可量測、 可回歸的 build-time 最佳化問題，讓有限色彩優先保住人眼真正注意的 色相、明暗與物件邊界。": "The GBA cannot display Tyrian's 256-color backgrounds directly. This project turns palette selection into a measurable, regression-tested build-time optimization problem, spending limited colors first on the hues, lightness, and object boundaries people actually notice.",
    "Episode 4 SURFACE 相同鏡位比較：左側是 PC 原始 LVL、SHP 與色盤，右側是 GBA v57 固定 4bpp 色盤結果": "Episode 4 SURFACE at the same camera composition: source PC LVL/SHP/palette on the left, fixed GBA v57 4bpp result on the right",
    "Episode 4 · SURFACE · 相同 camera composition": "Episode 4 · SURFACE · same camera composition",
    "左：PC stock 右：GBA v57 4bpp": "Left: stock PC · Right: GBA v57 4bpp",
    "本文的「訓練」不是在 GBA 上執行神經網路。它是離線分析原始關卡資料， 反覆最佳化 palette bank 並通過感知誤差門檻；ROM 執行時只讀取已完成的 tile 與色盤，因此不增加每幀成本。": "Here, \"training\" does not mean running a neural network on the GBA. Source level data is analyzed offline, palette banks are iteratively optimized, and perceptual-error gates are enforced. The ROM reads only finished tiles and palettes, adding no per-frame cost.",
    "真正的硬體邊界": "The real hardware boundary",
    "硬體限制": "Hardware constraint",
    "Tyrian 的 PC 素材以 256 色索引表示。GBA Mode 0 的 4bpp text background 則要求每塊 8×8 tile 選擇一個 16-entry palette bank，其中 index 0 用作透明，因此一塊 tile 實際只剩": "Tyrian's PC art uses 256 indexed colors. A GBA Mode 0 4bpp text background requires every 8×8 tile to select one 16-entry palette bank, with index 0 reserved for transparency. That leaves only",
    "15 個可見色": "15 visible colors",
    "。 對單一材質不成問題，但一塊 tile 同時含綠葉、褐色樹幹與沙地時， 兩組色相與亮度階梯必須共享這 15 個位置。": ". A single material fits, but when one tile contains green leaves, a brown trunk, and sand, both hue families and their lightness ramps must share those 15 slots.",
    "每 tile 可見色上限": "visible colors per tile",
    "綠色方塊從哪裡來": "Where the green blocks came from",
    "錯誤根因": "Failure cause",
    "Episode 4 第一個可玩關卡 SURFACE 曾在樹木四周出現規則的綠色方塊。 幾何、LVL 與 SHP 都沒有讀錯；問題是這些 tile 的": "SURFACE, the first playable Episode 4 stage, once showed regular green blocks around its trees. Geometry, LVL, and SHP data were all correct; the problem was that these tiles' ",
    "同時包含綠色與沙褐色，舊流程卻把它們 指派給單純的綠色 bank。結果樹葉仍像樹葉，周圍本應是沙地的像素卻也 被量化成綠色，8×8 邊界因此清楚浮現。": "contains both green and sand-brown, but the old path assigned them to a purely green bank. Leaves still looked like leaves, while nearby sand pixels also quantized to green, exposing clear 8×8 boundaries.",
    "舊驗收還要求「相鄰亮度 ramp 的碰撞數不得增加」。這個數字看似合理， 卻可能獎勵錯誤答案：多個沙色亮度映到不同的綠色，collision 很少， 人眼看到的色相卻完全錯誤。問題不在 GBA 少了更多顏色，而是最佳化目標 沒有代表真正的視覺品質。": "The old acceptance gate also required that collisions between adjacent lightness-ramp entries never increase. It sounded reasonable but could reward a wrong answer: several sand levels could map to distinct greens, producing few collisions while looking completely wrong. The issue was not simply too few GBA colors; the optimization target did not represent real visual quality.",
    "從原始資料訓練，而非逐關修圖": "Train from source data, not hand-fix each stage",
    "訓練流程": "Training process",
    "建置工具掃描完整 LVL、shape 與原始 palette，依「實際會出現的 tile」 建立 source index histogram、畫面曝光量與 hue mask。這次資料集涵蓋 62 個實體 LVL sections、75,555 個 per-level runtime keys； 合併後仍有 33,360 個 profile unique keys。流程不含 Episode 4 專用 手工色票，後續關卡會走同一套規則。": "The build scans complete LVL data, shapes, and source palettes, then derives source-index histograms, screen exposure, and hue masks from tiles that can actually appear. This dataset covers 62 physical LVL sections and 75,555 per-level runtime keys, merging to 33,360 profile-unique keys. It contains no Episode 4 hand-picked swatch; later stages follow the same rules.",
    "建立安全 baseline。": "Establish a safe baseline.",
    "保留原始 bank 作 fallback，只在未占用 bank 訓練候選色。": "Keep source banks as fallback and train candidate colors only in unused banks.",
    "用感知色彩空間計分。": "Score in perceptual color spaces.",
    "OKLab 保持穩定的最佳化方向，CIEDE2000 檢查人眼可見差異。": "OKLab supplies a stable optimization direction; CIEDE2000 checks human-visible differences.",
    "找出最壞反例。": "Find the worst counterexample.",
    "若 mixed-hue mask 仍有高誤差，把最大違規 key 依 histogram 與實際曝光量加權送回訓練。": "If a mixed-hue mask still has high error, weight its worst key by histogram and real exposure and feed it back into training.",
    "逐輪補強約束。": "Strengthen constraints each iteration.",
    "反例權重隨 iteration 增加，直到共同 palette 不再犧牲其中一類真實 tile。": "Counterexample weight rises each iteration until a shared palette no longer sacrifices either family of real tiles.",
    "硬性驗收。": "Apply hard acceptance gates.",
    "每個 key 的 OKLab、CIEDE2000 都不得退步，lightness inversion 也不得增加。": "No key may regress in OKLab or CIEDE2000, and lightness inversions may not increase.",
    "烘焙並回歸。": "Bake and regress.",
    "結果寫入 ROM 資產，再由實際 mGBA route 與全關 telemetry 驗證。": "Write the result into ROM assets, then verify it with real mGBA routes and all-level telemetry.",
    "SURFACE 的實測結果": "Measured SURFACE results",
    "量化結果": "Quantitative results",
    "新流程把": "The new path assigns",
    "指派給可同時表達綠葉與沙地的 mixed bank 4。 所有 867 個 SURFACE runtime keys 在兩種感知指標中都沒有退步；樹木區域 的高誤差像素大幅收斂，規則綠塊也從實際 mGBA 畫面消失。": "to mixed bank 4, which can express both leaves and sand. None of 867 SURFACE runtime keys regress under either perceptual metric. High-error tree pixels converge sharply, and the regular green blocks disappear from an actual mGBA frame.",
    "全關 OKLab 誤差改善": "all-level OKLab error improvement",
    "全關 CIEDE2000 改善": "all-level CIEDE2000 improvement",
    "兩種指標 regressed keys": "regressed keys across both metrics",
    "樹木區域 mean CIEDE2000": "tree-region mean CIEDE2000",
    "樹木區域 P95 CIEDE2000": "tree-region P95 CIEDE2000",
    "頁首對照圖刻意使用相同 camera composition：左側直接由 stock": "The header comparison deliberately uses the same camera composition. The left side is reconstructed directly from stock",
    "重建；右側只替換成最終 GBA 4bpp 資產。 這能隔離色盤轉換本身，不讓敵人生成時機或鏡頭差異干擾判讀。 另外也以新 ROM 的實際 mGBA frame 確認相同缺陷已消失。": "; the right changes only to final GBA 4bpp assets. This isolates palette conversion from enemy timing and camera differences. An actual frame from the new ROM also confirms that the same defect is gone.",
    "AI 協作，但不用 AI 當規格": "AI collaboration without making AI the specification",
    "AI 協作": "AI collaboration",
    "這次研究曾把硬體限制、真實 histogram、per-key 誤差與 ramp telemetry 交給 Gemini 3.1 Pro 複核。它指出 raw collision 不適合作 hard gate， 並建議先採 mask-only counterexample refinement；Codex 再依專案資料實作、 建置與跑完整回歸。AI 提供假說與審查角度，最後裁決仍來自": "This study gave hardware limits, real histograms, per-key errors, and ramp telemetry to Gemini 3.1 Pro for review. It identified raw collision count as a poor hard gate and suggested mask-only counterexample refinement first. Codex then implemented, built, and ran full regression against project data. AI supplied hypotheses and review angles; the final decision still came from",
    "原始素材、數學指標與模擬器實測": "source assets, mathematical metrics, and emulator measurements",
    "不是無限色，也不是零代價": "Neither unlimited color nor zero cost",
    "剩餘限制": "Remaining limits",
    "15 色同時承載兩組 hue family，仍會壓縮少量相鄰亮度層次；這是 4bpp 的真實邊界。新方法選擇的是「保住色相與物件關係，再犧牲較不顯眼的 ramp 細節」。若未來出現單一 mask 無法同時服務的情況，才會評估 key-specific override；在此之前維持簡單 runtime schema，更節省 ROM、 RAM 與 CPU，也比較容易證明所有關卡沒有回歸。": "Fifteen colors carrying two hue families still compress some adjacent lightness levels; that is a real 4bpp boundary. The new method preserves hue and object relationships before less-visible ramp detail. A key-specific override will be considered only if one mask truly cannot serve both. Until then, a simple runtime schema saves ROM, RAM, and CPU and makes all-level non-regression easier to prove.",
    "色盤訓練不是截圖濾鏡。它必須以所有可能出現的 runtime keys 驗收， 不能只讓一張示範圖變漂亮；否則同一關稍後的 tile 仍可能換一種方式破圖。": "Palette training is not a screenshot filter. Acceptance must cover every runtime key that can appear, not just one attractive demo image; otherwise a later tile in the same stage can fail differently.",

    /* Audio */
    "TyrianGbaPoc 如何把原版 TYM、SND 與語音轉為 GBA Maxmod soundbank，校準聲道響度、抑制刺耳瞬態並消除切歌爆音。": "How TyrianGbaPoc converts source TYM, SND, and voices into a GBA Maxmod soundbank, calibrates channel loudness, controls harsh transients, and removes transition clicks.",
    "音樂與音效的生成及校準 — TyrianGbaPoc 技術研究": "Generating and calibrating music and sound effects — TyrianGbaPoc research",
    "不只把聲音放大，而是保留原曲內部的關係。": "Preserve the relationships inside the music, not merely its loudness.",
    "PC 原版的 OPL2 音樂不能原封不動交給 GBA 播放。本專案將事件、音色與 聲道關係轉成 Maxmod 可負擔的 tracker／PCM 表示，並以離線量測修正聲道 響度、鼓聲瞬態、有限提示曲與場景切換。": "The PC version's OPL2 music cannot be handed unchanged to the GBA. Events, timbres, and channel relationships are translated into a tracker/PCM representation Maxmod can afford, while offline measurements correct channel loudness, percussion transients, finite cues, and scene transitions.",
    "兩條來源路徑，一個 runtime soundbank": "Two source paths, one runtime soundbank",
    "音樂仍由 ROMFS 的": "Music flow is still selected by ROMFS",
    "與原始 song ID 決定流程； build 端使用 41 首 TYM 表示保存 LDS／OPL2 pattern、patch、note、 channel volume 與 carrier total-level，再轉成 Maxmod 的 IT modules。 音效則直接由": "and original song IDs. At build time, 41 TYM representations preserve LDS/OPL2 patterns, patches, notes, channel volumes, and carrier total levels before conversion to Maxmod IT modules. Sound effects are taken directly from",
    "與": "and",
    "拆出原始 8-bit PCM。最後由": "as original 8-bit PCM. Finally,",
    "組成同一份": "combines everything into one",
    "Runtime 不以「第幾個 GBA 檔案」重寫遊戲規格。關卡、Game Menu、 Demo、JukeBox、Boss 與提示音仍傳入原版的 song／sound ID； GBA adapter 只負責找到相對應的 Maxmod module 或 sample。": "Runtime does not redefine the game in terms of GBA file numbers. Levels, Game Menu, Demo, JukeBox, bosses, and cues still pass source song/sound IDs; the GBA adapter only resolves the corresponding Maxmod module or sample.",
    "為什麼不能逐曲正規化": "Why per-track normalization is wrong",
    "早期轉換沿用為 SNES S-DSP／BRR 建立的 channel gain，還會把每首歌 最大的 voice gain 再正規化。結果是同一音色跨歌曲沒有共同響度基準， 而一首歌最強的聲道會連帶改變其他聲道比例。稀疏鼓聲尤其容易為了追上 整曲 RMS 而被放大成尖銳瞬態。": "The early conversion reused channel gains built for SNES S-DSP/BRR and normalized each song again by its largest voice gain. The same timbre therefore had no common loudness reference across songs, and a track's strongest voice changed the balance of every other voice. Sparse percussion was especially likely to be amplified into a sharp transient merely to match whole-song RMS.",
    "新流程以": "The new path uses",
    "的完整 OPL2 stem RMS 作固定 reference，對 Maxmod 真正使用的 signed 8-bit sample、 IT volume-column 量化與 runtime module volume": "complete OPL2 stem RMS values as a fixed reference, measuring the signed 8-bit samples, IT volume-column quantization, and runtime module volume that Maxmod actually uses at",
    "重新量測。原始參考與 GBA 內建喇叭都是 mono，因此以": ". Both the source reference and the GBA internal speaker are mono, so calibration uses an",
    "fold-down 比對，但 IT 內的 stereo pan 仍完整保留。": "fold-down while preserving stereo pan inside the IT module.",
    "保留 TYM event velocity 已包含的 carrier loudness；": "retain carrier loudness already present in TYM event velocity;",
    "全 catalog 只使用一次固定 +3 dB presentation gain；": "apply one fixed +3 dB presentation gain across the catalog;",
    "不做 per-track 或 per-sample master normalization；": "perform no per-track or per-sample master normalization;",
    "8-bit 量化後重新量測，clipping 或缺少 target 會讓 build 失敗。": "measure again after 8-bit quantization; clipping or a missing target fails the build.",
    "鼓聲要看 peak，不只看 RMS": "Percussion needs peak control, not RMS alone",
    "短 one-shot 的平均能量很低，若只追 RMS，kick、snare、hi-hat 或 crash 的單一下 peak 可能被推得過高。校準器因此另以原版 peak 的": "A short one-shot has low average energy. If calibration chases RMS alone, a kick, snare, hi-hat, or crash peak may be pushed far too high. The calibrator therefore also uses",
    "作軟上限；22 個稀疏 percussion sources 寧可略低於 RMS target，也不製造刺耳爆點。": "of the source peak as a soft ceiling. Twenty-two sparse percussion sources may fall slightly below their RMS target instead of creating harsh spikes.",
    "程序鼓聲依類型使用不同 sample length，不再共用同一段 46 ms noise。 每個 one-shot 使用 1.5 ms smoothstep attack 與 5 ms release，並只移除 極低頻 DC；沒有對全部曲目強套 120 Hz high-pass 或 8 kHz shelf， 避免把不同 Tyrian 音色 EQ 成同一種聲音。": "Procedural percussion uses different sample lengths by drum type instead of sharing one 46 ms noise burst. Every one-shot gets a 1.5 ms smoothstep attack and 5 ms release, with only very-low-frequency DC removed. No global 120 Hz high-pass or 8 kHz shelf forces every Tyrian timbre through the same EQ.",
    "原版 SFX 與語音不經手工替代": "Source SFX and voices, not hand-made substitutes",
    "內 29 個一般音效與": "contains 29 ordinary effects, while",
    "內 9 段語音以原始 archive offset 拆出，轉為 11,025 Hz mono 8-bit WAV。 與 OpenTyrian 的": "contains 9 voices. Both are extracted by source archive offsets and written as 11,025 Hz mono 8-bit WAV. Matching OpenTyrian's",
    "相同，每段 voice 都移除 尾端 100-byte 損壞資料，不只處理 Level End。遊戲中的爆炸、打擊、 Cube、Boss 與提示語音仍依 source sound number 呼叫。": ", every voice removes its corrupt 100-byte tail, not only Level End. Explosions, hits, Cubes, bosses, and spoken cues are still called by source sound number.",
    "極限全武器壓力測試可以選擇把密集 SFX 導向 GBA 原生 DMG square channel，以隔離 sample-mixer voice pressure；這是診斷 profile， 一般遊戲仍使用完整 Maxmod SFX。": "The all-weapons stress test may route dense effects through a native GBA DMG square channel to isolate sample-mixer voice pressure. That is a diagnostic profile; normal gameplay retains full Maxmod SFX.",
    "循環曲與只播一次的提示曲": "Looping music and finite cues",
    "Maxmod 的": "Maxmod's",
    "只控制 order list 結束後的行為； 原始 IT 內若仍有": "controls only what happens after the order list ends. If the source IT still contains a",
    "跳轉，歌曲仍會無限循環。因此 End of Level（09）、Game Over（10）與 Secret Level（30）各保留兩種 module： 一般版供 JukeBox 循環，": "jump, the song still loops forever. End of Level (09), Game Over (10), and Secret Level (30) therefore keep two modules: a normal version for JukeBox looping and an",
    "版只移除 Bxx，讓遊戲流程自然 播到結尾停止。": "version that removes only Bxx so game flow reaches the real ending and stops.",
    "兩種檔案的 PCM payload 完全相同，Maxmod 也會全域去重相同 sample。 三份有限 order／pattern 只讓 soundbank 增加 4,028 bytes，換來正確的 勝利、死亡與秘密關卡播放語意。": "The two files have identical PCM payloads, and Maxmod globally deduplicates matching samples. Three finite order/pattern sets add only 4,028 bytes to the soundbank while restoring correct victory, death, and secret-level semantics.",
    "切歌先跨過零點": "Cross zero before changing music",
    "直接在 Direct Sound FIFO 還含非零波形時": "Calling",
    "， 容易在選單進關卡的一瞬間產生爆音。現行 VBlank 狀態機只淡出背景 module，保留 UI 確認音效：": "while the Direct Sound FIFO still contains a non-zero waveform can click as the menu enters a level. The current VBlank state machine fades only the background module and preserves the UI confirmation effect:",
    "18 VBlanks 淡出。": "Fade out over 18 VBlanks.",
    "選單歌曲平順降到零。": "Bring menu music smoothly to zero.",
    "1 個完整靜音 VBlank。": "Hold one full silent VBlank.",
    "讓 Maxmod 送出靜音 buffer。": "Let Maxmod submit a silent buffer.",
    "停止並載入場景。": "Stop and load the scene.",
    "切換 module 與關卡資料。": "Switch the module and level data.",
    "30 VBlanks 淡入。": "Fade in over 30 VBlanks.",
    "新關卡歌曲從零開始。": "Start the new level song from zero.",
    "暫停則比照來源邏輯，只把 module volume 降為一半；死亡淡出、勝利與 Game Over 的 natural stop 都由 telemetry 計數，避免日後又退回無限循環。": "Pause follows source behavior and only halves module volume. Death fade plus victory and Game Over natural stops are counted by telemetry so they cannot silently regress to infinite loops.",
    "把聽感變成可重建的證據": "Turn listening into reproducible evidence",
    "Build 會輸出逐曲、逐 source 的 gain、RMS error、peak ceiling、 clipping 與 track mapping 報告；回歸另驗證 41 首 catalog、三首 finite cues、場景淡出／靜音／淡入步數、自然停止次數與所有 source SFX ID。人耳 A/B 仍不可取代，但每次修改不必再從零猜測「哪個聲道變大」。": "The build reports per-track and per-source gain, RMS error, peak ceiling, clipping, and track mapping. Regression also verifies the 41-song catalog, three finite cues, scene fade/silence/fade-in steps, natural-stop counts, and every source SFX ID. Human A/B remains essential, but each change no longer starts by guessing which channel became louder.",
    "離線量測不是宣稱已取得實機 LUFS 真值。它提供固定、可重現的比較基準； 最後仍要在 mGBA 與真實 GBA 輸出上檢查音樂、音效及語音的相對感受。": "Offline measurement is not a claim of true hardware LUFS. It provides a fixed, reproducible comparison baseline; final music, effects, and voice balance still requires listening through mGBA and real GBA output.",
  });

  const textRecords = [];
  const attributeRecords = [];
  let initialized = false;
  let currentLanguage = DEFAULT_LANGUAGE;

  function normalize(value) {
    return value.replace(/\s+/g, " ").trim();
  }

  function translatedText(source) {
    const key = normalize(source);
    const value = english[key];
    if (!value) return null;
    const leading = source.match(/^\s*/)?.[0] ?? "";
    const trailing = source.match(/\s*$/)?.[0] ?? "";
    return `${leading}${value}${trailing}`;
  }

  function collectTextNodes() {
    const walker = document.createTreeWalker(
      document.body,
      NodeFilter.SHOW_TEXT,
      {
        acceptNode(node) {
          const parent = node.parentElement;
          if (!parent || parent.closest("script, style, pre")) {
            return NodeFilter.FILTER_REJECT;
          }
          return normalize(node.nodeValue || "")
            ? NodeFilter.FILTER_ACCEPT
            : NodeFilter.FILTER_REJECT;
        },
      },
    );

    let node;
    while ((node = walker.nextNode())) {
      const source = node.nodeValue;
      const translated = translatedText(source);
      if (translated !== null) {
        textRecords.push({ node, source, translated });
      }
    }
  }

  function collectAttributes() {
    const selectors = [
      ["[aria-label]", "aria-label"],
      ["[alt]", "alt"],
      ["meta[name='description']", "content"],
    ];
    selectors.forEach(([selector, name]) => {
      document.querySelectorAll(selector).forEach((node) => {
        const source = node.getAttribute(name);
        if (!source) return;
        const translated = english[normalize(source)];
        if (translated) {
          attributeRecords.push({ node, name, source, translated });
        }
      });
    });
  }

  function readStoredLanguage() {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored === "en" || stored === "zh") return stored;
    } catch {
      // file:// privacy modes may reject storage. English remains the default.
    }
    return DEFAULT_LANGUAGE;
  }

  function updateLanguageButtons() {
    document.querySelectorAll("[data-language-option]").forEach((button) => {
      const active = button.dataset.languageOption === currentLanguage;
      button.classList.toggle("active", active);
      button.setAttribute("aria-pressed", String(active));
    });
  }

  function setLanguage(language, persist = true) {
    currentLanguage = language === SOURCE_LANGUAGE
      ? SOURCE_LANGUAGE
      : DEFAULT_LANGUAGE;
    const useEnglish = currentLanguage === DEFAULT_LANGUAGE;

    textRecords.forEach(({ node, source, translated }) => {
      node.nodeValue = useEnglish ? translated : source;
    });
    attributeRecords.forEach(({ node, name, source, translated }) => {
      node.setAttribute(name, useEnglish ? translated : source);
    });

    const sourceTitle = document.documentElement.dataset.sourceTitle;
    if (sourceTitle) {
      const translated = english[normalize(sourceTitle)];
      document.title = useEnglish && translated ? translated : sourceTitle;
    }
    document.documentElement.lang = useEnglish ? "en" : "zh-Hant";
    document.documentElement.dataset.language = currentLanguage;
    updateLanguageButtons();

    if (persist) {
      try {
        localStorage.setItem(STORAGE_KEY, currentLanguage);
      } catch {
        // The selected language still applies for the current page.
      }
    }
    document.dispatchEvent(
      new CustomEvent("tyrian-language-change", {
        detail: { language: currentLanguage },
      }),
    );
  }

  function addLanguageSwitcher() {
    const nav = document.querySelector("[data-nav]");
    if (!nav || nav.querySelector("[data-language-switcher]")) return;

    const group = document.createElement("div");
    group.className = "language-switcher";
    group.dataset.languageSwitcher = "";
    group.setAttribute("role", "group");
    group.setAttribute("aria-label", "Language / 語言");

    [
      ["en", "English"],
      ["zh", "中文"],
    ].forEach(([language, label]) => {
      const button = document.createElement("button");
      button.type = "button";
      button.dataset.languageOption = language;
      button.textContent = label;
      button.addEventListener("click", () => setLanguage(language));
      group.append(button);
    });
    nav.append(group);
  }

  function init() {
    if (initialized) return;
    initialized = true;
    document.documentElement.dataset.sourceTitle = document.title;
    collectTextNodes();
    collectAttributes();
    addLanguageSwitcher();
    setLanguage(readStoredLanguage(), false);
  }

  window.TyrianSiteLanguage = Object.freeze({
    init,
    setLanguage,
    getLanguage: () => currentLanguage,
    storageKey: STORAGE_KEY,
  });

  init();
})();
