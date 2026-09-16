# Toby を題材にした Prefab / IBehavior キャラ作り実践マニュアル

パターン別のカタログは [ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) にまとまっている。
本ドキュメントはそれを前提に、実際にリポジトリに存在する **`Toby`（`ToyGame/KitGame/Actors/Toby.h/.cpp`）
1体だけ**を題材にして、Prefab と IBehavior がどう組み合わさって1体のキャラになっているかを
最初から最後まで通しで読めるようにしたもの。「自分でキャラを1体作る」実習を想定した構成。

対象読者: ToyKit を初めて触る／Prefab と IBehavior の役割分担がまだ曖昧な人。

---

## 1. Toby は何でできているか（全体図）

```
Toby  ( = toy::kit::Agent<toy::kit::Humanoid> の薄いサブクラス )
  ├─ Prefab   : toy::kit::Humanoid           … 「体」。見た目・移動・コライダー・カメラを持つ
  └─ IBehavior: TobyControlBehavior（無名namespace内） … 「行動」。入力→アニメーション選択とカメラ切換え判断
```

[Toby.h:14-18](../../ToyGame/KitGame/Actors/Toby.h)
```cpp
class Toby : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};
```

`Toby` クラス自体は**何も実装していない**。これが ToyKit の基本方針で、
「Toby専用の特別なクラス」を書くのではなく、汎用の `Agent<Humanoid>` に
「Humanoid の Desc」と「専用の IBehavior」を差し込むだけでキャラを表現する。

覚えておくべき役割分担はこの1行だけ:

> **Prefab（Humanoid）が「動く・当たる・カメラを持つ」を担当し、
> IBehavior（TobyControlBehavior）が「入力をどう解釈するか」だけを担当する。**

移動そのもの（スティック入力→実際の位置更新）やロックオンの選択/解除、Free⇔Locked切換えは
**Humanoid が自分で** `toy::InputState` を見て処理する。Toby の IBehavior は移動を一切行わない
（[Toby.h:9-12](../../ToyGame/KitGame/Actors/Toby.h) のコメント参照）。

---

## 2. 「体」を作る：`HumanoidDesc`

Prefab の実体を作るには、まず「体の設計図」である `Desc` 構造体を組み立てる。
`Desc` はプリミティブ型だけの構築情報で、実行時の参照やロジックは持たない
（JSON化を見据えた設計方針。詳細は [ToyKit_Design_Guidelines_Revise.md](ToyKit_Design_Guidelines_Revise.md)）。

Toby の `Desc` を作る関数が `MakeTobyDesc()`（[Toby.cpp:5-42](../../ToyGame/KitGame/Actors/Toby.cpp)）。
フィールドを役割ごとに読み解く:

### 見た目
```cpp
desc.model         = "Hero/hero_f.gltf";
desc.scale          = 0.1f;
desc.yawOffsetDeg    = 180.0f;   // モデルの正面とGetForward()の向きを揃える補正
desc.toonRender      = true;
desc.contourFactor   = 1.01f;
desc.contourColor    = Vector3(0.2f, 0.2f, 0.2f);
```

### 当たり判定（本体用・攻撃用の2種類）
```cpp
desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
desc.colliderScale  = Vector3(0.5f, 1.0f, 0.4f);
desc.colliderFlags  = toy::C_FOOT | toy::C_BODY | toy::C_PLAYER_TEAM;

desc.enableAttackCollider = true;                    // 攻撃用コライダーを別に生成する
desc.attackColliderOffset = Vector3(0.0f, 0.0f, 1.0f); // 本体の少し前方
desc.attackColliderScale  = Vector3(0.6f, 1.0f, 0.6f);
desc.attackDamage         = 10;
```
攻撃用コライダーは常時は無効で、攻撃モーション中だけ IBehavior 側が
`SetAttackColliderActive(true)` で有効化する（4章で後述）。

