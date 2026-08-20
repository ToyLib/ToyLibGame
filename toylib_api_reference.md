# ToyLib API リファレンス

> **対象バージョン:** ToyLib (2026)  
> **対象読者:** ToyLib を使ってゲームを開発する C++ プログラマー  
> **名前空間:** `toy::` （ToyKit は `toy::kit::`）

---

## 目次

1. [Engine Core](#1-engine-core)
   - [Application](#application)
   - [Actor](#actor)
   - [Component](#component)
2. [Engine Runtime — InputSystem](#2-engine-runtime--inputsystem)
   - [InputState / KeyboardState / ControllerState](#inputstate--keyboardstate--controllerstate)
   - [InputSystem](#inputsystem)
3. [Camera](#3-camera)
   - [CameraComponent](#cameracomponent)
   - [FollowCameraComponent](#followcameracomponent)
4. [Graphics](#4-graphics)
   - [MeshComponent](#meshcomponent)
   - [SkeletalMeshComponent](#skeletalmeshcomponent)
   - [ParticleComponent](#particlecomponent)
5. [Physics](#5-physics)
   - [ColliderComponent](#collidercomponent)
   - [GravityComponent](#gravitycomponent)
6. [Audio](#6-audio)
   - [SoundComponent](#soundcomponent)
   - [SoundMixer](#soundmixer)
7. [Movement](#7-movement)
   - [MoveComponent](#movecomponent)
8. [ToyKit — シーン管理](#8-toykit--シーン管理)
   - [IScene](#iscene)
   - [GameFlow](#gameflow)

---

## 1. Engine Core

### Application

`#include "ToyLib.h"` または `#include "Engine/Core/Application.h"`

ゲーム全体のエントリポイントとなるクラス。サブシステムの初期化・メインループ・Actor 管理を担う。ゲーム側では `Application` を継承し、仮想関数をオーバーライドして使う。

#### ライフサイクル

| メソッド | 説明 |
|---|---|
| `bool Initialize()` | SDL・レンダラー・各サブシステムを初期化する。ループ開始前に呼ぶ。 |
| `void RunLoop()` | メインループ（`ProcessInput → UpdateFrame → Draw`）を実行する。 |
| `void Shutdown()` | 全リソースを解放する。 |

#### オーバーライドフック（protected）

| 仮想関数 | 呼ばれるタイミング |
|---|---|
| `virtual void InitGame()` | Initialize 完了直後。アセット読み込み・初期 Actor 生成をここで行う。 |
| `virtual void UpdateGame(float deltaTime)` | 毎フレーム、全 Actor 更新後に呼ばれる。 |
| `virtual void ProcessInput(const InputState& input)` | 毎フレームの入力処理フック。 |
| `virtual void ShutdownGame()` | Shutdown の前に呼ばれる。 |

#### Actor 管理

```cpp
// 型安全な生成（推奨）
T* CreateActor<T>(Args&&... args);

// 既存の unique_ptr を追加
void AddActor(std::unique_ptr<Actor> a);

// 削除予約（次フレームに削除される）
void DestroyActor(Actor* actor);
```

#### サブシステム取得

```cpp
IRenderer*       GetRenderer()        const;
PhysWorld*       GetPhysWorld()       const;
AssetManager*    GetAssetManager()    const;
SoundMixer*      GetSoundMixer()      const;
InputSystem*     GetInputSystem()     const;
TimeOfDaySystem* GetTimeOfDaySystem() const;
CameraManager*   GetCameraManager()   const;
```

#### ウィンドウ操作

```cpp
void SetFullscreen(bool enable);
void ToggleFullscreen();
bool IsFullScreen() const;
```

#### デバッグ

```cpp
void SetEnableDebug(bool b);       // デバッグオーバーレイ表示
void SetVisibleDebuWire(bool b);   // ワイヤーフレーム表示
float GetTimeSconds() const;       // 起動からの経過時間（秒）
```

**使用例**

```cpp
class MyGame : public toy::Application
{
protected:
    void InitGame() override
    {
        auto* player = CreateActor<PlayerActor>();
    }

    void UpdateGame(float deltaTime) override
    {
        // ゲーム固有の更新処理
    }
};
```

---

### Actor

`#include "Engine/Core/Actor.h"`

ゲームワールドに存在するエンティティの基底クラス。位置・回転・スケールを持ち、複数の `Component` を保持する。

#### 状態（State）

```cpp
enum class State { Active, Paused, Dead };
```

| 値 | 意味 |
|---|---|
| `Active` | 毎フレーム更新される通常状態 |
| `Paused` | 更新停止（描画は継続） |
| `Dead` | 削除予約（次フレームに破棄） |

```cpp
void SetState(const State& state);
const State& GetState() const;
void DestroyActor();   // State を Dead にするショートカット
```

#### Transform

位置・回転・スケールはワールド座標基準。変更すると内部で「dirty」フラグが立ち、次回 `GetWorldTransform()` 呼び出し時に行列を再計算する（遅延計算）。

```cpp
// 位置
void      SetPosition(const Vector3& pos);
Vector3   GetPosition() const;

// 回転
void       SetRotation(const Quaternion& rot);
Quaternion GetRotation() const;

// スケール（均一スケール）
void  SetScale(float sc);
float GetScale() const;

// 方向ベクトル（読み取り専用）
Vector3 GetForward() const;   // +Z 方向
Vector3 GetRight()   const;   // +X 方向
Vector3 GetUpward()  const;   // +Y 方向

// 前方向を直接設定（内部で回転を調整）
void SetForward(const Vector3& dir);

// ワールド行列取得（必要なら再計算）
const Matrix4& GetWorldTransform();
```

#### ポーズ（描画専用の姿勢補正）

物理・判定には影響しない。キャラが斜面に沿って傾くなどの見た目用途に使う。

```cpp
void       SetPoseRotation(const Quaternion& q);
Quaternion GetPoseRotation() const;
const Matrix4& GetRenderWorldTransform();
```

#### Component 管理

```cpp
// 型安全な生成（推奨）
T* CreateComponent<T>(Args&&... args);

// 最初に見つかった T を返す（なければ nullptr）
T* GetComponent<T>() const;

// 該当する T をすべて返す
std::vector<T*> GetAllComponents<T>() const;

void AddComponent(std::unique_ptr<Component> component);
void RemoveComponent(Component* component);
```

#### オーバーライドフック

```cpp
virtual void UpdateActor(float deltaTime) {}   // 毎フレームの Actor 固有更新
virtual void ActorInput(const InputState& state) {}  // 入力処理
```

#### その他

```cpp
void        SetActorID(const std::string& id);
std::string GetActorID() const;
Application* GetApp();
```

**使用例**

```cpp
class EnemyActor : public toy::Actor
{
public:
    EnemyActor(toy::Application* app) : toy::Actor(app)
    {
        mMesh    = CreateComponent<toy::SkeletalMeshComponent>();
        mMove    = CreateComponent<toy::MoveComponent>();
        mCollider = CreateComponent<toy::ColliderComponent>();
    }

    void UpdateActor(float deltaTime) override
    {
        // 毎フレームの AI 処理
    }
};
```

---

### Component

`#include "Engine/Core/Component.h"`

Actor に機能を追加する基底クラス。`MeshComponent` や `MoveComponent` など全コンポーネントがこれを継承する。

#### コンストラクタ

```cpp
Component(Actor* owner, int updateOrder = 100);
```

`updateOrder` が小さいほど先に更新される。移動系を `10`、描画系を `200` にするなど、依存関係に応じて調整する。

#### オーバーライドフック

```cpp
virtual void Update(float deltaTime) {}           // 毎フレーム更新
virtual void ProcessInput(const InputState&) {}   // 入力処理
virtual void OnUpdateWorldTransform() {}          // ワールド行列更新後
```

#### 情報取得

```cpp
Actor* GetOwner() const;
int    GetUpdateOrder() const;
virtual Vector3 GetPosition() const;   // ワールド位置（必要なら override）
```

---

## 2. Engine Runtime — InputSystem

`#include "Engine/Runtime/InputSystem.h"`

### InputState / KeyboardState / ControllerState

毎フレームのフレームスナップショット。`Actor::ActorInput()` や `Application::ProcessInput()` の引数として渡される。

#### ButtonState（ボタンの1フレーム状態）

```cpp
enum ButtonState
{
    ENone,      // 何も押していない
    EPressed,   // このフレームで押した
    EReleased,  // このフレームで離した
    EHeld       // 押しっぱなし
};
```

#### GameButton（論理ボタン）

キーボード・ゲームパッドを抽象化した論理ボタン。JSON でキーバインドを変更できる。

```
A, B, X, Y, L1, L2, R1, R2, Start, Select,
DPadUp, DPadDown, DPadLeft, DPadRight,
KeyW, KeyA, KeyS, KeyD
```

#### InputState

```cpp
// 論理ボタン問い合わせ
bool IsButtonDown(GameButton button) const;      // 押されている
bool IsButtonPressed(GameButton button) const;   // このフレームで押した
bool IsButtonReleased(GameButton button) const;  // このフレームで離した

// 低レベルアクセス
KeyboardState   Keyboard;
ControllerState Controller;
```

#### KeyboardState（キーボード生状態）

```cpp
bool        GetKeyValue(SDL_Scancode keyCode) const;   // 現在押されているか
ButtonState GetKeyState(SDL_Scancode keyCode) const;   // フレーム状態
```

#### ControllerState（コントローラー）

```cpp
bool        GetButtonValue(SDL_GamepadButton button) const;
ButtonState GetButtonState(SDL_GamepadButton button) const;

Vector2 GetLeftStick()  const;   // 左スティック（-1.0〜1.0、デッドゾーン処理済み）
Vector2 GetRightStick() const;   // 右スティック
float   GetLeftTrigger()  const; // 左トリガー（0.0〜1.0）
float   GetRightTrigger() const; // 右トリガー

ButtonState GetTriggerState(bool isLeft) const;  // トリガーをボタン扱い
bool GetIsConnected() const;
```

---

### InputSystem

`Application::GetInputSystem()` で取得する。通常は `InputState` 経由で問い合わせるが、直接呼び出すことも可能。

```cpp
const InputState& GetState() const;

bool IsButtonDown(GameButton button) const;
bool IsButtonPressed(GameButton button) const;
bool IsButtonReleased(GameButton button) const;

bool LoadButtonConfig(const std::string& filePath);  // JSON からキーバインド読み込み
void SetTextInputMode(bool enabled);                 // テキスト入力モード
bool IsTextInputMode() const;
```

**使用例**

```cpp
void PlayerActor::ActorInput(const toy::InputState& state)
{
    // 論理ボタンで判定（キーボード/パッド共通）
    if (state.IsButtonPressed(toy::GameButton::A))
    {
        mGravity->Jump();
    }

    // スティック直接読み取り
    toy::Vector2 stick = state.Controller.GetLeftStick();
    mMove->SetForwardSpeed(stick.y * mMoveSpeed);
}
```

---

## 3. Camera

### CameraComponent

`#include "Camera/CameraComponent.h"`

カメラ挙動を実装する基底コンポーネント。`FollowCameraComponent` や `OrbitCameraComponent` がこれを継承する。`CameraManager` を通じてアクティブカメラを切り替える。

```cpp
// 有効/無効
void SetIsEnabled(bool enable);
bool GetIsEnabled() const;

// 現在のカメラ位置・注視点（CameraManager が参照する）
const Vector3& GetCameraPosition() const;
const Vector3& GetCameraTarget()   const;
```

#### オーバーライドフック

```cpp
// 毎フレームのカメラ更新（View 行列を SetViewMatrix() で登録する）
virtual void UpdateCamera(float deltaTime) {}

// アクティブになった瞬間に呼ばれる（トランジション開始に活用）
virtual void OnActivated(const Vector3& prevPos, const Vector3& prevTarget) {}
```

#### カスタムカメラの実装例

```cpp
class MyCamera : public toy::CameraComponent
{
public:
    MyCamera(toy::Actor* owner) : toy::CameraComponent(owner) {}

    void UpdateCamera(float deltaTime) override
    {
        toy::Vector3 eye    = GetOwner()->GetPosition() + toy::Vector3(0, 5, -10);
        toy::Vector3 target = GetOwner()->GetPosition();
        toy::Matrix4 view   = toy::Matrix4::CreateLookAt(eye, target, toy::Vector3::UnitY);
        SetViewMatrix(view);
        SetCameraPosition(eye);
        mCameraTarget = target;
    }
};
```

---

### FollowCameraComponent

`#include "Camera/FollowCameraComponent.h"`

所有 Actor を後方から追従するサードパーソンカメラ。スプリングダンパーで滑らかに追従する。

#### パラメータ設定

```cpp
// カメラ距離（水平・垂直）
void SetDistance(float horz, float vert);

// 注視点オフセット距離
void SetTargetDistance(float dist);

// スプリング設定（追従の硬さ・減衰）
void SetSpringSettings(const SpringSettings& s);

// 縦方向の可動範囲（W/S キーで変化）
void SetHeightRange(float minVert, float maxVert);

// 縦移動速度
void SetHeightSpeed(float speed);

// 空中での Y 軸固定（ジャンプ中にカメラが跳ねない）
void SetFreezeYInAir(bool enable);
bool GetFreezeYInAir() const;

// アクティブ化した瞬間にカメラをスナップ（トランジションなし）
void SnapToIdeal();
```

#### SpringSettings

```cpp
struct SpringSettings
{
    float Stiffness    = 200.0f;  // バネ定数（大きいほど素早く追従）
    float DampingRatio = 1.0f;    // 減衰比（1.0 = 臨界減衰、振動なし）
};
```

**使用例**

```cpp
void PlayerActor::PlayerActor(toy::Application* app) : toy::Actor(app)
{
    auto* cam = CreateComponent<toy::FollowCameraComponent>();
    cam->SetDistance(10.0f, 4.0f);
    cam->SetHeightRange(1.0f, 10.0f);
    cam->SetSpringSettings({ 150.0f, 1.0f });
    cam->SetFreezeYInAir(true);
}
```

---

## 4. Graphics

### MeshComponent

`#include "Graphics/Mesh/MeshComponent.h"`

静的3Dメッシュを描画するコンポーネント。GLTF / FBX / DirectX X 形式のメッシュに対応する。

```cpp
// メッシュのセット
void SetMesh(std::shared_ptr<Mesh> m);
const std::shared_ptr<Mesh>& GetMesh() const;

// テクスチャのインデックス（複数テクスチャを持つメッシュ用）
void SetTextureIndex(unsigned int index);

// ローカル補正（メッシュの原点ずれを補正するために使う）
void SetLocalScale(float scale);
void SetLocalPositon(const Vector3& pos);
void SetYawOffset(float radians);   // Y 軸回転補正

// トゥーンレンダリング（フラグのみ）
void SetToonRender(bool t);
void SetContourFactor(float f);
void SetContourColor(const Vector3& color);
```

**使用例**

```cpp
auto* mesh = CreateComponent<toy::MeshComponent>();
auto  m    = app->GetAssetManager()->GetMesh("Assets/tree.glb");
mesh->SetMesh(m);
mesh->SetLocalScale(0.5f);
```

---

### SkeletalMeshComponent

`#include "Graphics/Mesh/SkeletalMeshComponent.h"`

ボーンアニメーション付きメッシュを描画するコンポーネント。`MeshComponent` を継承する。最大 96 ボーンまでサポート。

```cpp
// メッシュのセット（SkeletalMesh を自動認識）
void SetMesh(std::shared_ptr<Mesh> m) override;

// アニメーション切り替え
void SetAnimID(unsigned int animID, bool mode) override;

// AnimationPlayer への直接アクセス（細かい制御が必要な場合）
AnimationPlayer* GetAnimPlayer();
```

**使用例**

```cpp
auto* skelMesh = CreateComponent<toy::SkeletalMeshComponent>();
auto  mesh     = app->GetAssetManager()->GetMesh("Assets/character.glb");
skelMesh->SetMesh(mesh);
skelMesh->SetAnimID(0, true);   // アニメ 0 番をループ再生
```

---

### ParticleComponent

`#include "Graphics/Effect/ParticleComponent.h"`

パーティクルエフェクトを再生するコンポーネント。パラメータはコードまたは JSON ファイルで定義する。

```cpp
// コードで初期化
void Init(const Desc& desc);

// JSON ファイルから初期化（推奨）
bool InitFromFile(const std::string& filePath);

// 再生制御
void Start();
void Stop();
void Reset();
```

**使用例**

```cpp
auto* fx = CreateComponent<toy::ParticleComponent>();
fx->InitFromFile("Assets/Particles/explosion.json");
fx->Start();
```

---

## 5. Physics

### ColliderComponent

`#include "Physics/ColliderComponent.h"`

当たり判定を提供するコンポーネント。生成時に `BoundingVolumeComponent` を自動追加し、`PhysWorld` に登録される。コライダーの種別はビットフラグで管理する。

#### フラグ操作

```cpp
void     SetFlags(uint32_t flags);      // フラグを一括セット
void     AddFlag(uint32_t flag);        // フラグを追加
void     RemoveFlag(uint32_t flag);     // フラグを除去
bool     HasFlag(uint32_t flag) const;  // 単一フラグ判定
bool     HasAnyFlag(uint32_t mask) const;  // どれか1つ含むか
bool     HasAllFlags(uint32_t mask) const; // 全て含むか
uint32_t GetFlags() const;
```

#### 衝突情報

```cpp
// 衝突した相手一覧（毎フレームの結果）
const std::vector<ColliderComponent*>& GetTargetColliders() const;

// 少なくとも1つと衝突しているか
bool GetCollided() const;

// トリガーモード（衝突しても押し返さない）
void SetIsTrigger(bool b);
bool IsTrigger() const;

// 有効/無効
void SetEnabled(bool b);
bool GetEnabled() const;

// ターゲッティング状態（ロックオン等の UI 用）
void        SetTargetState(TargetState s);
TargetState GetTargetState() const;
```

**ColliderFlags の例（ゲーム側で定義）**

```cpp
// ColliderFlags.h（ゲーム側で定義するビットフラグの例）
constexpr uint32_t C_NONE       = 0;
constexpr uint32_t C_PLAYER     = 1 << 0;
constexpr uint32_t C_ENEMY      = 1 << 1;
constexpr uint32_t C_WALL       = 1 << 2;
constexpr uint32_t C_FOOT       = 1 << 3;
constexpr uint32_t C_CEILING    = 1 << 4;
```

**使用例**

```cpp
auto* col = CreateComponent<toy::ColliderComponent>();
col->SetFlags(C_PLAYER);

// 更新でチェック
void UpdateActor(float deltaTime) override
{
    for (auto* other : mCollider->GetTargetColliders())
    {
        if (other->HasFlag(C_ENEMY))
        {
            // 敵に当たった処理
        }
    }
}
```

---

### GravityComponent

`#include "Physics/GravityComponent.h"`

重力・接地・ジャンプを処理するコンポーネント。`C_FOOT` フラグを持つ `ColliderComponent` を足元として使用する。内部でサブステップ処理を行うため、低 FPS でも床抜けが起きにくい。

#### ジャンプ

```cpp
void Jump();  // 接地中のみ有効。上向き初速を与えて空中状態に移行。
```

#### パラメータ

```cpp
void SetSelfFlag(uint32_t flag);     // 自分のコライダー種別（C_PLAYER 等）
void SetGravityAccel(float g);       // 重力加速度（デフォルト -60.0）
void SetJumpSpeed(float s);          // ジャンプ初速（デフォルト 22.0）
void SetMaxFallSpeed(float v);       // 最大落下速度（デフォルト -40.0）
void SetMaxStepUp(float v);          // 段差上り許容量（デフォルト 0.35）
void SetMaxStepDown(float v);        // 段差下り許容量（デフォルト 0.75）
void SetEnableGroundPose(bool b);    // 地面法線に合わせて姿勢補正するか
```

#### 状態参照

```cpp
bool  IsGrounded()   const;   // 接地中か
float GetVelocityY() const;   // 現在の Y 軸速度

// 地面情報（影・エフェクト等に活用）
const GroundPose& GetGroundPose() const;
float   GetGroundY()      const;
Vector3 GetGroundNormal() const;

// 現在乗っている床コライダー（動く床への追従判定等）
const ColliderComponent* GetGroundCollider() const;
```

**使用例**

```cpp
mGravity = CreateComponent<toy::GravityComponent>();
mGravity->SetSelfFlag(C_PLAYER);
mGravity->SetJumpSpeed(25.0f);
mGravity->SetGravityAccel(-50.0f);

// ジャンプ
if (state.IsButtonPressed(toy::GameButton::A) && mGravity->IsGrounded())
{
    mGravity->Jump();
}
```

---

## 6. Audio

### SoundComponent

`#include "Audio/SoundComponent.h"`

Actor に取り付けて使う3D/2Dサウンド再生コンポーネント。OpenAL ソースを1つ保持し、Actor の位置と連動する。

```cpp
// 再生するサウンドファイルを設定
void SetSound(const std::string& fileName);

// 再生制御
void Play();
void Stop();
bool IsPlaying() const;

// 基本設定
void SetVolume(float volume);     // 0.0〜1.0
void SetLoop(bool loop);          // ループ再生
void SetAutoPlay(bool autoPlay);  // 生成直後に自動再生

// 3D 音響（距離減衰）
void Enable3DSound(bool use3DSound);

// 排他モード（同じ音が別の SoundComponent で鳴っていたら再生しない）
void SetExclusive(bool isExclusive);
```

**使用例**

```cpp
auto* sound = CreateComponent<toy::SoundComponent>();
sound->SetSound("Assets/Sounds/footstep.wav");
sound->SetVolume(0.8f);
sound->Enable3DSound(true);
sound->SetExclusive(true);
sound->Play();
```

---

### SoundMixer

`#include "Audio/SoundMixer.h"`

`Application::GetSoundMixer()` で取得する。BGM ストリーミング再生・効果音ワンショット再生・マスターボリューム管理を担う。

```cpp
// BGM 読み込み＆制御
bool LoadBGM(const std::string& fileName);
void PlayBGM();
void StopBGM();

// 効果音のワンショット再生
void PlaySoundEffect(const std::string& fileName);

// 音量
void SetBgmVolume(float volume);       // BGM ボリューム（0.0〜1.0）
void SetMasterVolume(float volume);    // マスターボリューム
float GetMasterVolume() const;

// ON/OFF
void SetBGMEnable(bool enable);
void SetSoundEnable(bool enable);
```

**使用例**

```cpp
toy::SoundMixer* mixer = GetApp()->GetSoundMixer();
mixer->LoadBGM("Assets/BGM/stage1.mp3");
mixer->SetBgmVolume(0.7f);
mixer->PlayBGM();

// 効果音（ワンショット）
mixer->PlaySoundEffect("Assets/Sounds/coin.wav");
```

---

## 7. Movement

### MoveComponent

`#include "Movement/MoveComponent.h"`

Actor の移動・回転を行う基本コンポーネント。速度パラメータを `set` すれば毎フレーム自動で位置を更新する。updateOrder のデフォルトは `10`（早めに処理される）。

```cpp
// 速度パラメータ
void SetForwardSpeed(float speed);   // 前後移動速度
void SetRightSpeed(float speed);     // 左右ストレイフ速度
void SetVerticalSpeed(float speed);  // 上下移動速度
void SetAngularSpeed(float speed);   // 回転速度（ヨー）

float GetForwardSpeed()  const;
float GetRightSpeed()    const;
float GetVerticalSpeed() const;
float GetAngularSpeed()  const;

// 移動・回転のロック
void SetIsMovable(bool b);   // false にすると速度もリセットされる
void SetIsTurnable(bool b);
bool GetIsMovable()  const;
bool GetIsTurnable() const;

// 速度リセット
void Reset();

// 壁すり抜け防止移動（Ray で衝突判定してから移動）
bool TryMoveWithRayCheck(const Vector3& moveVec, float deltaTime);
```

**使用例**

```cpp
auto* move = CreateComponent<toy::MoveComponent>();

// 入力処理
void PlayerActor::ActorInput(const toy::InputState& state)
{
    float fw = 0.0f;
    if (state.IsButtonDown(toy::GameButton::KeyW)) fw =  mSpeed;
    if (state.IsButtonDown(toy::GameButton::KeyS)) fw = -mSpeed;
    mMove->SetForwardSpeed(fw);
}
```

---

## 8. ToyKit — シーン管理

> **名前空間:** `toy::kit::`  
> `#include "ToyKit.h"` または個別ヘッダー

### IScene

`#include "KitCore/IScene.h"`

ゲームの1状態（タイトル / ゲームプレイ / ゲームオーバー等）を表す基底クラス。`InitScene()` で Actor を生成し、`UnloadScene()` で後片付けをする。Actor の寿命はシーンと連動し、シーン終了時に一括破棄される。

#### ライフサイクル

| メソッド | 呼ばれるタイミング |
|---|---|
| `virtual void InitScene()` | シーン開始時（Actor 生成はここで行う） |
| `virtual void UnloadScene()` | シーン終了時（後片付け） |
| `virtual void ProcessInput(const InputState&)` | 毎フレームの入力処理 |
| `virtual void Update(float deltaTime)` | 毎フレームの更新処理 |

#### Actor 生成（protected）

```cpp
// Application::CreateActor() のラッパー。生成した Actor は自動登録される。
T* CreateActor<T>(Args&&... args);
```

#### シーン遷移

```cpp
// 次のシーンへリクエストを送る（GameFlow が処理する）
void RequestChange(std::unique_ptr<IScene> next);

// Application への参照
const Application* GetApp();
```

**実装例**

```cpp
class GameScene : public toy::kit::IScene
{
protected:
    void InitScene() override
    {
        mPlayer = CreateActor<PlayerActor>();
        mCamera = CreateActor<CameraActor>();
    }

    void Update(float deltaTime) override
    {
        if (mPlayer->IsDead())
        {
            RequestChange(std::make_unique<GameOverScene>());
        }
    }

private:
    PlayerActor* mPlayer = nullptr;
    CameraActor* mCamera = nullptr;
};
```

---

### GameFlow

`#include "KitCore/GameFlow.h"`

シーンのライフサイクルを管理するクラス。フェードイン/アウトを伴うシーン遷移を制御する。`Application` のサブクラスで保持し、毎フレーム `Update()` を呼ぶ。

#### FlowState

```cpp
enum class FlowState
{
    Running,      // 通常実行中
    FadeOut,      // フェードアウト中
    SwitchScene,  // シーン切り替え中
    FadeIn        // フェードイン中
};
```

#### API

```cpp
explicit GameFlow(toy::Application* app);

void Init();

// 最初のシーンをセット（アプリ起動時に呼ぶ）
void SetInitialScene(std::unique_ptr<IScene> scene);

// シーン遷移リクエスト
void RequestChange(std::unique_ptr<IScene> next);

// Application から毎フレーム呼ぶ
void ProcessInput(const InputState& input);
void Update(float deltaTime);
```

**使用例**

```cpp
class MyGame : public toy::Application
{
    toy::kit::GameFlow mFlow { this };

protected:
    void InitGame() override
    {
        mFlow.Init();
        mFlow.SetInitialScene(std::make_unique<TitleScene>());
    }

    void UpdateGame(float deltaTime) override
    {
        mFlow.Update(deltaTime);
    }

    void ProcessInput(const toy::InputState& input) override
    {
        mFlow.ProcessInput(input);
    }
};
```

---

## 付録：よく使う型

| 型 | 用途 |
|---|---|
| `Vector2` | 2次元ベクトル（スティック入力など） |
| `Vector3` | 3次元ベクトル（位置・方向・速度） |
| `Quaternion` | 回転（`Quaternion(Vector3::UnitY, angle)` でY軸回転を作成） |
| `Matrix4` | 4x4行列（ワールド行列・ビュー行列など） |

定数値:

```cpp
Vector3::Zero     // (0, 0, 0)
Vector3::UnitX    // (1, 0, 0)
Vector3::UnitY    // (0, 1, 0)
Vector3::UnitZ    // (0, 0, 1)
Quaternion::Identity
Matrix4::Identity
```

---

*© Daisuke Nishimori — ToyLib is licensed under the MIT License.*
