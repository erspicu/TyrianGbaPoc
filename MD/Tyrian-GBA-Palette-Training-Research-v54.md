# Tyrian GBA 4bpp 背景調色盤訓練研究（v54）

日期：2026-07-30  
狀態：**方法研究完成，尚未修改產品程式或建置資源**

## 1. 本輪目的

本輪只研究如何改善 Tyrian PC 256 色背景轉為 GBA Mode 0 4bpp
背景的調色盤訓練，不修改 `src/`、runtime LUT 或既有資源。

已使用 `gemini-3.1-pro-preview` 進行三輪諮詢。第一輪回答有方向，
但混淆 BG 與 OBJ、忽略 runtime 的 mask assignment 限制；第二、三輪
針對這些問題追問並修正。最後再以色彩模型原始資料與標準來源交叉
檢查。本文是工程採用判斷，不是把 Gemini 回答直接當成規格。

三輪原始問答保存在：

- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-query-2026-07-30.md`
- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-response-2026-07-30.md`
- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-followup-query-2026-07-30.md`
- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-followup-response-2026-07-30.md`
- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-final-followup-query-2026-07-30.md`
- `C:\ai_project\AprTyrianNes\knowledgebase\message\TyrianGbaPoc-v54-palette-training-final-followup-response-2026-07-30.md`

## 2. 結論摘要

最值得後續實驗的方向不是單純把 RGB K-Means 換成另一個 K-Means，
而是同時改進五個層面：

1. 從全部 62 關 stock LVL／Shape 重建真正的 runtime cache-key
   dataset，保留每個 unique tile 的 histogram、hue mask、關卡／圖層
   與畫面曝光次數。
2. 以 **16 banks × 每 bank 15 個非零 BGR555 colours** 做聯合最佳化，
   在 bank colours 與 mask/key-to-bank assignment 間交替更新。
3. 主要色差改用 display-referred **OKLab squared distance**，但
   CIEDE2000、CAM16-UCS、ramp、edge 與 top-tail error 只作獨立
   report/gate，避免單一 metric gaming。
4. 平均 pixel fidelity 之外，保留 tile-level top-tail/CVaR，防止
   少數顯眼的混色 tile 被平均誤差掩蓋。
5. 先做不改 runtime 的 mask-only（M）baseline，再依實際 unique key
   數量與畫質收益評估 key-based（K）；compact feature override（F）
   是第三順位，不先憑空判定。

目前不應立即改 runtime。第一個合理產出應是離線 dataset 與實驗
runner，先證明新 objective 在全部關卡、worst tiles 與實際畫面上
都勝過 v53。

## 3. 現行 v53 基線

v53 的背景調色盤架構是：

- 五份 stock shape banks：`)`, `w`, `x`, `y`, `z`；
- 每關依原始 `shape_file` 選一套 adapter；
- 16 個 BG palette banks，每 bank 的 index 0 保留，另有 15 色；
- banks 0..10 保留十一個 single-hue brightness ramps；
- banks 11..15 依五組人工 hue-mask families 訓練；
- mixed banks 使用 hue-balanced weighted RGB squared-error Lloyd；
- runtime 用 64 KiB `hue_mask -> bank` dense LUT，再用
  `bank × source index -> local nibble` LUT pack 4bpp；
- shape-bank-specific 訓練相對全域 mixed palette 已改善
  2.2627%～10.4044% 的既有 weighted-RGB objective。

它的優點是 runtime 快、資料小、所有關卡有 fallback；限制則是：

- mixed mask 分組仍是人工指定；
- bank assignment 與 bank colours 沒有共同反覆最佳化；
- RGB squared error 不是視覺均勻 metric；
- 訓練 slices 沒有完整重播 LVL map、上下 shape 拼接與畫面曝光；
- 只看平均 pixel error，沒有保護最差 tile；
- 相同 hue mask 的不同 histogram 必須共用 bank，可能限制品質。

## 4. Gemini 回答中必須修正的部分

下列事項不可直接照 Gemini 原文實作：

| Gemini 中間結論 | 工程修正 |
|---|---|
| 對自機、Boss、子彈、UI 加背景訓練權重 | 本管線只處理 BG；這些主要使用 OBJ／其他 palette，必須排除 |
| 任意 tile 可選任意 bank | v53 runtime 目前是相同 hue mask 共用 bank；必須加入 equivalence-class constraint |
| `num_banks=15` | 硬體是 16 banks；15 是每 bank 可訓練的非零 colours |
| 整體離散訓練可得到 global optimum | 只有固定 cluster membership 後的單一 BGR555 centre 搜尋是 exact；其餘均是非凸局部解 |
| Top-K Hungarian 是 exact | 只對被保留的候選子集 exact；除非候選集擴至可證明涵蓋最佳解，否則仍是 approximate |
| 把一個 mask 聚合後計 CVaR | 會掩蓋同 mask 內的最差 tiles；CVaR 必須保留 tile/key 層級 |
| `OKLab ΔE=0.02` 是普遍 JND | 只能當工程 sweep 起點；實際可見差異受刺激尺寸、背景、顯示器與觀看條件影響 |
| AGB-001 可固定成 gamma 3.5 | 沒有本專案硬體量測前只能是 sensitivity transform，不能當 ground truth |
| 只需保留固定數量 N 的 exact banks | 還必須決定「哪幾個」hue banks；只 sweep N 不完整 |
| unique key 少就能直接 dense key table | cache key 空間約 25 bits，不能只按 active N 直接陣列索引；仍需 bucket/dictionary/verified hash |

另外，Gemini 第三輪正確撤回了第一輪的 BG/OBJ 混淆、任意 tile
assignment、錯誤的 OKLab `2.0` 尺度與過度昂貴的 26-neighbour
說法，這些修正已納入本文。

## 5. Dataset：先重建真正的 runtime 分布

### 5.1 每個 unique tile/key 的資料

後續離線工具應針對所有 62 關建立：

```text
profile / level / layer / segment
cache_key = (top_shape, bottom_shape, phase, sub_x)
hue_mask
source-index histogram[256]
non-zero pixel count
visible-instance / frame exposure
adjacent key IDs（可選）
```

`cache_key` 應使用和 `background_pattern_key()`、
`background_render_tile()` 完全相同的 bit layout 和上下 shape 拼接。
Build 端需對抽樣 keys 做 byte-for-byte round-trip，確認 Python 解出的
64 個 source indices 與 C runtime 一致。

### 5.2 Coverage 與 exposure 分開

- **Coverage set**：枚舉所有 LVL map、三層、所有合法 phase/sub-x 與
  上下相鄰 shape，確保任何 stock 關卡 key 都有 fallback／報告。
- **Exposure set**：重播真正捲動路徑與 viewport，統計玩家實際會看見
  的次數，用於 mean loss 權重。
- unique key 只存一次，重複出現用 exposure counter 表示，避免 dataset
  無謂膨脹。
- Boss 停留、捲動速度變化與事件停頓會改變曝光量；若腳本無法靜態
  得知，可由 AUTOTEST telemetry 補足。

### 5.3 Train/validation/final fit

- split 的用途是選 metric、CVaR、固定 bank 與 profile 數量，不是永久
  排除偶數關。
- 建議同時做：
  - by-level holdout；
  - by-shape-bank holdout；
  - by-spatial-segment holdout；
  - worst-mask/worst-key holdout。
- hyperparameters 選定後，用全部 62 關重新 final fit。
- 每一關仍要輸出 coverage、mean、P95/P99、CVaR、ramp inversion 和
  visual preview；不能只看全遊戲平均。

## 6. 建議的正式 objective

令：

- `t`：一個 unique runtime tile/key；
- `q_t`：該 tile 的畫面曝光次數；
- `h_t,c`：source colour `c` 在 tile 中的非零 pixel count；
- `n_t = sum_c h_t,c`；
- `f_t`：assignment feature，M 方案為 hue mask，K 方案為 cache key；
- `a(f_t)`：feature 指派的 bank；
- `P_b,j`：bank `b` 的第 `j` 個 BGR555 colour，`b=0..15`,
  `j=1..15`；
- `d(c,P)`：兩色經同一 display transform 後的 OKLab squared
  Euclidean distance。

單一 tile 在 bank `b` 的未正規化 error：

```text
D_t(b) = sum_c h_t,c * min_j d(c, P_b,j)
```

單一 tile 的 normalized error：

```text
e_t(b) = D_t(b) / n_t
```

### 6.1 Mean pixel fidelity

```text
L_mean =
    sum_t q_t * D_t(a(f_t))
    --------------------------------
    sum_t q_t * n_t
