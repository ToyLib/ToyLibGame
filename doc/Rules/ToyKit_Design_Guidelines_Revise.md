# ToyKit 設計方針

## 1. ToyLib と ToyKit の役割

``` text
Game
 │
 │ Scene宣言 / Game Logic
 ▼
ToyKit (TK)
 │
 │ 高水準・宣言的API
 ▼
ToyLib
 │
 │ Actor / Component / Physics
 │ Renderer / Animation / Audio / Input
 ▼
SDL / OpenGL / Vulkan ...
```

-   **ToyLib**: ゲームエンジンとしての低レベル機能を提供する。
-   **ToyKit**:
    ToyLibの機能を組み合わせ、ゲームを簡潔に構築するためのToolkit。
-   利用者は原則としてToyKitからゲームを作り、必要な場合だけToyLib
    Coreへ降りる。

## 2. 基本思想：「ゲームを作る ≒ Sceneを作る」

Sceneをゲーム構築の中心とし、Environment、Lighting、Camera、World、UI、Game
Logicとの接続を宣言的に記述する。

``` cpp
void VillageScene::Define()
{
    Environment()
        .Weather(Weather::Clear)
        .TimeOfDay(17.5f);

    mPlayer = Spawn(Humanoid(playerDesc)).At({ 0, 0, 0 });
    mCat = Spawn(Creature(catDesc)).At({ 5, 0, 2 });

    UI().Add<MessageBox>("message");
}
```

Sceneは「どう実装するか」ではなく、**この世界に何が存在するか**を書く場所とする。

## 3. ゲーム側では Actor を継承せず Prefab を内包する

``` cpp
class Player
{
private:
    toy::kit::Humanoid mBody;
    int mHP = 100;
};
```

``` text
Enemy                 ← Game
├─ HP / AI / Damage / Die()
└─ Creature            ← ToyKit
   └─ Actor             ← ToyLib
      ├─ Physics
      ├─ Animation
      ├─ Visual
      └─ Collider
```

ゲーム固有の意味とエンジン上の実体を分離する。

## 4. 基本 Prefab

  Prefab           用途
  ---------------- ----------------------------------
  `Humanoid`       人間型キャラクター
  `Creature`       動物・モンスター・異形
  `StaticObject`   建物・岩・家具・障害物
  `Projectile`     弾・魔法・飛翔物・一時エフェクト

Player/NPC/Enemyなどゲーム上の役割ではPrefabを分けず、Prefabの種類を増やしすぎない。

## 5. Prefab / Desc / Game Logic の分離

``` text
Prefab     = 何者であるか
Desc       = どう構築するか
Game Logic = どう振る舞うか
```

Descは基本的に生成時の構成情報とする。

``` cpp
CreatureDesc desc;
desc.model = "cat.glb";
desc.gravity = -80.0f;
desc.moveSpeed = 5.0f;
desc.colliders = {...};

auto cat = Spawn(Creature(desc));
```

ゲーム中の状態変更にはDescを使わない。

## 6. 実行中の変更は Prefab Interface

``` cpp
cat->Move(direction);
cat->Jump();
cat->PlayAnimation("WALK");
cat->SetPosition(position);
cat->SetVisible(false);
cat->SetCollisionEnabled(false);
```

``` text
Desc       → Prefab
Game       → Interface → Prefab
Game       ← Signal    ← Prefab
```

## 7. Prefab は「事実」を Signal として通知する

Prefabはゲームルールを判断せず、物理・アニメーション等で発生した事実を通知する。

``` cpp
struct CollisionEvent
{
    ObjectHandle self;
    ObjectHandle other;
    ColliderRole selfRole;
    ColliderRole otherRole;
    Vector3 point;
    Vector3 normal;
};
```

``` cpp
body.OnCollision.Connect(this, &Enemy::OnCollision);
```

設計原則は、

> **Prefabは事実を通知し、Game Logicが意味を与える。**

とする。

## 8. ゲーム上の意味は Game Logic に置く

HP、Attack、Damage、Experience、Quest、Enemy、Playerなどは原則としてPrefabに持たせない。