### プレイヤー操作専用の機能を有効化するフラグ
```cpp
desc.enableLockOnCombat = true;
```
**これが Toby と NPC を分ける唯一のスイッチ。** true にすると Humanoid 内部で
`SetupMove()` / `SetupCamera()` / `SetupCombatSensor()` が呼ばれ、
Free/Locked切換え・追従カメラ・ロックオン用の索敵センサーが生成される
（NPCは基本 false のままで、代わりに `enableVision` 系のセンサーを別途使う。詳細は
[ToyKit_KitPrefab_KitSignal_Reference.md](ToyKit_KitPrefab_KitSignal_Reference.md)）。

```cpp
desc.sensorFovDeg           = 60.0f;   // ロックオン対象を探す視野角
desc.sensorMaxDist          = 40.0f;
desc.sensorNearOverrideDist = 20.0f;

desc.lockBreakDist    = 35.0f;         // これ以上離れたら自動でロック解除
desc.lockLostGraceSec = 0.7f;          // ロックオン対象を見失ってからの猶予秒数
```

### その他
```cpp
desc.enableGroundPose   = false;  // 接地ポーズはHumanoid側で自動制御せず、アニメは自前で制御する
desc.footstepSound      = "Hero/Walk.wav";
desc.freezeCameraYInAir = true;   // 空中でカメラの高さが揺れないようにする
```

**このセクションのポイント:** `Desc` を読むだけで「このキャラは何ができるか」が一覧できる。
新しいキャラを作るときも、まずは既存キャラ（Toby/Wolf/Ninja等）の `MakeXxxDesc()` を並べて見比べ、
どのフラグを立てる/立てないかを決めるところから始めるとよい。

---

## 3. 「行動」を作る：`IBehavior`

`IBehavior` は4つのフックだけを持つ最小のインターフェース（[IBehavior.h:24-40](../../ToyKit/include/KitBehavior/IBehavior.h)）。
Toby の場合は全フックのうち **`OnStart` と `OnInput` の2つだけ**を使う（`OnUpdate`/`OnCollision` は使わない）。

| フック | 呼ばれるタイミング | Tobyでの使い道 |
|---|---|---|
| `OnStart(Prefab&)` | Agent生成直後に1回 | カメラ切換えのSignal購読を仕込む |
| `OnUpdate(Prefab&, dt)` | 毎フレーム | **使わない**（移動はHumanoidが自動処理するため不要） |
| `OnInput(Prefab&, InputState&)` | 入力がある時 | ボタン入力→アニメーション選択・ロックオン操作・攻撃 |
| `OnCollision(Prefab&, CollisionEvent&)` | 衝突検知時 | **使わない**（攻撃判定はHumanoid内部の共通コードが処理） |

### 3-1. `OnStart`：カメラ切換えの「判断」を仕込む

[Toby.cpp:66-83](../../ToyGame/KitGame/Actors/Toby.cpp)
```cpp
void OnStart(toy::kit::Prefab& body) override
{
    mBody = static_cast<toy::kit::Humanoid*>(&body); // Humanoid固有メソッドを使うのでstatic_cast

    mBody->OnPlayModeChanged().Connect(
        [this](const toy::kit::PlayModeEvent& e)
        {
            if (e.locked /* && 切り替えたい条件 */)
            {
                //mApp->GetCameraManager()->SetActiveCamera(mBody->GetFollowCamera());
            }
            else if (!e.locked)
            {
                mApp->GetCameraManager()->SetActiveCamera(mBody->GetOrbitCamera());
            }
        });
}
```

ここで重要な設計上の分離が起きている:

- **Humanoid は「ロックした/外れた」という事実だけを `PlayModeEvent{ bool locked }` として通知する。**
  どのカメラに切り替えるべきかは Humanoid は一切知らない（Toby/NPC共通のクラスなので、
  カメラの知識を持たせすぎないため）。
- **「どちらのカメラを有効化するか」を決めて実際に `CameraManager::SetActiveCamera()` を呼ぶのは
  IBehavior側（Game Logic側）の責務。**