```

這一項保持真正的線性 pixel exposure，不在主要 fidelity loss 中把
tile 內 count 改成 `log1p` 或 `sqrt`。

### 6.2 Weighted top-tail / CVaR

取 confidence `alpha`，例如 `0.90 / 0.95 / 0.99`；`alpha=0.95`
表示關注最差 5%，不是 `0.05`：

```text
CVaR_alpha =
    min_eta [
        eta +
        1 / (1-alpha) *
        sum_t rho_t * max(e_t - eta, 0)
    ]
```

`rho_t` 正規化為總和 1。兩種權重都需做 ablation：

- `rho_t ∝ q_t`：每次可見 tile instance 權重相同；
- `rho_t ∝ q_t * n_t`：每個可見非零 pixel 權重相同。

第一種較能避免 sparse 但嚴重錯色的 tile 被低估；第二種與全畫面
pixel fidelity 一致，不能只靠文字判定，需以 worst-scene review 選擇。

### 6.3 Ramp 與一致性

Tyrian palette 已有：

```text
hue = source_index >> 4
brightness = source_index & 0x0f
```

所以不需人工 semantic JSON 即可建立第一版 ramp constraints。

對同一 hue family、相鄰 brightness levels，報告：

- mapped lightness inversion count；
- mapped collision count；
- 原本明顯亮度差被壓平的比例；
- hue-category crossing。

Soft penalty 可寫成：

```text
R_invert = sum u * max(0, L_map(r) - L_map(r+1))^2