``` text
衝突を検知                  ← ToyKit
        ↓
「これは攻撃」と判断         ← Game Logic
        ↓
20ダメージ
        ↓
HP = 0
        ↓
死亡
        ↓
Prefabへ DEAD再生等を指示
```

同じCreatureをRPGの敵にも、戦闘のないゲームの猫にも利用できる。

## 9. Signal / Handler モデル

QtのSignal / Slotに近いモデルを採用する。

``` text
Prefab                         Game Logic

Collision ─────────────────→ OnCollision()
Grounded ──────────────────→ OnGrounded()
AnimationFinished ─────────→ OnAnimationFinished()
```

``` cpp
void Enemy::OnCollision(const CollisionEvent& e)
{
    if (!IsPlayerAttack(e))
        return;

    mHP -= 20;
    mBody.PlayAnimation("DAMAGE");

    if (mHP <= 0)
        Die();
}
```

ToyKitでは `Slot` という名称に固定せず、Handler /
Callbackとして扱ってよい。

## 10. Collider は複数保持を標準とする

``` text
Creature
├─ Body Collider
├─ Head Hurt Collider
├─ Attack Collider
├─ Ground Sensor
└─ Interaction Sensor
```

``` cpp
enum class ColliderRole
{
    Body,
    Hurt,
    Hit,
    Sensor,
    Interaction,
    Custom
};
```

必要ならBone追従も可能にする。

``` cpp
collider.attachBone = "Tail_03";
```

これにより人間、猫、ドラゴン、多脚モンスターなどを同じ基本機構で扱える。

## 11. Dynamic / Kinematic は設定軸にする

`HumanoidFullPhysics` / `HumanoidKinematic` のようにPrefabを増殖させず、

``` cpp
desc.motionMode = MotionMode::Dynamic;
// または
desc.motionMode = MotionMode::Kinematic;
```

とする。

``` text
Prefab     = 種類
Descriptor = 物理構成・初期設定
```

## 12. UI も宣言的にする

``` cpp
UI()
    .Add<HealthBar>("player_hp")
    .Anchor(Anchor::TopLeft);

UI()
    .Add<MessageBox>("message")
    .Anchor(Anchor::Bottom);
```

Text、Image、Gauge、Panel、Menu、MessageBoxなどの高水準部品を用意する。

3D World Prefabとは性質が異なるため、必要なら `toy::kit::world` と
`toy::kit::ui` のようにAPI上は分離する。

## 13. スクリプト言語対応を前提に API を設計する

``` text
C++ Game ──────┐
Lua ───────────┤
Python ────────┼──→ ToyKit → ToyLib
Other Script ──┘
```

ToyLib内部の
`Actor*`、`Component*`、`std::shared_ptr<>`、Renderer、PhysWorldなどをスクリプトへ直接露出しない。

代わりに、ObjectHandle、PrefabHandle、EntityID、Desc、Signal/Event、Command、State
Queryなど、安定したToyKit APIを境界にする。

## 14. ToyKit のプログラミングモデル

``` text
                     Scene
                       │
          ┌────────────┼────────────┐
          │            │            │
     Environment      World         UI
                       │
                     Spawn
                       │
                       ▼
                    Prefab
                       │
          ┌────────────┴────────────┐
          │                         │
        Signal                  Interface
          │                         ▲
          ▼                         │
                    Game Logic
                 HP / AI / Quest
                Damage / Rules ...
```

基本サイクルは次の4段階。

1.  **Sceneを宣言する**
2.  **Prefabを配置する**
3.  **SignalにGame Logicを接続する**
4.  **InterfaceでPrefabを操作する**

## 15. ToyKit の位置付け

ToyLibが **ゲームを動かすエンジン / ライブラリ**
であるのに対し、ToyKitは **ゲームを組み立てるためのToolkit**
と位置付ける。

Qtで「Widgetを配置してSignal /
Slotで振る舞いを書く」ように、ToyKitでは、

> **Prefabを配置してSignal / Handlerでゲームの振る舞いを書く**

ことを基本的な開発体験とする。

