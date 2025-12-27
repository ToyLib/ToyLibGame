#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Component.h"

#include <algorithm>
#include <memory>

namespace toy {

//=============================================================================
// Lifecycle
//=============================================================================

//-------------------------------------------------------------
// コンストラクタ
//-------------------------------------------------------------
Actor::Actor(Application* a)
    : mStatus(State::Active)
    , mPosition(Vector3::Zero)
    , mRotation(Quaternion::Identity)
    , mScale(1.0f)
    , mApp(a)
    , mIsRecomputeWorldTransform(true)
    , mPoseRotation(Quaternion::Identity)
    , mActorID("Unnamed Actor")
{
}

//-------------------------------------------------------------
// デストラクタ
//-------------------------------------------------------------
Actor::~Actor()
{
}

//=============================================================================
// Transform (dirty / recompute)
//=============================================================================

//-------------------------------------------------------------
// ワールド行列を再計算する必要があることを通知
//-------------------------------------------------------------
void Actor::MarkWorldDirty()
{
    if (!mIsRecomputeWorldTransform)
    {
        mIsRecomputeWorldTransform = true;
    }
}

//-------------------------------------------------------------
// ワールド行列の再計算（dirty のときのみ）
// ・SRT から WorldTransform を構築
// ・Pose を合成して RenderWorldTransform を更新
// ・Component に OnUpdateWorldTransform() を通知
//-------------------------------------------------------------
void Actor::ComputeWorldTransform()
{
    if (!mIsRecomputeWorldTransform)
    {
        return;
    }

    // SRT（Scale → Rotate → Translate）
    Matrix4 local = Matrix4::CreateScale(mScale);
    local *= Matrix4::CreateFromQuaternion(mRotation);
    local *= Matrix4::CreateTranslation(mPosition);

    mWorldTransform = local;

    // Pose を反映（描画用）
    Matrix4 poseMatrix = Matrix4::CreateFromQuaternion(mPoseRotation);
    mRenderWorldTransform = poseMatrix * mWorldTransform;

    mIsRecomputeWorldTransform = false;

    // Component にワールド更新を通知
    for (auto& comp : mComponents)
    {
        comp->OnUpdateWorldTransform();
    }
}

//=============================================================================
// Update
//=============================================================================

//-------------------------------------------------------------
// 毎フレーム更新
//-------------------------------------------------------------
void Actor::Update(float deltaTime)
{
    if (mStatus != State::Active)
    {
        return;
    }

    // 派生 Actor の処理
    UpdateActor(deltaTime);

    // Component の更新
    UpdateComponents(deltaTime);

    // Transform の確定（必要な場合のみ）
    ComputeWorldTransform();
}

//-------------------------------------------------------------
// Component 更新
//-------------------------------------------------------------
void Actor::UpdateComponents(float deltaTime)
{
    for (auto& comp : mComponents)
    {
        comp->Update(deltaTime);
    }
}

//=============================================================================
// Input
//=============================================================================

//-------------------------------------------------------------
// 入力処理
// ・Component → Actor の順で処理
//-------------------------------------------------------------
void Actor::ProcessInput(const struct InputState& state)
{
    if (mStatus != State::Active)
    {
        return;
    }

    for (auto& comp : mComponents)
    {
        comp->ProcessInput(state);
    }

    ActorInput(state);
}



//=============================================================================
// Component management
//=============================================================================

//-------------------------------------------------------------
// Component を追加（UpdateOrder 順）
//-------------------------------------------------------------
void Actor::AddComponent(std::unique_ptr<Component> component)
{
    int order = component->GetUpdateOrder();
    auto iter = mComponents.begin();

    for (; iter != mComponents.end(); ++iter)
    {
        if (order < (*iter)->GetUpdateOrder())
        {
            break;
        }
    }

    mComponents.insert(iter, std::move(component));
}

//-------------------------------------------------------------
// Component を削除
//-------------------------------------------------------------
void Actor::RemoveComponent(Component* component)
{
    auto iter = std::find_if(
        mComponents.begin(),
        mComponents.end(),
        [component](const std::unique_ptr<Component>& c)
        {
            return c.get() == component;
        }
    );

    if (iter != mComponents.end())
    {
        mComponents.erase(iter);
    }
}

//=============================================================================
// Orientation helpers
//=============================================================================

//-------------------------------------------------------------
// Forward ベクトルを指定して回転を設定（Yaw のみ）
//-------------------------------------------------------------
void Actor::SetForward(const Vector3& dir)
{
    // Y 成分を無視して XZ 平面に投影
    Vector3 flatDir(dir.x, 0.0f, dir.z);

    if (flatDir.LengthSq() == 0.0f)
    {
        return;
    }

    flatDir = Vector3::Normalize(flatDir);

    // Z 前・X 右 前提で Yaw を算出
    float yaw = std::atan2(flatDir.x, flatDir.z);

    Quaternion rot =
        Quaternion::FromEulerDegrees(Vector3(0.0f, yaw, 0.0f));

    SetRotation(rot);
}

} // namespace toy