R_collapse =
    sum u * max(
        0,
        kappa * (L_src(r+1)-L_src(r))
            - (L_map(r+1)-L_map(r))
    )^2
```

`kappa` 建議從 `0 / 0.25 / 0.5 / 0.75` sweep，不先寫死某個
`Min ΔL`。若使用 OKLab，lightness 以 0..1 記；若報告 CIELAB，
才使用 L*=0..100，兩者不可混用。

同一 source colour 在不同 banks/profiles 的 mapping consistency
可先作 report，之後才加入：

```text
C = sum_(c,b1,b2) v(c,b1,b2) *
    || mapped(c,b1) - mapped(c,b2) ||^2
```

`v` 只對真正相鄰、同場共現或跨關頻繁使用的組合加權，避免為了一致
而犧牲所有 profile-local capacity。

總 objective：

```text
J = L_mean
    + lambda_tail * CVaR_alpha
    + lambda_ramp * (R_invert + R_collapse)
    + lambda_consistency * C
```

除 `L_mean` 外的權重皆只是起始 hyperparameters，必須用 validation
與人工偏好決定。

## 7. 離線最佳化 pipeline

### 7.1 BGR555 distance table

Build-time 可預算：

```text
255 source colours × 32768 BGR555 candidates × float32
= 33,423,360 bytes（約 31.88 MiB）
```

這只存在開發 PC，不進 ROM。它讓固定 source cluster 的最佳離散
centre 可用窮舉精確求得。

### 7.2 三層交替最佳化

每個 profile 做 8～16 個 multi-start；其中至少一個 seed 使用 v53
palette，其他使用 weighted K-Means++／farthest mask-class seeds。

```text
for each multi-start:
    initialize 16 banks × 15 colours
    initialize mask/key -> bank assignments
    initialize tail_multiplier[t] = 1

    repeat outer IRLS:
        repeat middle assignment/palette loop:
            for bank b:
                W[b,c] =
                    sum over assigned tiles t:
                        q_t * tail_multiplier[t] * h_t,c

                optimize 15 BGR555 colours from W[b,*]

            for each assignment equivalence class f:
                choose bank minimizing:
                    sum over t with f_t=f:
                        q_t * tail_multiplier[t] * D_t(bank)
                    + applicable ramp/consistency delta

            compute true J and checkpoint best-so-far

        compute weighted eta and tile-level CVaR
        target_multiplier[t] =
            1 + lambda_tail/(1-alpha) when e_t is in tail
            1 otherwise
        damp multiplier update

        detect convergence or cycle