`ToyKit` / `TK` という名称にも、このToolkitとしての方向性を反映する。

## 16. Desc と JSON の対応

DescはJSONとして表現できる範囲に収める。これによりJSONファイルをそのままDescとして扱えるようにする。

``` cpp
// ファイルパスからDescを読み込む
auto cat = Spawn(Creature("cat.json"), {5, 0, 2});
```

``` json
// cat.json
{
    "model": "cat.glb",
    "gravity": -80.0,
    "moveSpeed": 5.0,
    "colliders": [...]
}
```

この方針から、Descの設計上の制約を定める。

- フィールドはJSONでシリアライズできる型のみ（プリミティブ、配列、入れ子構造体）
- 実行時オブジェクトへの参照を持たない
- ランタイムの状態を持たない

「Descは生成時の構成情報」という原則と、「JSONは静的データ」という性質が一致する。デザイナーはJSONを編集し、Game LogicはスクリプトやC++で書く、という役割分担も自然に生まれる。

## 17. スクリプト言語ターゲット

最初のスクリプト言語ターゲットは **Lua** とする。

- C++への組み込みが確立しており実績が多い（sol2 等）
- 文法がシンプルで学習コストが低い
- LuaのテーブルはJSONと自然に対応する

TypeScriptはツールチェーンが複雑になるため、当面は対象外とする。将来的な対応は、ToyKit APIの境界（ObjectHandle、Desc、Signal、Interface）が安定してから検討する。

スクリプト側には ToyLib 内部（`Actor*`、`Component*`、`PhysWorld` 等）を露出しない。スクリプトユーザーが触るのは ToyKit の公開APIのみとする。

## 18. 「BASICでゲームを作る」体験感

ToyKit の最終目標は、**BASICでゲームを書くくらいの気軽さで3Dゲームを作れる**ことである。

- 難しい概念を知らなくてもとりあえず動くものが作れる
- 「動いた」までの距離が極端に短い
- 複雑なことをしたいときだけ深く潜ればいい

この体験感を実現する鍵は **良いデフォルト値** である。物理、コリジョン、アニメーション、影、カメラ追従——何も設定しなくてもデフォルトで動いている。凝りたい場合だけDescやInterfaceで上書きする。

``` lua
-- これだけで3Dキャラクターが物理つきでフィールドに立っている
local player = spawn("hero.glb")
```

また、スクリプトのリロード（ホットリロード）をサポートし、ゲームを起動したまま即座にフィードバックが得られる開発体験を目指す。

## 19. スクリプトユーザーが知らなくていいこと

スクリプト層からは以下を隠蔽する。

``` text
知らなくていいこと（隠す）        知っていること（公開する）
─────────────────────────────────────────────────────
Renderer の詳細                  Prefab の配置・操作
PhysWorld / Actor の管理         Signal への接続
Component のライフサイクル       Desc（JSON）の編集
ObjectHandle の生存管理          Scene の宣言
シェーダー・描画パイプライン      UI の配置
```

スクリプトユーザーが扱う概念は「Sceneに何を置くか」「Signalに何を書くか」の2つに絞る。

## 20. ビジョン：スクリプトでのゲーム記述例

以下は将来のLua APIで書かれたゲームの理想形。C++でのAPI設計の判断基準とする。

``` lua
-- village_scene.lua

function Scene:define()
    self:environment { weather = "clear", timeOfDay = 17.5 }

    player = self:spawn("hero.glb", {0, 0, 0})
    cat    = self:spawn("cat.json",  {5, 0, 2})

    UI.add("HealthBar", { anchor = "top_left" })
    UI.add("MessageBox", { anchor = "bottom" })
end

function Scene:start()
    player.onCollision:connect(function(e)
        if e.otherRole == "Hit" then
            hp = hp - 20
            player:playAnimation("DAMAGE")
            if hp <= 0 then
                player:playAnimation("DEAD")
            end
        end
    end)

    player.onGrounded:connect(function()
        canJump = true
    end)
end
```

このコードで「村のシーン」「プレイヤー」「猫」「HP管理」「衝突ダメージ」が完結する、という体験感を設計のゴールとする。
