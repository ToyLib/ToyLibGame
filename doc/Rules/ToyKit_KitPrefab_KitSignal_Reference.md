# KitPrefab / KitSignal クラスリファレンス（ToyKit）

`ToyKit/include/KitPrefab/` と `ToyKit/include/KitSignal/` に置かれている各クラスの役割一覧。
使い方の手順は [ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) を参照。

---

## KitSignal — Prefab⇔Game Logic 間の通知の仕組み

Prefab は「起きた事実」だけを通知し、それをどう解釈するかは常に Game Logic 側が決める、という
設計方針を支える最小限の基盤。この2ファイルに実処理ロジックは一切なく、型と配線だけを提供する。

### `Signal<TEvent>`（Signal.h）

`std::function` ベースの最小限の Observer 実装。

```cpp
template<typename TEvent>
class Signal
{
    void Connect(Handler handler);        // ハンドラを登録
    void Emit(const TEvent& e) const;      // 登録済み全ハンドラを呼ぶ
};
```

- `Prefab::OnCollision()` / `OnGrounded()` のように、Prefab→Game Logic の通知に使われる。
- `HeroControlBehavior::OnCastMagic()` のように、Game Logic→Scene の通知にも同じ型がそのまま使われる
  （方向を問わない汎用ユーティリティ）。
- 複数箇所から `Connect` されうる想定（`mHandlers` は `vector`）。`Disconnect` は無い＝一度つないだら
  そのオブジェクトの生存期間中はつなぎっぱなしになる設計。

### `CollisionEvent` / `GroundedEvent`（Events.h）

Prefab が Emit する「事実」のペイロード構造体。

| 型 | フィールド | 意味 |
|---|---|---|
| `CollisionEvent` | `toy::ColliderComponent* other` | 何と接触したか（そのフラグを見て攻撃/障害物等を Game Logic 側が判断） |
| `GroundedEvent` | `bool grounded` | 接地状態が変化した（立ち上がり/立ち下がりの瞬間だけ発火） |

新しい Signal ペイロードが必要になったら、この2つと同じ形（意味づけを含まないプレーンな事実データ）で
同じ `KitSignal/Events.h` に足すか、あるいは `CastMagicEvent`（`Hero.h`）のように
ゲーム側ヘッダにローカルに置く（Scene連携用の一回性イベントは ToyKit に昇格させない）。

---

## KitPrefab — 体（Prefab）と行動（Behavior）

### 基盤クラス

#### `Prefab`（Prefab.h / Prefab.cpp）— 全 Prefab の基底

- 内部に `toy::Actor` を1つ持つが、**外からは一切見せない**。
  `detail::PrefabActorAdapter`（Prefab.cpp 内に閉じた非公開クラス）が実体の `toy::Actor` で、
  `UpdateActor()` から `Prefab::TickFromActor()` を呼び返すだけの薄いブリッジ。
  - `Prefab` 破棄時に `ClearOwner()` で逆参照を断ってから `DestroyActor()` する
    （Dead マーク後も同フレーム内でもう一度 `UpdateActor()` が呼ばれうるため、
    先に参照を切らないと解放済みメモリへアクセスする）。
- 提供する共通 Interface: `SetPosition/GetPosition`, `SetRotation/GetRotation`, `SetScale/GetScale`,
  `GetWorldTransform`, `GetForward`。
- 派生クラスが実装する純粋仮想 Interface: `SetVisible`, `SetCollisionEnabled`,
  `PlayAnimation(int)`, `PlayAnimationBlend(int, float)`。
- 提供する Signal: `OnCollision()`, `OnGrounded()`（`TickFromActor` の中で毎フレーム自動検出・Emit）。
- 公開の視界クエリ: `HasSensorHit()` — `SetupSensor()` 済みなら、`targetMask` に該当するコライダーが
  視野角/距離（+任意でLOS）内にあるかどうかを返す（無ければ常に false）。中身は Humanoid の
  ロックオン索敵と同じ `toy::SensorComponent`。Behavior 側からは `Prefab&` 越しにこれを呼ぶだけでよい。
