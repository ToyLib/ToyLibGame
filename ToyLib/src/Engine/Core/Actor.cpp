#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Component.h"

#include <algorithm>
#include <iostream>
#include <memory>

namespace toy {

//=============================================================
// コンストラクタ／デストラクタ
//=============================================================

// コンストラクタ
Actor::Actor(Application* a)
    : mStatus(EActive)
    , mPosition(Vector3::Zero)
    , mRotation(Quaternion::Identity)
    , mScale(1.0f)
    , mApp(a)
    , mIsRecomputeWorldTransform(true)
    , mPoseRotation(Quaternion::Identity)
    , mInheritParentPose(true)
    , mActorID("Unnamed Actor")
    , mParent(nullptr)
{
}

Actor::~Actor()
{
    // 親の子リストから自分を外す
    if (mParent)
    {
        auto& siblings = mParent->mChildren;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }
    
    // 子の親参照をクリア（ここでは「孤児」にする方針）
    for (auto* child : mChildren)
    {
        if (child)
        {
            child->mParent = nullptr;
            child->MarkWorldDirty();
        }
    }
}

//=============================================================
// Transform 更新（親子連鎖）
//=============================================================

// ワールド行列の再計算フラグを立て、子にも伝播
void Actor::MarkWorldDirty()
{
    if (!mIsRecomputeWorldTransform)
    {
        mIsRecomputeWorldTransform = true;
        
        // 子へも伝播
        for (auto* child : mChildren)
        {
            if (child)
            {
                child->MarkWorldDirty();
            }
        }
    }
}

// ワールドマトリックスの計算
void Actor::ComputeWorldTransform()
{
    if (!mIsRecomputeWorldTransform)
    {
        return;
    }
    // 親がいるなら、まず親の WorldTransform を確定させる
    if (mParent)
    {
        mParent->ComputeWorldTransform();
    }
    
    // ローカル行列（SRT）
    Matrix4 local = Matrix4::CreateScale(mScale);
    local *= Matrix4::CreateFromQuaternion(mRotation);
    local *= Matrix4::CreateTranslation(mPosition);
    
    if (mParent)
    {
        // 親ワールドのコピーを作る
        Matrix4 parentBase =
            (mInheritParentPose)
                ? mParent->mRenderWorldTransform
                : mParent->mWorldTransform;

        Matrix4 parentNoScale = parentBase;
        
        // 親の軸は GetXAxis/Y/Z が「正規化済みの向き」を返すので、
        // それらを書き戻すことでスケール成分を 1 にリセット
        parentNoScale.SetXAxis(mParent->mWorldTransform.GetXAxis());
        parentNoScale.SetYAxis(mParent->mWorldTransform.GetYAxis());
        parentNoScale.SetZAxis(mParent->mWorldTransform.GetZAxis());
        // 平行移動はそのまま使う
        
        // 親の「スケールなし」行列と自分のローカル行列からワールド行列を作成
        mWorldTransform = local * parentNoScale;
    }
    else
    {
        mWorldTransform = local;
    }
    
    // ポーズを反映
    Matrix4 poseMatrix = Matrix4::CreateFromQuaternion(mPoseRotation);
    mRenderWorldTransform = poseMatrix * mWorldTransform;
    
    mIsRecomputeWorldTransform = false;
    
    // 各 Component にもワールド更新イベントを通知
    for (auto& comp : mComponents)
    {
        comp->OnUpdateWorldTransform();
    }
}

//=============================================================
// Update 系
//=============================================================

// メインルーチン（毎フレームの更新）
void Actor::Update(float deltaTime)
{
    // EActive のときのみ更新
    if (mStatus == EActive)
    {
        // 派生クラス側の処理
        UpdateActor(deltaTime);
        
        // コンポーネントの更新
        UpdateComponents(deltaTime);
        
        // 座標系更新
        ComputeWorldTransform();
    }
}

// 全コンポーネントの Update を呼ぶ
void Actor::UpdateComponents(float deltaTime)
{
    for (auto& comp : mComponents)
    {
        comp->Update(deltaTime);
    }
}

//=============================================================
// 入力処理
//=============================================================

// 入力を受け取り、コンポーネント → Actor の順に処理
void Actor::ProcessInput(const struct InputState& state)
{
    if (mStatus == EActive)
    {
        // コンポーネントの入力処理
        for (auto& comp : mComponents)
        {
            comp->ProcessInput(state);
        }
        // 派生クラス側の入力処理
        ActorInput(state);
    }
}

// Actor 固有の入力処理（デフォルトは何もしない）
void Actor::ActorInput(const struct InputState& state)
{
    (void)state;
}

//=============================================================
// コンポーネント管理
//=============================================================

// コンポーネントを追加（UpdateOrder 順に挿入）
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

// コンポーネントを削除
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

//=============================================================
// 向き／位置／親子関係
//=============================================================

// 前方ベクトルを指定して回転を設定
void Actor::SetForward(const Vector3& dir)
{
    // Y成分を無視（XZ平面に投影）
    Vector3 flatDir = Vector3(dir.x, 0.0f, dir.z);
    
    if (flatDir.LengthSq() == 0.0f)
    {
        return; // 方向なし
    }
    flatDir = Vector3::Normalize(flatDir);
    
    // atan2(x, z) で Yaw を取得（Zが前、Xが右 の前提）
    float yaw = std::atan2(flatDir.x, flatDir.z);
    
    // Yaw から Quaternion を生成（Pitch = 0, Roll = 0）
    Quaternion rot = Quaternion::FromEulerDegrees(Vector3(0.0f, yaw, 0.0f));
    
    SetRotation(rot);
}

// 位置設定
void Actor::SetPosition(const Vector3& pos)
{
    // 親がいる場合は「親からのオフセット（ローカル）」、
    // 親がいない場合は「ワールド座標」として扱う
    mPosition = pos;
    MarkWorldDirty();
}

// 親の設定（子リストの付け替えのみ／ワールド維持はしない）
void Actor::SetParent(Actor* newParent)
{
    if (mParent == newParent)
    {
        return;
    }
    // 古い親から外す
    if (mParent)
    {
        auto& siblings = mParent->mChildren;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }
    
    mParent = newParent;
    
    // 新しい親に自分を登録
    if (mParent)
    {
        mParent->mChildren.push_back(this);
    }
    
    // ローカル値として扱うだけ（ワールド位置維持などはここでは行わない）
    MarkWorldDirty();
}

} // namespace toy