return the lowest true-J checkpoint across starts
```

重點：

- assignment 可受 `mask -> bank` 約束，但 CVaR 仍保留每個 tile；
- non-tail 必須保留 mean-loss 的 base weight 1，不能設成 0；
- weighted quantile 必須按 `rho_t`，不是按 mask 數量；
- VaR 邊界有 ties 時，需只取足夠的 fractional tail mass；
- damping/Polyak 只是穩定 IRLS 的 heuristic，不保證單調；
- 每輪都重算真正 `J` 並保留 best checkpoint；
- 可用 assignment/tail-set hash 偵測 2-cycle 或更長震盪。

建議起始停止條件：

- inner 15-colour loop：membership 不變，或 relative loss
  `< 1e-7` 連續兩輪，最多 20 輪；
- middle loop：assignment 不變且 `J` 改善 `< 1e-5` 連續三輪，
  最多 50 輪；
- outer IRLS：tail set 與 best `J` 穩定三輪，最多 30 輪；
- 以上均是起始值，需記錄 convergence curve。

### 7.3 單一 bank 的 15-colour solver

```text
repeat:
    1. 將有權重的 255 source colours 指派到最近 centre。
    2. Empty cluster 從最高 residual-cost source colour 重新 seed。
    3. 對每個固定 cluster 掃 32768 lattice colours，
       取得候選 colour 與 cost。
    4. 解決 15 centres 不可重複的 assignment。
    5. membership/cost 穩定後停止。