- 派生クラスが使う protected ヘルパー:
  - `TrackCollider` / `TrackGravity` — 登録すると毎フレームの Signal 自動検出対象になる。
  - `SetupNameBoard` — 頭上の名前ビルボード（追従する別Actorを内部生成）。
  - `SetupTargetSprites` — ロックオン候補/ロック中の足元スプライト（`TrackCollider` 済み前提）。
  - `SetupAmbientSound` — 常時ループする3Dサウンド（唸り声等）。
  - `SetupSpeechText` — 本体に直接つく常時表示テキート（吹き出し等）。
  - `SetupSensor(fovDeg, maxDist, targetMask, requireLOS=false)` — `HasSensorHit()` を有効にする。
    Humanoid は元々ロックオン索敵用に自前で `toy::SensorComponent`（`Humanoid::SetupCombatSensor`、
    候補一覧・画面ソート等ロックオン固有のロジックを含む、Humanoid内で完結した実装のまま）を
    持っていたが、「センサーを持つ」という部分だけを2人目の利用者（Creature/Noriko）のために
    Prefab 基底へ昇格した。`Humanoid` は `enableLockOnCombat`（ロックオン専用センサー、Player用）と
    `enableVision`（この汎用センサー、NPC が Player 等を見つける用）の両方を独立に持てる——
    `CreatureDesc`/`HumanoidDesc` はどちらも `enableVision`/`visionFovDeg`/`visionMaxDist`/
    `visionTargetMask`/`visionRequireLOS` という同じ名前のフィールドでこれを設定する
    （`HumanoidDesc` 側は既存の `sensorFovDeg` 等＝ロックオン専用センサーの設定と名前が
    衝突しないよう、あえて `sensor` ではなく `vision` を接頭辞にしている）。
  - `OnUpdate(float)` — 派生 Prefab 固有の毎フレーム処理（仮想、既定は何もしない）。
- ゲーム的な意味（HP・AI等）は一切持たない。装飾ヘルパーは呼ばなければ生成コストゼロ。

#### `IBehavior`（IBehavior.h）— Prefab に後付けできる行動インターフェース

```cpp
class IBehavior
{
    virtual void OnStart(Prefab& body);
    virtual void OnUpdate(Prefab& body, float deltaTime);
    virtual void OnInput(Prefab& body, const toy::InputState& state);
    virtual void OnCollision(Prefab& body, const CollisionEvent& event);
};
```

- 全フック既定は空実装。使うものだけ override すればよい。
- Prefab の型を新設せずに、同じ体（Humanoid等）を使う複数キャラの「行動だけ」を差し替えるための仕組み。
- 1体しかいない・複雑な一回性の処理は無理に `IBehavior` 化せず専用 Game Logic クラスでもよい
  （`Hero`/`Player` はこの形で `IBehavior` を使っている＝両立する）。

#### `Agent<TPrefab>`（Agent.h）— Prefab + IBehavior を束ねる汎用ラッパー

```cpp
template<class TPrefab>
class Agent
{
    Agent(Application* app, unique_ptr<IBehavior> behavior, Args&&... args); // TPrefab(app, args...) を構築
    void Update(float deltaTime);              // → mBehavior->OnUpdate
    void ProcessInput(const InputState& state); // → mBehavior->OnInput
    TPrefab& GetBody();
    IBehavior* GetBehavior();                   // Signal購読等で具体的なBehavior型に触りたい場合のみ使う
};
```

- コンストラクタで `mBody.OnCollision()` を自動的に `mBehavior->OnCollision` へ配線し、`OnStart` を1回呼ぶ。
- `Wolf`/`Shiro`/`Noriko`/`Player`/`Hero` は全てこの `Agent<T>` の薄いサブクラス（`using Agent::Agent;` のみ）。
- `GetBehavior()` は普段は不要。`Hero` のように Behavior の Signal を外部（Scene）から購読する必要があるときだけ、
  派生クラス側に `GetControlBehavior()` のような型付きアクセサを追加して使う。

### 具体 Prefab（体の種類）