> **現状の注意:** ロックオン時に FollowCamera へ切り替える行は**コメントアウトされたまま**
> （「切り替えたい条件」がまだ決まっていない、検討中の状態）。ロック解除時に OrbitCamera へ戻す
> 処理だけが有効。このセクションを触るときは、まずこの条件が何であるべきかをチームで確認すること。

同じパターン（Signal購読でカメラを切換える）は `Hero`（[Hero.cpp:40-51](../../ToyGame/RPG/Actors/Hero.cpp)）
でも使われている。Hero版はロック時/解除時の両方が有効化されているので、実装の参考にするなら
Hero側を見るとよい。

### 3-2. `OnInput`：ボタン入力をアニメーション・アクションに変換する

[Toby.cpp:85-102](../../ToyGame/KitGame/Actors/Toby.cpp)
```cpp
void OnInput(toy::kit::Prefab& /*body*/, const toy::InputState& state) override
{
    const int moveMotion = mBody->IsTargetLocked() ? H_WalkSS : H_Run;
    UpdateMovementAnimation(state, moveMotion);

    SelectTarget(state);

    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        mBody->ReleaseTarget();
    }
}
```

ここで呼んでいる `mBody->IsTargetLocked()` / `SelectPrevTarget()` / `SelectNextTarget()` /
`ReleaseTarget()` はすべて **Humanoid が既に持っている機能**（[Humanoid.h:62-68](../../ToyKit/include/KitPrefab/Humanoid.h)）。
IBehavior はこれらを「いつ呼ぶか」（＝どのボタンに割り当てるか）を決めるだけで、
ロックオン選択のアルゴリズム自体は書かない。

入力割り当ての一覧（[Toby.cpp](../../ToyGame/KitGame/Actors/Toby.cpp) 全体から抽出）:

| 入力 | 処理 |
|---|---|
| スティック移動 | Humanoid内部のMoveComponentが自動処理（IBehaviorは関与しない） |
| `L1` / `R1` | `SelectPrevTarget()` / `SelectNextTarget()`（ロックオン対象の切換え） |
| `B`（`L2`を押していない時） | `ReleaseTarget()`（ロックオン解除） |
| `A` | `Jump()` + ジャンプアニメーション |
| `L2` を押しながら `B`/`X`/`Y` | 近接攻撃3種（Slash/Spin/Stab） |

### 3-3. 攻撃処理の内訳

[Toby.cpp:135-160](../../ToyGame/KitGame/Actors/Toby.cpp)
```cpp
void InputAttack(const toy::InputState& state)
{
    if (!mBody->IsTargetLocked()) return;  // ロックオン中でなければ攻撃不可
    if (!mBody->IsMovable())      return;  // 既に攻撃/被弾等でロック中なら不可

    mBody->SetAnimPlayRate(1.5f);

    if (state.IsButtonPressed(toy::GameButton::B))
    {
        mBody->PlayAnimationOnce(H_Slash, H_Stand);
        mBody->SetMovable(false);            // アニメ再生中は移動を止める
        mBody->SetAttackColliderActive(true); // 攻撃用コライダーをこの間だけ有効化
    }
    // X→Spin, Y→Stab も同じ形
}
```

3つの技はすべて **「ロック中フラグを立てる→アニメーション再生→攻撃コライダー有効化」**という
同じ3手順の繰り返し。技を増やす場合もこの形をコピーすればよい。

攻撃コライダーを無効化するタイミング（＝いつ動けるようになるか）は IBehavior 側では**明示的に書かない**。
`Humanoid::UpdateMovableRecovery()`（[Humanoid.h:101](../../ToyKit/include/KitPrefab/Humanoid.h)、非公開）が
アニメーション終了を検知して自動的に `SetMovable(true)` に戻す。IBehavior は
`if (!mBody->IsMovable()) return;` で「今は動けない期間」を検出するだけでよい。

**衝突判定そのもの（コライダーが当たってダメージが入る処理）は IBehavior には一切出てこない。**
コライダー同士の衝突検知→ダメージ適用は `Humanoid::HandleCollision()`（Prefab側の共通コード）が
Toby/NPC問わず同じロジックで処理する。IBehaviorが担当するのは「いつ攻撃コライダーを開くか」だけ。

