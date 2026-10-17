# UI 利用ガイド（ToyLib）

ゲーム作者向けに、ToyLib の UI 部品（メッセージ / 選択肢 / 確認ダイアログ）の使い方と、
UI を開いている間の入力のルールをまとめる。画像アセットは不要で、ウィンドウの背景・枠線・カーソルは
すべてコードで生成したテクスチャで描かれる。

実体:
- UI 部品: [MessageBoxActor.h](../../ToyLib/include/UI/MessageBoxActor.h) /
  [ChoiceListActor.h](../../ToyLib/include/UI/ChoiceListActor.h) /
  [ConfirmDialogActor.h](../../ToyLib/include/UI/ConfirmDialogActor.h)
- 土台: [UIScreen.h](../../ToyLib/include/UI/UIScreen.h)（UI の基底 Actor）/
  [UIStack.h](../../ToyLib/include/UI/UIStack.h)（入力の配送）/
  [UILayout.h](../../ToyLib/include/UI/UILayout.h)（配置）/
  [UIFrame.h](../../ToyLib/include/UI/UIFrame.h)（背景・枠線）
- 使用例: [TitleScene.cpp](../../ToyGame/KitGame/Scenes/TitleScene.cpp)（選択肢）/
  [StoryScene.cpp](../../ToyGame/KitGame/Scenes/StoryScene.cpp)（選択肢付きメッセージ）/
  [FieldScene.cpp](../../ToyGame/KitGame/Scenes/FieldScene.cpp)・
  [SnowScene.cpp](../../ToyGame/KitGame/Scenes/SnowScene.cpp)（確認ダイアログ）

---

## 1. 部品一覧

| 部品 | 見た目 | 主な用途 |
|---|---|---|
| `MessageBoxActor` | 画面下の会話ウィンドウ。折り返し・改ページ・文字送り・▼ インジケータ付き | 会話、ストーリー、説明文 |
| `MessageBoxActor` + `ChoiceListActor` | メッセージを読み終えると右上に選択肢が出る | 会話の分岐 |
| `ChoiceListActor` | 縦並びの選択肢リスト | タイトルメニュー、コマンド選択 |
| `ConfirmDialogActor` | 画面中央の質問文 + 横並びの はい / いいえ | シーン移動・セーブ等の確認 |

どの部品も同じ流儀で使う:

1. Scene の `DefineUI()` で `Desc` を組み立てて `CreateActor()` し、ポインタをメンバに持つ
2. 必要になったら `Open(..., コールバック)` で開く
3. プレイヤーが決定・閉じると、**UI が閉じてから** コールバックが呼ばれる

コールバックの中でシーン切り替え（`RequestChange`）や、別の UI を開く・同じ UI を開き直すことをしてよい。

---

## 2. 基本ルール（最重要）

### UI を開いている間、シーンの `ProcessInput()` は呼ばれない

メッセージ・選択肢・確認ダイアログは **Modal**（入力を占有する）。開いている間は:

- Scene の `ProcessInput()` が呼ばれない（`GameFlow` が止める）
- 通常の Actor の `ActorInput()` / Component の `ProcessInput()` も呼ばれない

なので Scene 側で「メッセージが開いているかどうか」を調べて入力を無視する、といった処理は書かなくてよい。
プレイヤーキャラ（`mToby->ProcessInput()` のように Scene から呼んでいるもの）も自動的に止まる。

**UI を閉じた A ボタンが、同じフレームにシーン側にも届くことはない。** 「A でメッセージを閉じたら、
その A でシーンが進んでしまった」という問題は起きない。

例外:
- `Application::ProcessInput()`（`GameApp::ProcessInput` の F6 など、アプリ全体のキー）は呼ばれ続ける
- `Actor::SetAlwaysReceiveInput(true)` を付けた Actor は入力を受け取り続ける（デバッグ表示の
  `DebugOverlayActor` が F2〜F4 のために使っている）

### ゲームを止めるかどうかは `pauseGame` で選ぶ

既定では、UI が止めるのは「入力」だけで、Scene の `Update()` や Actor の更新は動き続ける
（モンスターは動き、時間も進む）。会話中も背景のキャラを動かしておきたい場合はこのままでよい。

UI を開いている間ゲームを止めたい場合は、各部品の `Desc.pauseGame = true` にする
（[FieldScene.cpp](../../ToyGame/KitGame/Scenes/FieldScene.cpp) の確認ダイアログが例）。

```cpp
toy::ConfirmDialogActor::Desc confirmDesc;
confirmDesc.pauseGame = true;   // 確認中はキャラや時間を止める
```

開いている間、止まるもの / 動き続けるもの:

| 止まる | 動き続ける |
|---|---|
| Scene の `Update()`（`mToby->Update()` など Scene から呼ぶ更新も含む） | UI 部品（文字送り・開閉アニメーション・▼） |
| 通常の Actor と、その Component の更新（アニメーション・移動など） | `Actor::SetUpdateWhilePaused(true)` を付けた Actor（`DebugOverlayActor` など） |
| 時刻（`TimeOfDaySystem`）と物理（当たり判定） | `Application::UpdateGame()` フック、シーン切り替えのフェード、サウンド |

- UI が閉じた（決定した）瞬間に再開する。閉じるアニメーション中はもう動いている。
- 天候（雨・雪のオーバーレイ、空）とポストエフェクトのアニメーションは実時間で動いているので、
  停止中も動き続ける（11章）。

### Actor は Scene の `CreateActor()` で作る

UI 部品も Actor なので、[ToyKit_Actor_Lifecycle_Rules.md](ToyKit_Actor_Lifecycle_Rules.md) の通り
Scene の `CreateActor()` で作り、`Actor*` だけを保持する。シーン終了時に自動で破棄される。

---

## 3. メッセージ（`MessageBoxActor`）

```cpp
// DefineUI() など
auto font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 20);

toy::MessageBoxActor::Desc d;
d.layout.anchor = toy::UIAnchor::Bottom;   // 画面下中央
d.layout.offset = Vector2(0, -20);         // 下端から 20px 上
d.layout.size   = Vector2(680, 180);
d.padding       = Vector2(18, 14);
d.font          = font;
mMsg = CreateActor<toy::MessageBoxActor>(d);

// 開く（閉じたときの処理は省略可）
mMsg->Open("こんにちは。\nここは ToyLib の村です。",
           [this]() { /* 閉じた後の処理 */ });
```

- 文章は枠の幅で自動的に折り返し、枠に入りきらない分は自動で改ページされる。`\n` で明示的に改行できる。
- 文字は1文字ずつ表示される（文字送り）。ページを出し切ると右下に ▼ が出て、上下に揺れる。
- **A**：文字送り中なら全文表示 → 次のページ → 最後のページなら閉じる
- **B**：その場で閉じる（`Desc.enableBToClose = false` で無効化できる）
- `IsOpen()` で開いているかを調べられる。

主な `Desc` 項目:

| 項目 | 内容 | 既定 |
|---|---|---|
| `layout` | 配置（7章） | 画面下中央、下端から 40px 上、640x160 |
| `padding` | 枠の内側の余白 | (16, 16) |
| `frame` | 背景・枠線（8章） | 角丸 8px、背景アルファ 0.5 |
| `transition` | 開閉アニメーション | 0.15 秒で下から浮き上がる |
| `textColor` / `lineGapPx` | 文字色・行間 | 白 / 2px |
| `showIndicator` / `indicatorText` | ▼ を出すか / その文字 | true / `"▼"` |
| `pauseGame` | 開いている間ゲームを止めるか（2章） | false |
| `font` | フォント | 下の注意を参照 |

> **注意:** `font` は必ず指定すること。未指定時の既定パス（`"Font/rounded-mplus-1c-bold.ttf"` を
> システムアセットから読む）は、実際のファイル配置（`ToyLib/Assets/` 直下）と合っておらず読めない。

---

## 4. 選択肢付きメッセージ（`OpenWithChoices`）

メッセージを読み終えたところで選択肢を出したい場合は、メッセージに選択肢リストを渡しておき、
`OpenWithChoices()` で開く（[StoryScene.cpp](../../ToyGame/KitGame/Scenes/StoryScene.cpp)）。

```cpp
// DefineUI() など：選択肢リストを作ってメッセージに渡す
toy::ChoiceListActor::Desc choiceDesc;
choiceDesc.font        = font;
choiceDesc.layout.size = Vector2(220.0f, 0.0f);   // 幅だけ指定（高さは項目数から自動）
choiceDesc.allowCancel = false;                   // 必ずどれかを選ばせる
mMsg->AttachChoiceList(CreateActor<toy::ChoiceListActor>(choiceDesc));

// 開く
mMsg->OpenWithChoices(story, { "次へ進む", "もう一度読む" },
                      [this](int index)
                      {
                          if (index == 0) RequestChange(std::make_unique<FieldScene>());
                          else            OpenStory();   // もう一度最初から
                      });
```

- 最後のページを出し切ると、メッセージの **右上（右端をそろえた位置）** に選択肢が自動で出る。
  選択肢の位置は自分で指定しなくてよい（`layout.size` の幅だけが使われる）。
- 選んでいる間もメッセージは表示されたまま。入力は選択肢リストが受け取る。
- 決定すると、メッセージと選択肢が閉じてから `onDecide(index)` が呼ばれる。
- 選択肢付きのときは、メッセージの B で閉じる機能は無効になる（選ばずに閉じられないように）。
- 選択肢リストで B（キャンセル）した場合はメッセージに戻り、A でもう一度選択肢を出せる。
  キャンセル自体を禁止するなら `allowCancel = false`。