| クラス | Desc | 特徴 |
|---|---|---|
| `Creature`（Creature.h/.cpp） | `CreatureDesc` | 4足・簡易骨格アニメ動体。`SkeletalMeshComponent`+`ColliderComponent`+重力。`PlayAnimation`/`PlayAnimationBlend` 実装あり。 |
| `Humanoid`（Humanoid.h/.cpp） | `HumanoidDesc` | 二足歩行キャラ。Player/NPC を型で分けない共通クラス。`enableLockOnCombat` で以下を丸ごとON/OFF：Field/Battle切換え、追従カメラ、索敵（`SensorComponent`）、ロックオン選択/解除、足音自動再生。ジャンプ・移動判定・`PlayAnimationOnce`（1回再生して指定クリップへ戻る）・`SetMovable`（攻撃中などの移動ロック）を提供。 |
| `Projectile`（Projectile.h/.cpp） | `ProjectileDesc` | パーティクル+ポイントライトのみの一時エフェクト。コライダー・アニメーションは持たない（no-op実装）。`IsExpired()`で寿命切れを外部に伝えるだけで、破棄判断はGame Logic/Scene側。 |
| `StaticObject`（StaticObject.h/.cpp） | `StaticObjectDesc` | メッシュ+コライダー（+任意で重力）のみ。行動ロジックなし。建物・岩・家具・障害物用。 |

各 Desc（`CreatureDesc`/`HumanoidDesc`/`ProjectileDesc`/`StaticObjectDesc`）はプリミティブ型のみで構成される
「構築情報」構造体（JSON化を見据えた形、実行時参照は持たない）。

#### `StaticObjectPlacement`（StaticObjectPlacement.h/.cpp）— Scene構築のDesc化の単位

`StaticObjectDesc` + `position`/`rotation` をまとめただけの、これもプリミティブのみの構造体。
Scene 側は「procedural に1体ずつ`new`する」代わりに、この構造体を要素とする
`std::vector<StaticObjectPlacement>`（データ）を組み立てて、
`MakeStaticObjects(app, placements)` に渡すだけで良い（1件だけなら `MakeStaticObject`）。
`FieldScene`/`OutdoorScene` のレンガ・島・家の配置がこの形になっている
（[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) 参照）。Creature/Humanoid の配置はまだこの形に
なっていない——`target`（追跡/逃走対象への実行時ポインタ）を伴うため、名前解決の仕組みを
先に設計する必要があり、意図的に後回しにしている。

### 移動ユーティリティ

#### `MovementUtil`（MovementUtil.h/.cpp）— Behavior が共有する移動の数式

`IBehavior` ではなく、`Prefab&` を受け取るだけの素の free function 群。Walk/Run は速度（+アニメ）の
パラメータ違い、Random/ToTarget は「向かう先の点」の決め方の違いでしかないという整理のもとに、
`ChaseBehavior`/`FollowBehavior`（Stan）/`IdleWalkBehavior`（Noriko）で3重に重複していた移動の数式を
1箇所へ集約したもの。壁避け等は行わない（呼び出し側の Behavior が必要なら判断する）。

| 関数 | 用途 |
|---|---|
| `PickRandomDirectionXZ()` | ランダムな単位方向ベクトルを選ぶ（Random Walk/Run 用） |
| `DirectionAwayFromPointXZ(body, point)` | 対象点と反対方向への単位ベクトルを求める（Flee 用） |
| `FaceDirectionXZ(body, dir)` | 指定方向を向く |
| `MoveInDirectionXZ(body, dir, speed, dt)` | 指定方向へ一定速度で前進する（Yは重力任せ） |
| `GetDistanceXZ(body, point)` | 対象点とのXZ距離 |
| `FaceTowardPointXZ(body, point)` | 対象点の方を向く（ToTarget 用） |
| `MoveTowardPointXZ(body, point, speed, dt, stopRange)` | 対象点へ接近する。`stopRange`以内なら止まる |

**モデルの前方向の規約:** キャラクターモデルは Blender で正面を向けてモデリング・エクスポート
したものをそのまま使っている。この前方向は `toy::Actor::GetForward()`
（`Vector3::Transform(Vector3::UnitZ, mRotation)`、ローカル+Z）とは逆向き（ローカル-Z）になる。
`SensorComponent` の視界コーン（`q.forward = GetOwner()->GetForward()`）や `MovementUtil` の
`Face*`系関数はすべて「`GetForward()` = 見た目の正面」を前提に書かれているため、**新しい
Creature/Humanoid の Desc には必ず `yawOffsetDeg = 180.0f` を設定し、見た目をエンジン側の
前方向規約に合わせること**（`Noriko`/`Wolf`/`Shiro`/`Ninja` はすべてこの規約で統一済み）。

