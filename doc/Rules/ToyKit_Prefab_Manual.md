# Prefab で NPC / プレイヤーを作る手順（ToyKit）

`toy::Actor` を直接継承するのではなく、`toy::kit::Prefab` 派生オブジェクトを
ゲーム側クラスが**メンバとして持つ**（コンポジション）のが ToyKit の基本方針。
関連: [ToyKit_Actor_Lifecycle_Rules.md](ToyKit_Actor_Lifecycle_Rules.md)

---

## 1. 全体像

```
Game Logic クラス（Wolf / Noriko / Player / Hero など）
  ├─ Prefab（Creature / Humanoid / Projectile / StaticObject）… 見た目・当たり判定・Interface
  └─ IBehavior（省略可）… 毎フレームの振る舞い・入力処理
```

- **Prefab** は「体」。位置・回転・見た目・コライダーの共通 Interface（`SetPosition`/`PlayAnimation`等）と
  Signal（`OnCollision`/`OnGrounded`）だけを提供し、ゲーム的な意味（HP・AI等）は一切持たない。
- **IBehavior** は「行動」。`OnStart`/`OnUpdate`/`OnInput`/`OnCollision` の4フックを持ち、
  `Prefab&` 越しに動く。Prefab を継承せずに差し替えできる。
- 多くの場合、`toy::kit::Agent<TPrefab>` という汎用ラッパーが両者を束ねる。
  **新しいキャラを追加するとき、まず「専用の Game Logic クラスを書かずに Agent + IBehavior だけで足りないか」を検討する。**

### どれを使うか迷ったら

| ケース | 選択肢 |
|---|---|
| 同じ体で「近づいたら追いかけてくる」等、既存の汎用 IBehavior で表現できる敵が複数いる | `Agent<TPrefab>` + 既存の `IBehavior`（例: `ChaseBehavior`） |
| 敵固有の行動だが、他の敵とは体だけ違って行動ロジックは同じ | `Agent<TPrefab>` + 新規の専用 `IBehavior`（複数体で使うなら ToyKit 側へ） |
| プレイヤー操作や Hero のような一回性の複雑な処理（Scene に Signal を出す等） | `Agent<TPrefab>` + ゲーム側専用の `IBehavior`（.cpp ローカル可） |
| メッシュ+コライダーを置くだけ（動かない） | `toy::kit::StaticObject` を直接使う（Behavior もラッパークラスも不要） |

---

## 2. Prefab の種類（既存4種、体の形で選ぶ）

| Prefab | 用途 | Desc |
|---|---|---|
| `Creature` | 4足・簡易メッシュ等、Humanoid ほど複雑でない動体 | `CreatureDesc` |
| `Humanoid` | 二足歩行キャラ。ロックオン戦闘・Field/Battle切換え・索敵はオプトイン | `HumanoidDesc` |
| `Projectile` | 魔法弾・回復エフェクト等、寿命付きの一発エフェクト | `ProjectileDesc` |
| `StaticObject` | 建物・岩・家具など、メッシュ+コライダーだけの動かない物 | `StaticObjectDesc` |

新しい Prefab 型を増やすのは「体の形」が本当に新しい場合だけ（例: 脚を持たない乗り物）。
操作方式の違い（シューティング/格闘等）は Prefab ではなく IBehavior 側で吸収する。

### Desc の主なフィールド

`XxxDesc` はプリミティブ型だけで構成され、JSON化を見据えた「構築情報」。実行時参照は持たない。

- `CreatureDesc` / `HumanoidDesc` 共通: `model`（メッシュパス）, `scale`, `colliderOffset/Scale/Flags`,
  `useGravity`, `displayName`（頭上の名前ビルボード, 空なら非表示）, `candidateTexture`/`lockedTexture`（ロックオン用足元スプライト）
- `HumanoidDesc` 固有: `gravityAccel`/`jumpSpeed`, `enableLockOnCombat`（**プレイヤー操作キャラだけ true**。
  NPCはfalseのままにして、AIは IBehavior 側で索敵する）, `footstepSound`, `ambientSound`（常時ループ音）, `speechText`（吹き出し）