```

第 3 步對固定 cluster 是 exact。第 1～5 步整體仍是 local optimum。

重複 centre 的處理建議：

- 每 cluster 保存 top-K lattice candidates；
- 對候選 union 做 min-cost bipartite matching；
- K 由 `4, 8, 16, 32...` 自適應擴大，直到成本與選色穩定；
- 這只對目前 candidate union exact。除非掃描／證明被裁掉 candidates
  不可能改善，不能宣稱全域 exact；
- 每次 multi-start 保存無重複、最低真實 error 的結果。

Ramp penalty 若直接塞進 centre update 會破壞 additive cluster cost。
第一個 prototype 應先把 ramp 當 hard report/gate；第二輪再以局部
colour-swap/repair 最小化 ramp penalty，並確認 mean/CVaR 不退化。

## 8. Assignment 架構：M、K、F

### 8.1 M：Mask-only baseline

```text
a(t) = g(hue_mask(t))
```

優點：

- 不改 v53 runtime；
- 每 profile 64 KiB dense table；
- O(1) lookup；
- 是最乾淨的第一個離線比較基線。

缺點是相同 mask 的不同 material histogram 不能選不同 bank。

### 8.2 K：Cache-key assignment

```text
a(t) = g(top_shape, bottom_shape, phase, sub_x)
```

這最接近離線 tile-level optimum，而且 `background_render_tile()` 在
cache miss 時已持有 key。必須先統計每 profile 的 unique active
key 數量 `N`。

25-bit key 空間不能因 `N` 小就直接配置 `2^25` dense table。候選：

| Active keys/profile | 候選表示 | 初步評估 |
|---:|---|---|
| 幾百～約 4K | high-bit bucket + sorted full key/bank entries | 簡單、可驗證；只在 cache miss 查找 |
| 約 4K～32K | two-level bucket、verified minimal-perfect-hash 或 open addressing | 必須保存 full key/fingerprint 驗證，collision 時 fallback M |
| 更大 | 先比較 ROM/cycle，可能回到 M 或 F | 不先為 K 犧牲 gameplay |

這些只是資料結構候選，不是固定門檻。需量測：

- entries bytes/profile；
- lookup comparisons/cycles；
- cache miss 路徑 cycle；
- missed VBlank；
- 相對 M 的 mean/CVaR/visual improvement。

### 8.3 F：Ambiguous-mask compact override

保留 M 的 64 KiB primary table；只對離線顯示「同 mask 內最佳 bank
分歧大、且 tail error 高」的少數 masks，加入：

- brightness mean/range；
- dominant-hue count ratio；
- top source-index brightness bins；
- 小型 threshold tree 或 sorted override entries。

它不是 1 MiB dense LUT。只有 K 太大、而 M 的特定 masks 明顯失真時
才值得研究。任何「多 50 cycles 一定掉幀」的說法都必須由 telemetry
證明。

推薦順序：

```text
M baseline
  -> 離線計算 K 可得的品質上限與 N
  -> K 若收益/成本合理，做帶 M fallback 的 prototype
  -> K 太大或只有少數 masks 需要細分時，再做 F
```

## 9. Profiles 與固定 hue banks

### 9.1 Profiles

第一輪先保留 v53 的五個 shape-bank profiles，隔離「訓練演算法」
與「profile 數量」兩個變因。

第二輪才比較：

- P=5（v53 shape-bank）；
- P=8；
- P=12；
- P=16；
- per-level upper bound。

可做 agglomerative clustering：

1. 每關先得到自己的 palette objective upper bound；
2. 每次估算兩群共用 palette 的 merge loss；
3. 對候選最小的 merges 做完整 joint retraining；
4. 合併至 P profiles；
5. 以 validation levels 與跨 profile consistency 選 P。

因每次 merge retraining 很貴，可先用 cross-evaluation loss 篩選，再
對少量最佳 pair 做真實重訓。這是近似 hierarchical search，不是
全域最佳 clustering。

Dense mask table 每 profile 只有 64 KiB；即使 P=12 也只有 768 KiB。
在 ROM 仍有空間時應優先保留低風險 dense table，不急著引入 perfect
hash。只有實際 ROM 壓力出現才壓縮 191 active masks，而且 unknown
mask 必須能驗證並 fallback。

### 9.2 固定 single-hue banks

不能只 sweep「固定 N 個」，還要決定是哪 N 個：

- 先量測每個 exact bank 的 exposure、leave-one-out error 與 tail
  保護價值；
- 做 greedy forward/backward selection；
- 十一個候選的完整 subset 只有 `2^11=2048`，可在縮小 dataset 上做
  exhaustive screening，再對最佳 subsets 完整重訓；
- 比較 `0..11` fixed banks、free banks、mean/CVaR、ramp inversion、
  profile consistency 與視覺畫面。

## 10. 心理視覺模型定案

### 10.1 建議 metric ensemble

| 模型／規則 | 本專案角色 | 判斷 |
|---|---|---|
| OKLab squared Euclidean | Primary train loss | 適合 D65、正常觀看、image-processing 與 Lloyd；不是 CIE 標準，也不是所有 GBA 面板的真實 JND |
| Euclidean OKLab | Mean/P50/P95/P99/CVaR report | 與 train 同尺度，便於定位 tail；threshold 需由偏好測試校準 |
| CIEDE2000 | Secondary colour-difference report | CIE 標準、小色差資料充分；沒有空間結構且實作較複雜，不作第一版 centre update |
| CAM16-UCS | Viewing-condition sensitivity report | 可明示 adapting luminance、surround、white point；沒有可靠 GBA viewing conditions 前不作 ground truth |
| S-CIELAB | Optional spatial/dither report | 原論文針對 digital image reproduction/patterned regions；需固定 pixels-per-degree，不能作 primary loss |
| JzAzBz / ICtCp | 不採用 | 主要面向 HDR/PQ/廣色域，與 GBA SDR 目標不匹配 |
| MacAdam ellipses | 歷史／sanity reference | 不是可直接塞入本問題的 image metric |
| Ramp monotonicity/collapse | Hard report，之後可作 constraint | 非完整人眼模型，但非常符合 Tyrian palette 結構與 pixel-art 造型 |
| Edge sign/contrast preservation | Hard report/gate | 避免 spatial metric 以模糊或 dither「騙分」 |
| Hue category crossing | Report/gate | 工程 heuristic，需由畫面與偏好測試驗證 |

Oklab 原作者將其定位為 D65、正常良好照明下的 image-processing
perceptual space，並說明其衍生參考 CAM16、Pointer gamut 與 perceived
hue data；這支持把它當「實用 primary metric」，但不支持把固定
`0.02` 宣稱成所有刺激條件的普遍 JND。

### 10.2 Display transforms

第一個可重現 profile：

```text
PC VGA6 expanded RGB8 code values
GBA BGR555 bit-replicated RGB8 code values
    -> 同一套 standard sRGB inverse piecewise transfer
    -> linear sRGB D65
    -> OKLab
