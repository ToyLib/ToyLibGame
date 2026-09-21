# ポストエフェクト利用ガイド（ToyLib）

ToyLib のポストエフェクトは、シーン全体を描いた1枚のカラーテクスチャ（`uSceneTex`）だけを入力に、
フルスクリーンクアッドで加工して画面に出す仕組み。深度バッファや法線バッファは見ていない、
純粋なスクリーンスペース処理。GL / VK 両バックエンドで同じ `PostEffectType` / `PostEffectDesc` を
共有しており、呼び出し側（ゲームコード）は描画バックエンドを意識しなくてよい。

実体:
- 型定義: [PostEffect.h](../../ToyLib/include/Render/PostEffect.h)
- シェーダー本体: [PostEffect.frag (GL)](../../ToyLib/Shaders/GL/PostEffect.frag) /
  [PostEffect.frag (VK)](../../ToyLib/Shaders/VK/src/PostEffect.frag)
- 描画側: [GLRenderer_DrawPass.cpp](../../ToyLib/src/Render/GL/GLRenderer_DrawPass.cpp) /
  [VKRenderer_PostEffect.cpp](../../ToyLib/src/Render/VK/VKRenderer_PostEffect.cpp)

---

## 1. 基本の使い方

Scene の `DefineEnvironment()` で `PostEffectDesc` を組み立てて `SetPostEffect()` に渡すのが基本パターン
（[FieldScene.cpp:21-28](../../ToyGame/KitGame/Scenes/FieldScene.cpp)）。

```cpp
void FieldScene::DefineEnvironment()
{
    toy::PostEffectDesc effectDesc;
    effectDesc.stage0.type      = toy::PostEffectType::OldFilm;
    effectDesc.stage0.intensity = 1.0f;
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);
    ...
}
```

`PostEffectDesc` は最大2段の `PostEffectStage`（`stage0` / `stage1`）を持つ。1段だけでよい場合は
`stage1` に触らなければ良い（デフォルトが `PostEffectType::None` なので自動的に無効化される）。

```cpp
struct PostEffectStage
{
    PostEffectType type { PostEffectType::None };
    float intensity     { 1.0f };   // 0..1
};

struct PostEffectDesc
{
    PostEffectStage stage0;
    PostEffectStage stage1;
};
```

エフェクトを外したい場合は `type = PostEffectType::None` の `PostEffectDesc` を渡す
（デフォルト構築でよい）。

### intensity の意味（全エフェクト共通ルール）

**`intensity = 0.0` で元画像と完全に同じ（エフェクトなし）、`1.0` で最大強度になるよう、
全エフェクトが統一的に設計されている。** 内部実装は「フル強度で計算した結果を、最後に元画像と
`intensity` でクロスフェードする」か、各項の係数に `intensity` を掛けて 0 のとき恒等になるように
作られているかのどちらか。どちらの方式でも呼び出し側からは同じ挙動に見えるので、
**`intensity` を毎フレーム 0→1 でアニメーションさせて「フェードイン」させる、といった使い方をして問題ない**
（[TitleScene.cpp](../../ToyGame/KitGame/Scenes/TitleScene.cpp) の `mIntensity` を減衰させる例を参照）。

---

## 2. 使えるエフェクト一覧（`PostEffectType`）

| type | 見た目 | 備考 |
|---|---|---|
| `None` | 何もしない | デフォルト |
| `Sepia` | セピア調 + わずかなコントラスト | |
| `CRT` | 樽型歪み・走査線・シャドウマスク・色収差・ノイズ | ブラウン管風 |
| `FeilyLand` | パステル調 + 空の霧 + ゆるい揺らぎ + キラキラ | おとぎ話風。空の上部ほど強くかかる |
| `Noisy` | 手続き型グレイン + 微細ノイズのみ | 色調はいじらない、純粋なノイズオーバーレイ |
| `Grayscale` | 通常のグレースケール | |
| `Monochrome` | グレースケール→二値化（`intensity` で段階的に遷移） | 0〜0.5でグレースケール化、0.5〜1で二値化が強まる |
| `Watercolor` | 輪郭の顔料溜まり(エッジ検出) + にじみ(UV揺らぎ+ブラー) + 色面のポスタライズ + 粒状感 | テクスチャ入力は使わず`uSceneTex`だけから計算する本格版 |
| `OldFilm` | セピア退色 + 明滅 + 縦スクラッチ + グレイン + ビネット | 低フレームレート風に時間を量子化してコマ落ち感を出す |