- `AttachChoiceList()` をせずに `OpenWithChoices()` を呼ぶと、ログを出して普通のメッセージとして開く。

---

## 5. 選択肢リスト（`ChoiceListActor`）

メッセージと関係なく、単独のメニューとして使う場合（[TitleScene.cpp](../../ToyGame/KitGame/Scenes/TitleScene.cpp)）。

```cpp
// DefineWorld() / DefineUI() など
toy::ChoiceListActor::Desc menuDesc;
menuDesc.font          = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 22);
menuDesc.layout.anchor = toy::UIAnchor::Bottom;
menuDesc.layout.offset = Vector2(0.0f, -80.0f);
menuDesc.layout.size   = Vector2(240.0f, 0.0f);   // 高さ 0 = 項目数から自動
mMenu = CreateActor<toy::ChoiceListActor>(menuDesc);

// ProcessInput() など
mMenu->Open({ "次へ進む", "やめる" },
            [this](int index) { if (index == 0) RequestChange(std::make_unique<StoryScene>()); },
            nullptr,   // キャンセル時の処理（省略可）
            0);        // 最初に選んでおく項目（省略可）
```

- **上下**で選択、**A** で決定、**B** でキャンセル（`allowCancel = false` で無効）。
- `wrap = true`（既定）なら、端から反対側へ回り込む。
- 項目数は `Open()` のたびに変えてよい。

主な `Desc` 項目: `layout` / `padding` / `itemHeight`（1行の高さ、既定 36px）/ `frame` / `transition` /
`cursorColor`・`cursorAlpha`・`cursorRadius`（カーソル帯）/ `textColor`・`selectedTextColor` /
`font` / `allowCancel` / `wrap` / `pauseGame`。

---

## 6. 確認ダイアログ（`ConfirmDialogActor`）

「はい / いいえ」を聞くだけの画面中央のダイアログ（[FieldScene.cpp](../../ToyGame/KitGame/Scenes/FieldScene.cpp)）。

```cpp
// DefineUI()
toy::ConfirmDialogActor::Desc confirmDesc;
confirmDesc.font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 22);
mConfirm = CreateActor<toy::ConfirmDialogActor>(confirmDesc);

// ProcessInput()
if (input.IsButtonPressed(toy::GameButton::Start))
{
    mConfirm->Open("雪原へ移動しますか？",
                   [this](bool yes)
                   {
                       if (yes) RequestChange(std::make_unique<SnowScene>());
                   });
}
```

- **左右**（上下・A / D キーでも可）で はい / いいえ を切り替え、**A** で決定。
- **B** は「いいえ」として扱う（`allowCancel = false` で無効）。
- `Open()` の第3引数 `defaultYes`（既定 true）を `false` にすると、「いいえ」を選んだ状態で開く。
  セーブデータの削除など、うっかり決定されると困る確認では `false` にする。
- 質問文は枠の幅で折り返す。長い文章を入れる場合は `layout.size` を大きくする（既定 400x160）。
- ボタンの文言は `yesText` / `noText` で変えられる。
- `pauseGame = true` で、確認中はゲームを止められる（2章）。

---

## 7. 配置（`UILayout`）

UI の座標は **仮想解像度（1280x720）基準、左上原点、+Y が下** の論理座標。ウィンドウサイズが変わっても
自動でスケールされる。

配置は「画面のどこを基準に、どれだけずらすか」で指定する:

```cpp
toy::UILayout l;
l.anchor = toy::UIAnchor::Bottom;   // 画面の下中央を基準に
l.offset = Vector2(0, -20);         // そこから 20px 上
l.size   = Vector2(680, 180);       // 大きさ
```

- `anchor`：`TopLeft` / `Top` / `TopRight` / `Left` / `Center` / `Right` / `BottomLeft` / `Bottom` / `BottomRight`
- `pivot`：自分のどこを基準点に合わせるか。省略すると `anchor` と同じ
  （`Bottom` なら自分の下中央を、画面の下中央に合わせる）
- `offset`：+X 右 / +Y 下
- `stretch` + `stretchMargin`：横（または縦）を画面いっぱいに広げ、両端に余白をとる

```cpp
// 画面幅いっぱい（左右 40px あけて）、下端から 20px 上、高さ 180px
l.anchor        = toy::UIAnchor::Bottom;
l.offset        = Vector2(0, -20);
l.size          = Vector2(0, 180);
l.stretch       = toy::UIStretch::Horizontal;
l.stretchMargin = Vector2(40, 0);
```

右端・下端に付けたものは、アスペクト比の違う画面でも実際の画面の端に付く。

---