---

## 4. 組み立てる：Factory 関数

[Toby.cpp:202-208](../../ToyGame/KitGame/Actors/Toby.cpp)
```cpp
std::unique_ptr<Toby> MakeToby(toy::Application* app)
{
    auto toby = std::make_unique<Toby>(
        app,
        std::make_unique<TobyControlBehavior>(app),  // 行動
        MakeTobyDesc());                                // 体の設計図

    toby->GetBody().SetPosition(Vector3(0.0f, 30.0f, 0.0f));
    toby->GetBody().SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180.0f)));
    return toby;
}
```

`Agent<Humanoid>` のコンストラクタは `(app, behavior, desc)` の順で受け取り、内部で
`Humanoid` を構築してから `behavior->OnStart(body)` を呼ぶ（[Agent.h:20-40](../../ToyKit/include/KitBehavior/Agent.h)）。
つまり2〜3章で作った「体の設計図」と「行動オブジェクト」をここで初めて1つに束ねる。
初期位置・初期向きなど、そのキャラ固有の配置はこの Factory 関数の中で行う
（`Desc` には入れない。`Desc` は「型として持つ性質」、配置は「このインスタンス固有の値」という区別）。

---

## 5. Scene に登録する

Toby はどこかに置いておくだけでは動かない。Scene が毎フレーム明示的に
`ProcessInput()` / `Update()` を呼ぶ必要がある。

[FieldScene.h](../../ToyGame/KitGame/Scenes/FieldScene.h) 側でメンバとして保持:
```cpp
std::unique_ptr<class Toby> mToby;
```

[FieldScene.cpp:51](../../ToyGame/KitGame/Scenes/FieldScene.cpp)（`SpawnCharacters()` 内）:
```cpp
mToby = MakeToby(GetApp());
```

[FieldScene.cpp:98-122](../../ToyGame/KitGame/Scenes/FieldScene.cpp):
```cpp
void FieldScene::ProcessInput(const toy::InputState& input)
{
    if (mToby) mToby->ProcessInput(input);   // → TobyControlBehavior::OnInput
    ...
}

void FieldScene::Update(float deltaTime)
{
    ...
    if (mToby) mToby->Update(deltaTime);     // → TobyControlBehavior::OnUpdate（Tobyは空実装）
    ...
}
```

他の敵キャラ（Noriko/Ninja/Bunny）を生成する際に `&mToby->GetBody()` を渡している点にも注目
（[FieldScene.cpp:57,63,66,69,75,78](../../ToyGame/KitGame/Scenes/FieldScene.cpp)）。
これは「NPCのAI（ChaseBehavior/FleeBehavior/ChaseAttackBehavior）が追跡・逃走の対象として
Tobyの `Prefab&` を参照する」ための受け渡しで、Toby自身はこれを知らない
（Toby側からNPCへの参照は存在しない、一方向の依存）。

---

## 6. Toby を題材にした演習：自分でキャラを1体作ってみる