いずれもテクスチャアセット（紙のテクスチャなど）を必要としない、`uSceneTex` と手続き型ノイズ
（`hash12` / `paperNoise`）だけで完結する実装になっている。

---

## 3. 2段チェーン（stage0 + stage1）

`stage1` にも `None` 以外の type を設定すると、`stage0` の出力をそのまま `stage1` の入力として
2回目のポストエフェクトを重ねがけできる（例: 白黒フィルム風 = `OldFilm` → `Grayscale`）。

```cpp
toy::PostEffectDesc effectDesc;
effectDesc.stage0.type      = toy::PostEffectType::OldFilm;
effectDesc.stage0.intensity = 1.0f;
effectDesc.stage1.type      = toy::PostEffectType::Grayscale;
effectDesc.stage1.intensity = 1.0f;
GetApp()->GetRenderer()->SetPostEffect(effectDesc);
```

- `stage0` だけを使う場合は、従来通り1回のフルスクリーン描画で完結する（挙動・コストは変わらない）。
- `stage1` を使う場合のみ、内部で中間オフスクリーンターゲット（GL: `mPostMidRT` = `GLRenderTarget`、
  VK: `mPostMidRT` = `VKPostMidTarget`、いずれも深度なしのcolorのみ）を確保し、
  `stage0` → 中間RT → `stage1` → 画面 の2パス構成になる。中間RTは初回使用時に遅延生成され、
  画面サイズが変わったときだけ作り直される。

### 制約

- **チェーンできるのは2段まで。** それ以上必要になったら `PostEffectDesc` の段数拡張と、
  GL/VK 双方の `DrawPostEffectPass()` のループ・中間RT管理を見直す必要がある
  （現状は2段固定で設計しているため、単純に配列を伸ばすだけでは足りない箇所がある）。
- 各段は `uSceneTex` 相当の1枚のカラーテクスチャしか見られない。深度・法線を使うエフェクト
  （被写界深度など）を作りたい場合は、この仕組みの外で別途バッファを渡す設計が要る。

---

## 4. 新しいエフェクトを追加する手順

1. [PostEffect.h](../../ToyLib/include/Render/PostEffect.h) の `PostEffectType` に新しい値を追加する
   （既存の並び順を変えると `uPostType` の数値がズレるので、基本は末尾に追加する）。
2. [PostEffect.frag (GL)](../../ToyLib/Shaders/GL/PostEffect.frag) と
   [PostEffect.frag (VK)](../../ToyLib/Shaders/VK/src/PostEffect.frag) の両方に、
   同じ数値の `if (uPostType == N) { ...; return; }` 分岐を追加する（GL/VKでロジックがズレないように、
   基本は同じ処理を2重に書く）。
   - GL は `uv` だけ、VK は `fxUV`（非反転・手続き計算用）と `uv`（サンプリング用、`FlipY`補正込み）
     を使い分けている点に注意。
   - `intensity=0` で元画像と同じになるように実装する（2章のルール）。`Watercolor`/`OldFilm` のように
     「フル強度で計算してから `mix(orig, c, I)` で元画像とクロスフェード」する形が一番安全。
3. **GL/VKどちらのC++コードも、型ごとの分岐は不要**（`GLRenderer::DrawPostEffectPass()` も
   `VKRenderer::DrawPostEffectPass()` も、型を `(int)` / `static_cast<float>` でそのまま
   シェーダーに渡しているだけなので、シェーダー側に分岐を足せばそれで動く）。
4. ビルドして `ToyLib/Shaders/VK/spv/PostEffect.frag.spv` が再生成されることを確認する
   （CMake が `glslc` で自動コンパイルする）。

---

## 5. 実装メモ（内部を触る人向け）

- `PostEffectDesc` は `IRenderer::mPost` として GL/VK 共通で保持される
  （[IRenderer.h](../../ToyLib/include/Render/IRenderer.h)）。
- VK 側は `stage1` を使うときだけ、`"PostEffectMid"` という専用パイプライン
  （`"PostEffect"` と同じシェーダー・descriptor layout・push constantだが、深度アタッチメントを
  持たないcolor-onlyのrender pass向け）を遅延生成する。descriptor set は
  `frameIndex * kPostStageSlots + stageIndex` で per-frame × per-stage に分けて確保している
  （同じ set を1フレーム内で2回書き換えると、GPU実行時に両ステージが最後の書き込み内容を
  見てしまう問題を避けるため）。
- VK のみ、`stage0` が `mSceneRT` を直接サンプルするときだけ `flipY` 補正が必要
  （中間RTは補正済みの向きで焼いているので `stage1` 側は補正不要）。