- `StaticObjectDesc`: `model`, `actorScale`/`meshScale`, `colliderFromMeshComponent`（頂点直接 or MeshComponent経由）

---

## 3. パターンA: 静止物を置くだけ（StaticObject）

Behavior もラッパークラスも不要。**1体だけなら** `StaticObjectPlacement` を組み立てて
`toy::kit::MakeStaticObject()` に渡すだけ（`StaticObjectPlacement.h`）。

```cpp
// OutdoorScene.cpp
void OutdoorScene::DeployHouse(const Vector3& pos)
{
    toy::kit::StaticObjectPlacement placement;
    placement.desc.model         = "house.x";
    placement.desc.actorScale    = 0.003f;
    placement.desc.colliderFlags = toy::C_WALL | toy::C_GROUND | toy::C_FOOT;
    placement.position = pos;
    placement.rotation  = Quaternion(Vector3::UnitY, Math::ToRadians(150.0f));

    mStaticObjects.push_back(toy::kit::MakeStaticObject(GetApp(), placement));
}
```

**複数体まとめて置く場合**は、procedural に1体ずつ`DeployXxx(pos)`を呼ぶループにせず、
`StaticObjectPlacement` の配列（＝データ）を組み立ててから `MakeStaticObjects()` に一括で渡す
（Scene構築のDesc化。設計方針16）。こうしておくと、この配列を後からJSON化したときに
Scene側のコードを一切変えずに済む。

```cpp
// FieldScene.cpp
void FieldScene::DeployBricks()
{
    toy::kit::StaticObjectDesc brickDesc;
    brickDesc.model         = "Field/brick.glb";
    brickDesc.actorScale    = 4.0f;
    brickDesc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    std::vector<toy::kit::StaticObjectPlacement> placements;
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 5; ++j)
            placements.push_back({ brickDesc, Vector3(-80 + 15 * j / 2 + 10 * i * 1.5, 20, 20 + 5 * j * 1.5) });

    for (auto& obj : toy::kit::MakeStaticObjects(GetApp(), placements))
    {
        mStaticObjects.push_back(std::move(obj));
    }
}
```

Scene 側は `std::vector<std::unique_ptr<toy::kit::StaticObject>> mStaticObjects;` として保持するだけで、
`Update()` を呼ぶ必要もない（動かないので）。

**現状のスコープ:** Desc化しているのは StaticObject の配置だけ。Creature/Humanoid の配置
（Noriko/Wolf/Shiro等）は `target`（追跡/逃走対象への実行時ポインタ）を伴うため、Descだけの
プリミティブなデータにはまだできていない（名前解決の仕組みが必要、未着手）。

---

## 4. パターンB: 汎用 IBehavior で動く NPC（ChaseBehavior の例）

「近づいたら追いかけてくる」敵は、体（`HumanoidDesc`）と AI パラメータ（`ChaseBehaviorDesc`）を変えるだけで量産できる。

### 4-1. Game Logic クラスは薄いサブクラスにする

```cpp
// Wolf.h
class Wolf : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Wolf> MakeWolf(toy::Application* app, toy::kit::Prefab* target);
```

### 4-2. Desc を組み立てて Factory 関数を書く

```cpp
// Wolf.cpp
namespace {

toy::kit::HumanoidDesc MakeWolfDesc()
{
    toy::kit::HumanoidDesc desc;
    desc.model          = "Enemy/wolf.gltf";
    desc.yawOffsetDeg   = 180.0f; // モデルはBlenderで正面向きに作られているため、見た目をGetForward()に揃える補正（新規Descには常に設定する）
    desc.colliderFlags  = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                         | toy::C_HURTBOX | toy::C_ENEMY_TEAM;
    desc.ambientSound       = "growling.wav";
    desc.ambientSoundVolume = 0.2f;
    return desc;
}

toy::kit::ChaseBehaviorDesc MakeWolfChaseDesc()
{
    toy::kit::ChaseBehaviorDesc desc;
    desc.loseDistance = 30.0f;
    desc.moveSpeed   = 8.0f;
    desc.idleAnim    = 2;
    desc.chaseAnim   = 3;
    return desc;
}

} // namespace

std::unique_ptr<Wolf> MakeWolf(toy::Application* app, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<toy::kit::ChaseBehavior>(MakeWolfChaseDesc());
    behavior->SetTarget(target); // 追いかける相手（例: &hero->GetBody()）

    auto wolf = std::make_unique<Wolf>(app, std::move(behavior), MakeWolfDesc());
    wolf->GetBody().SetScale(3.0f);
    return wolf;
}
```