ここまでの構造を理解したら、同じ形をなぞって新しいキャラを作ってみるのが最短の習熟ルート。
以下は「Tobyとよく似た、操作可能な2人目のキャラ」を作る想定の手順
（`Toby方式`＝Behaviorを`.cpp`に隠す形。判断基準は
[ToyKit_Prefab_Manual.md 6-1/6-2](ToyKit_Prefab_Manual.md#6-パターンd-入力操作するキャラplayer--heroの例) 参照）。

1. **体を決める**: 二足歩行で見た目・移動・カメラを持つなら `Humanoid` を再利用する
   （新しい Prefab 型を作る必要はない。「体の形」自体が違う場合のみ新型を検討）。
2. **`XxxDesc` を作る**: `MakeTobyDesc()` をコピーして `MakeXxxDesc()` にリネームし、
   `model` / `colliderFlags` / `enableLockOnCombat = true` 等、そのキャラに合う値へ変更する。
3. **`IBehavior` を作る**: `TobyControlBehavior` をコピーして、
   - `OnStart` でカメラ切換えのSignal購読（そのままで大抵よい）
   - `OnInput` でボタン割り当てを決める（アニメーションIDだけ、そのキャラのモデルに合わせて変更）
   を書く。Scene側からこのBehaviorの中身に触る必要がなければ、
   `.cpp` の無名namespace内に置いたままでよい（触る必要が出たら Hero方式へ移行）。
4. **Factory関数を書く**: `MakeXxx(app)` で `Agent<Humanoid>` を組み立て、初期位置/向きを設定する。
5. **Scene に登録する**: `unique_ptr` メンバを追加し、`SpawnCharacters()`で生成、
   `ProcessInput()`/`Update()` で呼び出す。
6. **ビルドして警告ゼロを確認する。**

**確認ポイント（詰まりやすい箇所）:**
- `enableLockOnCombat = true` を忘れると `GetOrbitCamera()`/`GetFollowCamera()` が
  `nullptr` のまま返ってくる（[Humanoid.h:115-116](../../ToyKit/include/KitPrefab/Humanoid.h) 参照。
  生成条件は `enableLockOnCombat` 時のみ、[Humanoid.h:17-22](../../ToyKit/include/KitPrefab/Humanoid.h) のコメント）。
- 攻撃中に動けてしまう場合は `SetMovable(false)` を呼び忘れている。
- 移動アニメーションが再生されない場合は `IsMovable()` が false のまま返り続けていないか
  （`UpdateMovableRecovery()` に任せていて、自分で `SetMovable(true)` に戻す処理を書いていないか）を疑う。

---

## 7. NPCとの対比で理解を固める

Toby の理解を、同じ `Humanoid` を使う NPC（`ChaseAttackBehavior` 適用のNinja）と対比すると
「Prefab（体）は共有し、IBehavior（行動）だけが違う」という設計方針がより明確になる。

| 観点 | Toby（`TobyControlBehavior`） | NPC（`ChaseAttackBehavior`、[ChaseAttackBehavior.h](../../ToyGame/KitGame/Actors/ChaseAttackBehavior.h)） |
|---|---|---|
| 体 | `Humanoid`（`enableLockOnCombat=true`） | `Humanoid`（`enableLockOnCombat=false`、代わりに`enableVision`系センサー） |
| 攻撃の起点 | ボタン入力（`OnInput`） | センサーが対象を検知＋距離条件（`OnUpdate`内の状態機械） |
| 状態管理 | 状態機械なし。`IsMovable()`等をその場でチェック | `KitStateMachine<State>`でIdle/Chase/Attackを明示管理 |
| 攻撃の実行方法 | `PlayAnimationOnce`+`SetMovable(false)`+`SetAttackColliderActive(true)` | 同じ3点セット（**Prefab側のAPIは完全に共通**） |
| ダメージ判定 | `Humanoid::HandleCollision`（共通コード） | 同上（**Toby/NPCで分岐なし**） |

「攻撃コライダーを開いて閉じる」という Prefab 側のAPIはToby/NPCで完全に同一。
違うのは「いつ開くかの意思決定ロジック」だけであり、それこそが IBehavior に切り出す価値そのもの。

---

## 関連ドキュメント

- [ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) — パターン別カタログ（StaticObject/汎用Behavior/専用Behavior/入力操作の4パターンと判断基準）
- [ToyKit_KitPrefab_KitSignal_Reference.md](ToyKit_KitPrefab_KitSignal_Reference.md) — `Prefab`/`Humanoid`等のAPIリファレンス
- [ToyKit_Game_Architecture_Overview.md](ToyKit_Game_Architecture_Overview.md) — Sceneの4フェーズ等、全体アーキテクチャ
- [ToyKit_Combat_Attack_Draft_Notes.md](ToyKit_Combat_Attack_Draft_Notes.md) — 攻撃/被弾/カメラ分離の設計検討過程（ドラフトメモ）
- [ToyKit_Actor_Lifecycle_Rules.md](ToyKit_Actor_Lifecycle_Rules.md) — Actor/Prefabのライフサイクル規約