以前は `FaceDirectionXZ`（Flee系）と `FaceTowardPointXZ`（Chase系）とで回転角の符号を逆に
実装し、Behavior側で見た目のズレを吸収していた時期があったが、これは「センサーの向きだけ
GetForward()のまま＝見た目と食い違う」というバグの温床だった。現在は `yawOffsetDeg=180` を
モデル側の標準補正とし、`FaceDirectionXZ`/`FaceTowardPointXZ` はどちらも同じ規約
（引数の方向がそのまま `GetForward()` になる）で統一されている。**見た目の向きがおかしいときは
Behavior側の符号をいじって帳尻を合わせるのではなく、まず Desc の `yawOffsetDeg` を疑うこと。**

### 汎用 Behavior

#### `ChaseBehavior`（ChaseBehavior.h/.cpp） + `ChaseBehaviorDesc`

- 索敵→追跡の汎用AI。`IBehavior` 実装。内部の移動計算は `MovementUtil` に委譲している。
- `SetTarget(Prefab*)` でターゲットを設定（位置だけを `Prefab` Interface 越しに参照、型を問わない）。
- `KitStateMachine<State>`（`Idle`/`Chase`）で状態管理。
  - Idle→Chase: body の `HasSensorHit()`（視界。body 側で `enableVision` が必要）が true、かつ
    `stopRange` より離れている場合。`stopRange` のガードが無いと、Chase→Idle
    （下記）で近づきすぎて Idle に戻った直後、まだ視界に入ったままなので即座に
    Chase に戻ってしまい、Idle⇔Chase を毎フレーム往復する。
  - Chase→Idle: `stopRange` 以内に近づいた（＝とりあえず待機。Attack相当は未実装）か、
    `loseDistance * loseRangeMultiplier` より離れて見失った場合（こちらは既存のまま距離判定。
    Chase中はターゲットの方を向き続けるので視界で見失うことはほぼ無いが、あえて
    センサー化はしていない＝要求されていない変更はしない、という方針）。検知（Idle→Chase）は
    body側の視界センサーが担当するため、`loseDistance` は検知距離ではなく見失い判定専用
    （検知距離を変えたいときは `HumanoidDesc`/`CreatureDesc` の `visionMaxDist` を変える）。
- 状態遷移の入口でのみ `PlayAnimationBlend(idleAnim/chaseAnim, animBlendSec)` を呼ぶ。
- `Wolf`/`Shiro`/`Ninja`（Chase選択時）はこの1つの `ChaseBehavior` を Desc の値違いだけで
  共有している（挙動を変えるときは全員に影響する）。

`toy::kit::FleeBehavior`（元は Noriko 専用のローカル実装だったが、Ninja が2人目の利用者に
なった時点で ToyKit 側へ昇格。`HasSensorHit()` で検知・距離で見失う。ChaseBehaviorの「逆」）や
`FollowBehavior`（Stan、ToTarget のみ・索敵/見失いなし）も同じ `MovementUtil` を使っている
（[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) 参照）。Attack 相当の汎用 Behavior はまだ無い
（実際に攻撃してくる敵が出てから、その形を見て設計する）。

---

## 依存関係の向き

```
KitSignal（Signal / Events）
      ↑
KitPrefab/Prefab ← Creature / Humanoid / Projectile / StaticObject
      ↑
KitPrefab/IBehavior ← ChaseBehavior（ToyKit汎用） / ゲーム側ローカルBehavior（PlayerControlBehavior等）
      ↑
KitPrefab/Agent<TPrefab>  ← ゲーム側 Game Logic クラス（Wolf/Noriko/Player/Hero等）
```

`KitSignal` は他の何にも依存しない最下層。`Prefab` は `KitSignal` にのみ依存し `IBehavior`/`Agent` を知らない。
`IBehavior`/`Agent` は `Prefab` を介してのみ体を操作し、具体 Prefab 型（`Humanoid`等）には依存しない
（必要なら呼び出し側で `static_cast` する）。