同じ形の敵をもう一種類増やしたくなったら、`Desc` の値だけ変えた `MakeXxxDesc()` を書けばよい
（`Shiro` が同じパターンでもう一つの実例）。

---

## 5. パターンC: 専用の IBehavior を書く（Noriko の例）

既存の汎用 Behavior で足りない場合は、ゲーム側 `.cpp` の無名namespace内にローカルな `IBehavior` を書く。
**他で使う予定がない限り ToyKit 側には昇格させない**（2人目の利用者が現れてから汎用化する）。

```cpp
// Noriko.h
class Noriko : public toy::kit::Agent<toy::kit::Creature>
{
public:
    using Agent::Agent;
};
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position);
```

```cpp
// Noriko.cpp
namespace {

toy::kit::CreatureDesc MakeNorikoDesc() { /* ... */ }

class IdleWalkBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = &body;
        mFSM.Register(State::Idle, [this](float) {
            if (mFSM.IsEnterFrame()) mBody->PlayAnimationBlend(0, kAnimBlendSec);
            if (mFSM.GetTimer() > 10.0f) mFSM.To(State::Walk);
        });
        mFSM.Register(State::Walk, [this](float dt) {
            if (mFSM.IsEnterFrame()) { mBody->PlayAnimationBlend(1, kAnimBlendSec); PickRandomDirection(); }
            MoveForward(dt);
            if (mFSM.GetTimer() > 5.0f) mFSM.To(State::Idle);
        });
        mFSM.Start(State::Idle);
    }

    void OnUpdate(toy::kit::Prefab& /*body*/, float deltaTime) override { mFSM.Update(deltaTime); }

private:
    enum class State { Idle, Walk };
    static constexpr float kAnimBlendSec = 0.2f;
    void PickRandomDirection() { /* ランダムXZ方向を決めて mBody->SetRotation */ }
    void MoveForward(float dt) { /* mBody->SetPosition(pos + dir * speed * dt) */ }

    toy::kit::Prefab* mBody = nullptr;
    toy::kit::KitStateMachine<State> mFSM;
    Vector3 mMoveDir = Vector3::UnitZ;
    float mMoveSpeed = 3.0f;
};

} // namespace

std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position)
{
    auto noriko = std::make_unique<Noriko>(app, std::make_unique<IdleWalkBehavior>(), MakeNorikoDesc());
    noriko->GetBody().SetPosition(position);
    return noriko;
}
```

ポイント:
- `IBehavior` は `Prefab&` だけを見るので、`Creature` 固有メソッドが必要なければ `static_cast` すら要らない。
- アニメーションのブレンドは **状態遷移の入口（`IsEnterFrame()`）でだけ**呼ぶ。毎フレーム`PlayAnimationBlend`を呼ぶと毎フレーム再ブレンドが走ってしまう。

---

## 6. パターンD: 入力操作するキャラ（Player / Hero の例）

プレイヤー操作は `IBehavior::OnInput(Prefab&, const InputState&)` を使う。
`Agent<TPrefab>::ProcessInput(state)` が `OnInput` に委譲する。

### 6-1. 外部に Signal を公開しない場合（Player）

Scene から Behavior の中身を触る必要がなければ、`.cpp` の無名namespaceに隠せる。