## 8. 見た目（背景・枠線・開閉アニメーション）

### 背景と枠線（`UIFrameStyle`）

各部品の `Desc.frame` で変える。テクスチャはコードで生成されるので、画像を用意する必要はない。

| 項目 | 内容 | 既定 |
|---|---|---|
| `bgColor` / `bgAlpha` | 背景の色・不透明度 | 濃紺 / 部品ごと（0.5〜0.85） |
| `radius` | 角の丸み（px）。0 で角丸なしの四角 | 8 |
| `lineWidth` | 枠線の太さ（px）。0 で枠線なし | 2 |
| `lineColor` / `lineAlpha` | 枠線の色・不透明度 | 薄いグレー / 0.9 |

```cpp
d.frame.bgAlpha   = 0.6f;
d.frame.radius    = 12;
d.frame.lineColor = Vector3(0.9f, 0.8f, 0.4f);   // 金色っぽい枠
```

### 開閉アニメーション（`UITransition`）

各部品の `Desc.transition` で変える。開くときは「フェード + 少し下から浮き上がる」、閉じるときはその逆再生。

| 項目 | 内容 | 既定 |
|---|---|---|
| `duration` | 秒数。0 以下でアニメーションなし | 0.15 |
| `ease` | 動きのカーブ（`toy::Ease::OutCubic` など。`OutBack` で少し行き過ぎて戻る） | `OutCubic` |
| `fadeFrom` | 開き始めの不透明度の倍率 | 0 |
| `slideFrom` | 開き始めのずれ（px） | (0, 16)。選択肢・確認ダイアログは (0, 8) |

閉じるアニメーション中はもう入力を手放している（ゲーム側に入力が戻っている）。見た目が消えきる前に
次の操作ができる。

---

## 9. 操作キー

キー割り当ては [InputConfig.json](../../ToyLib/Settings/InputConfig.json) の論理ボタンに従う。

| 論理ボタン | キーボード | 役割 |
|---|---|---|
| `A` | Z | 決定 / メッセージを進める |
| `B` | X | キャンセル / メッセージを閉じる / 確認ダイアログの「いいえ」 |
| `DPadUp` / `DPadDown` | ↑ / ↓（W / S でも可） | 選択肢の移動 |
| `DPadLeft` / `DPadRight` | ← / →（A / D キーでも可） | はい / いいえ の切り替え |
| `Start` | Enter | （UI 部品は使わない。シーン側で「メニューを開く」などに使う） |

---

## 10. 自分で UI 画面を作る場合

既存の部品で足りない画面は、`toy::UIScreen` を継承して作る。最低限の形:

```cpp
class MyPanel : public toy::UIScreen
{
public:
    MyPanel(toy::Application* app) : UIScreen(app)
    {
        mFrame.Create(this, 900, 901);   // 背景 + 枠線
        // ... 中の部品を CreateComponent で作る ...
        SetLayout(layout);                // 配置
        SetVisible(false);                // 初期は非表示（自分で用意する関数）
    }

    void Open()
    {
        SetVisible(true);
        PushToUIStack();   // 入力を受け取り始める（Modal）
        BeginShow();       // 開くアニメーション
    }

    void Close()
    {
        RemoveFromUIStack();   // 入力をすぐ手放す
        BeginHide();           // 閉じるアニメーション → 終わると OnHidden()
    }

protected:
    void OnLayoutChanged(const toy::UIRect& rect) override
    {
        mFrame.SetRect(GetLocalRect());
        // 中の部品は GetLocalRect()（左上が原点）を基準に SetOffset で並べる
    }
    void OnHidden() override { SetVisible(false); }

    void ActorInput(const toy::InputState& state) override
    {
        // スタックに積まれている間だけ呼ばれる
    }

private:
    toy::UIFrame mFrame;
};
```

- 入力を下に流したい画面（HUD など）は `SetModal(false)` にする。
- 開いている間ゲームを止めたい画面は `SetPausesGame(true)` にする。
- 一番上の画面になった / でなくなったときに `OnFocusGained()` / `OnFocusLost()` が呼ばれる。
- 角丸の四角が欲しいときは `NineSliceSpriteComponent` に
  `GetAssetManager()->GetRoundedRectTexture(半径)` を貼る。

---

## 11. 現状の制約

- **一時停止中も天候とポストエフェクトのアニメーションは動く**（実時間で動いているため。2章）。
- **UI 画面どうしの親子関係はない。** 画面の中の部品は `OnLayoutChanged()` で手で並べる。
- 選択肢付きメッセージの選択肢は、開いた時点の位置で固定される。表示中にウィンドウサイズが変わると
  メッセージに付いてこない（アスペクト比固定の現在の設定ではほぼ起きない）。
- `MessageBoxActor` のフォント既定値は使えない（3章の注意）。