```

這代表「現代 emulator/screenshot parity」，不是原始 CRT 或 GBA LCD
的光譜 ground truth。

AGB-001、AGS-101、mGBA/VBA-M correction 應先作 sensitivity analysis：

- 每套 transform 必須有名稱、來源、矩陣／transfer 版本；
- source 與 target 的 transform 必須明確，不能只對 target 隨意套
  gamma；
- 沒有色度計／光譜量測前，不宣稱任何固定 gamma 是真實面板；
- 本地 VBA-M `filters_agb.cpp` 本身使用 2.2 target/display gamma，
  再搭配 profile matrix 與 darken control，已說明「AGB=單一 3.5」
  不是可直接採信的普遍模型。

## 11. 心理物理資料與標準

| 資料／來源 | 可用性與本專案用途 |
|---|---|
| CIEDE2000 / ISO/CIE 11664-6 | 正式色差公式；適合作 secondary report |
| CIE 230:2019 | 比較多種 small-colour-difference formula；包含開發 CIEDE2000 的 COM 與九個新資料集，官方頁面說明資料可下載 |
| Sharma/Wu/Dalal supplementary test data | 驗證 CIEDE2000 程式是否算對，不是 pixel-art 人類偏好 dataset |
| RIT-DuPont、BFD-P、Leeds、Witt、COM | 主要是 uniform/surface small-colour-difference 背景；可理解 metric 來源，不直接用來擬合 Tyrian tile objective |
| Munsell Renotation | RIT 提供資料；適合 hue/value/chroma ordering sanity check，不包含 pixel-art 空間結構 |
| Pointer's Gamut | 真實表面色 gamut；可理解 Oklab 衍生範圍，但不應限制人工遊戲色 |
| S-CIELAB papers | 支持 patterned digital reproduction 的 spatial report；需要觀看距離與解析度參數 |

參考來源：

- [Oklab 原始說明與衍生資料](https://bottosson.github.io/posts/oklab/)
- [ISO/CIE 11664-6:2022 CIEDE2000](https://www.cie.co.at/publications/colorimetry-part-6-ciede2000-colour-difference-formula-1)
- [CIE 230:2019 small-colour-difference datasets](https://www.cie.co.at/publications/validity-formulae-predicting-small-colour-differences)
- [CIEDE2000 implementation notes/test data](https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/)
- [CAM16/CAM16-UCS paper DOI](https://doi.org/10.1002/col.22131)
- [S-CIELAB original paper DOI](https://doi.org/10.1889/1.1985127)
- [RIT Munsell Renotation Data](https://www.rit.edu/science/munsell-color-science-lab-educational-resources)

## 12. 人工偏好測試

Metric 最後仍需由人類畫面偏好校準：

- 使用 `Reference + A + B` triplet；
- A/B 隨機左右，整數倍 nearest-neighbour，禁止 smoothing；
- 場景依 episode、shape bank、雲／水／岩石／暗部、平均與 worst-tail
  分層抽樣；
- participants、scenes、display profile 設 random effects；
- 演算法版本為 fixed effect；
- 可用 Bradley–Terry 或 mixed-effects logistic model；
- scene 與 participant 都留 holdout，避免反覆用同一批偏好調參；
- bootstrap 或 hierarchical interval 報告 effect；
- sequential stopping rule 必須在實驗前固定，並處理 repeated looks，
  不能看到暫時領先就停止；
- 勝率 interval 不跨 0.5 才叫有方向性證據；實用等價範圍例如
  `[0.45,0.55]` 只是起始 margin，需事前定義。

人工結果只能回饋 global hyperparameters（例如 `alpha`、
`lambda_tail`、ramp weight、profile P），不可為單一 holdout scene
建立人工 correction table。

## 13. 建議的離線實驗順序

1. 建立 62 關 exact cache-key/exposure dataset 與 round-trip gate。
2. 重現 v53 的 RGB objective，作 baseline。
3. 只換 OKLab metric，其他架構不變，隔離 metric 收益。
4. 實作 M：16×15 generalized Lloyd + tile-level mean/CVaR。
5. 做 CVaR `alpha`、tail weighting、ramp gate ablation。
6. 做 fixed single-hue bank subset ablation。
7. 計算 K 的離線品質上限、unique `N`、ROM representation 與估計
   lookup cost。
8. 只有 K 明顯勝過 M 才做 runtime prototype；否則停在 M。
9. K 太大而少數 masks 仍差時，才做 F ambiguous overrides。
10. 固定演算法後比較 P=5/8/12/16 profiles。
11. 輸出全部關卡 visual grids、error heatmaps 與 metric ensemble。
12. 最後做 holdout 人工偏好測試，再決定是否改產品 code。

## 14. Non-regression gates

任何未來 prototype 至少必須：

- source tile decoder/cache-key round-trip 100%；
- 所有 62 關、所有三層、所有 active keys 有 coverage；
- index 0 mapping 不變；
- 16 banks × 15 colours、無非法 BGR555；
- M/K/F unknown feature 有可驗證 fallback；
- overall mean OKLab 不高於 v53；
- 每 shape bank／episode mean 不退化；
- P95/P99/CVaR 有明確改善或至少不退化；
- ramp inversion 不增加；
- critical scene heatmap 人工檢查通過；
- 若導入 K/F，gameplay cache-miss cycles、missed VBlank、Maxmod 音訊
  telemetry 不退化；
- 實驗 seed、parameters、dataset hash、palette hash 全部寫入 report，
  可重現。

## 15. 尚未定案、必須實測的項目

- 真正 unique runtime keys 與各關曝光量；
- M 相對 v53 的實際視覺收益；
- K 相對 M 的品質上限與 lookup 成本；
- CVaR `alpha/lambda`；
- tail 用 tile exposure 或 non-zero pixel exposure；
- 哪些／多少 single-hue banks 應固定；
- profiles 最佳數量 P；
- OKLab、CIEDE2000、CAM16-UCS 與人類偏好的一致程度；
- AGB-001／AGS-101 真實 display transform；
- K/F 是否值得修改 runtime。

因此本輪正確結論是「方法與實驗設計已具體化」，不是「已證明某個新
palette 一定較好」。下一階段若獲准，應只先建立離線 dataset/prototype，
在有全 62 關數據後才決定產品改寫。