```cpp
// Player.h
class Player : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};
std::unique_ptr<Player> MakePlayer(toy::Application* app);
```

```cpp
// Player.cpp
namespace {
toy::kit::HumanoidDesc MakeHeroDesc() { /* ... enableLockOnCombat = true ... */ }

class PlayerControlBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = static_cast<toy::kit::Humanoid*>(&body); // Humanoid固有メソッドが必要なので static_cast
    }
    void OnInput(toy::kit::Prefab& /*body*/, const toy::InputState& state) override
    {
        SelectTarget(state);
        if (state.IsButtonDown(toy::GameButton::L2)) InputAttack(state);
        if (state.IsButtonPressed(toy::GameButton::B)) mBody->ReleaseTarget();
    }
    void OnUpdate(toy::kit::Prefab& /*body*/, float /*dt*/) override { UpdateMovementAnimation(); }
private:
    toy::kit::Humanoid* mBody = nullptr;
    // ...
};
} // namespace

std::unique_ptr<Player> MakePlayer(toy::Application* app)
{
    auto player = std::make_unique<Player>(app, std::make_unique<PlayerControlBehavior>(), MakeHeroDesc());
    player->GetBody().SetPosition(Vector3(0.0f, 30.0f, 0.0f));
    return player;
}
```

### 6-2. 外部から Signal を購読する必要がある場合（Hero）

Scene が「魔法を撃った」等のイベントを受け取って Projectile を生成する必要があるなら、
Behavior を**名前付きクラスとしてヘッダに公開**し、`Agent::GetBehavior()` 経由でアクセスする。

```cpp
// Hero.h
struct CastMagicEvent { Vector3 position; Vector3 forward; };

class HeroControlBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override;
    void OnInput(toy::kit::Prefab& body, const toy::InputState& state) override;
    void OnUpdate(toy::kit::Prefab& body, float deltaTime) override;

    toy::kit::Signal<CastMagicEvent>& OnCastMagic() { return mOnCastMagic; }
private:
    toy::kit::Humanoid* mBody = nullptr;
    toy::kit::Signal<CastMagicEvent> mOnCastMagic;
};

class Hero : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
    // Behavior固有のSignalを外から購読したいときだけ、こういうアクセサを足す
    HeroControlBehavior& GetControlBehavior()
    {
        return static_cast<HeroControlBehavior&>(*GetBehavior());
    }
};

std::unique_ptr<Hero> MakeHero(toy::Application* app);
```

```cpp
// Hero.cpp（抜粋）
void HeroControlBehavior::OnAttackInput(const toy::InputState& state)
{
    if (state.IsButtonPressed(toy::GameButton::Y))
    {
        mBody->PlayAnimationOnce(H_Stab, H_Stand);
        mOnCastMagic.Emit(CastMagicEvent{ mBody->GetPosition(), mBody->GetForward() });
    }
}

std::unique_ptr<Hero> MakeHero(toy::Application* app)
{
    return std::make_unique<Hero>(app, std::make_unique<HeroControlBehavior>(), MakeHeroDesc());
}
```

```cpp
// OutdoorScene.cpp（Scene側で購読）
mHero = MakeHero(GetApp());
mHero->GetControlBehavior().OnCastMagic().Connect(
    [this](const CastMagicEvent& e)
    {
        mMagicBolts.push_back(std::make_unique<MagicBolt>(GetApp(), e.position, e.forward));
    });
```

**Player と Hero、どちらの形にするかの判断基準はこれだけ**:
Behavior が Scene に何かを伝える必要があるか（Signal を外に出す必要があるか）どうか。
必要なければ `.cpp` に隠す（Player方式）、必要なら公開する（Hero方式）。

---

## 6.5. Scene の4フェーズ（IScene）

`toy::kit::IScene` は `Init()` から呼ばれる4つのフェーズを持つ（`IScene.h`/`IScene.cpp`）。
必要なものだけ override すればよい（既定は何もしない）。

