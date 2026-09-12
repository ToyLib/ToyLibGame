# ToyKitでゲームを作るときの基本形（全体アーキテクチャ）

ToyKitでの「ゲームを作る」流れの全体像をまとめる。個々のクラスの使い方は
[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md)、各クラスの役割は
[ToyKit_KitPrefab_KitSignal_Reference.md](ToyKit_KitPrefab_KitSignal_Reference.md) を参照。

---

## 1. 全体の流れ

```
Scene を定義する
  └─ 4フェーズ: Environment / World / SpawnCharacters / UI
       └─ SpawnCharacters() で Agent<TPrefab> + IBehavior を組み立てる
            └─ Scene の Update()/ProcessInput() が毎フレーム転送する
                 └─ Behavior が Prefab の Interface 越しに実際に動かす

Scene 間の遷移
  └─ ProcessInput()/Update() の中で条件を見て RequestChange(次のScene)
```

**ゲームを作る作業のほとんどは、この形の上に「Behaviorを書き足す」「Scene遷移の条件を書き足す」ことになる。**
どちらもフレームワーク化はせず、素直な C++ コードとして書く（理由は各項目を参照）。

---

## 2. Scene: 4フェーズで定義する

`toy::kit::IScene` は `Init()` から呼ばれる4つのフェーズを持つ（非virtualな`InitScene()`が固定の順序で呼ぶ）。
必要なものだけ override すればよい。

```
IScene::InitScene()
  → DefineEnvironment()  … ポストエフェクト/時刻/BGM等、見た目・雰囲気
  → DefineWorld()        … 地形・StaticObject の配置など、動かない世界
  → SpawnCharacters()    … Player/NPC（Agent<TPrefab>）のスポーン
  → DefineUI()           … UI Actor
```

- `DefineWorld()` は「動かないもの」だけを扱う（地面・空・レンガ・家など）。
  StaticObject の配置は procedural に1体ずつではなく、`StaticObjectPlacement` の配列（データ）を
  組み立てて `MakeStaticObjects()` に渡す形にしてある（Scene構築のDesc化。設計方針16）。
- `SpawnCharacters()` は「動くもの」（Player/NPC）だけを扱う。ここで `MakeXxx(app, ...)` ファクトリを
  呼んで `Agent<TPrefab>` を組み立て、Scene のメンバ（`unique_ptr`/`vector<unique_ptr>`）に保持する。

この4フェーズは、3つの Scene（FieldScene/SnowScene/OutdoorScene）が同じ形を個別に書いていたものを
`IScene` 側へ昇格したもの（2人目・3人目の利用者が現れてから昇格、という一貫した方針）。

### まだDesc化していないもの

Creature/Humanoid（Noriko/Wolf/Shiro等）の「どこに何をスポーンするか」は、まだ `SpawnCharacters()`
内に直接 C++ で書いている。StaticObject と違い `target`（追跡/逃走対象への実行時ポインタ）を伴うため、
単純な Desc データにできない——名前解決の仕組み（例: 文字列IDでターゲットを指定し、Scene構築時に
実ポインタへ解決する）が必要で、意図的に後回しにしている。

### 「いつ・なぜスポーンするか」はフレームワーク化しない

`SpawnCharacters()` は「どうスポーンするか」（Descを組み立てて`MakeXxx`を呼ぶ）の置き場所を
決めているだけで、「時間経過で湧く」「主人公の状態次第で湧く」「シナリオ進行で湧く」といった
*条件* は各 Scene の `Update()` 等に普通の C++ コードとして書く。条件の種類はゲームによって
大きく変わるため、具体的な2件目の要件が出てから汎用化を検討する（`ChaseBehavior`/`FleeBehavior`と
同じ「2人目の利用者が現れてから昇格」の方針）。

---

## 3. Actor: Agent + IBehavior で動かす

Scene がスポーンした各キャラは `toy::kit::Agent<TPrefab>`（体=Prefab + 行動=IBehavior）。

- **体（Prefab）**: `Creature`/`Humanoid`/`Projectile`/`StaticObject` の4種から選ぶ。
  見た目・当たり判定・共通Interfaceだけを持ち、ゲーム的な意味は一切持たない。
- **行動（IBehavior）**: `OnStart`/`OnUpdate`/`OnInput`/`OnCollision` の4フック。
  既存の汎用Behavior（`ChaseBehavior`＝視界に入ったら追う、`FleeBehavior`＝視界に入ったら逃げる、
  `FollowBehavior`＝常について歩く）で足りるならDescの値だけ変えて使い回し、足りなければ
  ゲーム側 `.cpp` にローカルな専用Behaviorを書く（1体専用なら無名namespace、2体目が現れたら
  ToyKit側へ昇格）。

**Sceneが毎フレーム明示的にやること**（自動化されていない部分）:

```cpp
// ProcessInput() — 入力が必要なものだけ
mPlayer->ProcessInput(input);

// Update() — 毎フレーム
mPlayer->Update(deltaTime);
for (auto& monster : mMonsters) monster->Update(deltaTime);
```

`Agent`（延いては`Prefab`）は `toy::Actor` を継承しないため、エンジンの自動Update対象にはならない。
Sceneが保持しているAgentの`Update()`/`ProcessInput()`を回してあげる、という1行がキャラごとに要る。

---

## 4. Scene 間の遷移

Scene遷移は `IScene::RequestChange(std::unique_ptr<IScene>)` を、各Sceneの `ProcessInput()`/`Update()`
の中で条件を見て呼ぶ、という素直なC++コードのまま扱う（例: `FieldScene`でStartボタンを押したら
`SnowScene`へ、`StoryScene`でメッセージを閉じたら`FieldScene`へ）。汎用的な遷移テーブル/ステート
マシンは用意していない。ゆくゆくスクリプト化する可能性はあるが、今のところは「C++で書く」を
基本形としている。

---

## 5. まとめ: 「ゲームを作る」とは

上記の基本形（Scene 4フェーズ + Agent/IBehavior + 手書きの遷移条件）が土台としてできあがっている。
ここから先、具体的なゲームを作る作業は主に:

1. 新しいキャラが要れば、既存Behaviorの使い回し or 新規Behaviorを書く（[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) のチェックリスト参照）
2. 新しいSceneが要れば、4フェーズを埋める
3. Scene遷移・スポーン条件をゲームの仕様に合わせて書き足す

の3つを積み重ねていくことになる。土台側（Prefab/Agent/IBehavior/ISceneの4フェーズ）に新しい
概念を足すのは、既存の型で表現できない・2件目以降の本当の利用者が出てきたときだけでよい。