```
IScene::InitScene()  ※非virtual・固定の呼び出し順
  → DefineEnvironment()  … ポストエフェクト/時刻/BGM等、見た目・雰囲気
  → DefineWorld()        … 地形・StaticObject の配置など、動かない世界
  → SpawnCharacters()    … Player/NPC（Agent<TPrefab>）のスポーン
  → DefineUI()           … UI Actor
```

以前は各 Scene が自分で `InitScene()` を override して4つを手で呼んでいたが、
3つの Scene（FieldScene/SnowScene/OutdoorScene）が同じ形をしていたので `IScene` 側に
昇格した（2人目・3人目の利用者が現れてから昇格、というこのプロジェクトの一貫した方針）。

「いつ・なぜスポーンするか」（時間経過・シナリオ進行・主人公の状態などの条件）は
汎用フレームワーク化せず、各 Scene 固有の C++ コードとして書く
（`SpawnCharacters()` は「どうスポーンするか」の置き場所を決めるだけ）。

---

## 7. Scene への組み込み

Scene（`IScene`）は Prefab/Agent を所有する Game Logic オブジェクトの寿命を自分で管理する。

```cpp
// FieldScene.h
std::unique_ptr<class Player> mPlayer;
std::vector<std::unique_ptr<class Noriko>> mMonsters;
std::vector<std::unique_ptr<toy::kit::StaticObject>> mStaticObjects;
```

```cpp
// FieldScene.cpp — SpawnCharacters()（地形/StaticObjectはDefineWorld()側）
mPlayer = MakePlayer(GetApp());
for (int i = 0; i < 10; ++i)
    mMonsters.push_back(MakeNoriko(GetApp(), pos));

// ProcessInput() — 入力が必要なものだけ
mPlayer->ProcessInput(input);

// Update() — 毎フレーム
mPlayer->Update(deltaTime);
for (auto& monster : mMonsters) monster->Update(deltaTime);
```

寿命が尽きるオブジェクト（`Projectile` 等）は `IsExpired()` を持たせ、
`std::erase_if(container, [](auto& o){ return o->IsExpired(); })` で毎フレーム掃除する。

### 前方宣言だけで `unique_ptr` メンバを持つ場合の注意

ヘッダで `class Player;` のように前方宣言し `std::unique_ptr<Player> mPlayer;` を持つ場合、
**コンストラクタ・デストラクタは必ず `.cpp` 側で out-of-line 定義する**
（ヘッダにインラインで `= default;` と書くと、他の翻訳単位が暗黙にデストラクタを実体化しようとして
「incomplete type」エラーになる）。

```cpp
// FieldScene.h
FieldScene();
~FieldScene() override; // 宣言のみ

// FieldScene.cpp（Player等の完全な型が見える場所）
FieldScene::FieldScene() {}
FieldScene::~FieldScene() = default;
```

---

## 8. チェックリスト（新しいキャラを1体追加するとき）

1. 体（Prefab型）はどれか？ 新しい形が必要か、既存4種で足りるか。
2. `XxxDesc` を作る関数（`MakeXxxDesc()`）を書く。JSON化前提なのでプリミティブのみ。
3. 行動は既存の `IBehavior`（`ChaseBehavior`等）で足りるか？ 足りなければ専用 `IBehavior` を書く。
   - 他のキャラでも使う可能性が高いなら ToyKit 側（`KitPrefab/`）へ。
   - 1体専用なら `.cpp` の無名namespace に留める。
4. Scene から Behavior の中身に触る必要があるか？
   - ない → `Agent<TPrefab>` の薄いサブクラス + `.cpp` ローカル Behavior（Player方式）
   - ある（Signal購読等） → Behavior を名前付きでヘッダ公開 + `GetBehavior()`/専用アクセサ（Hero方式）
5. `MakeXxx(app, ...)` ファクトリ関数を書き、初期位置等をここで設定する。
6. Scene 側に `unique_ptr`/`vector<unique_ptr>` メンバを追加し、`SpawnCharacters()` で生成、
   `ProcessInput()`/`Update()` で呼び出す。
7. ビルドして警告ゼロを確認する。
