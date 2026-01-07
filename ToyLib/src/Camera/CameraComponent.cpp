#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Core/Application.h"
#include "Physics/ColliderComponent.h"

namespace toy {

CameraComponent::CameraComponent(Actor* owner, int updateOrder)
    : Component(owner, updateOrder)
    , mCameraPosition(Vector3::Zero)
    , mCameraTarget(Vector3::Zero)
    , mIsEnabled(true)
{
    // カメラ Actor 生成などがあればここで
    //mCameraActor = std::make_unique<Actor>(owner->GetApp());
    //mCameraActor->SetPosition(owner->GetPosition());

    if (auto* app = owner->GetApp())
    {
        if (auto* mgr = app->GetCameraManager())
        {
            mgr->RegisterCamera(this);
        }
    }
}

CameraComponent::~CameraComponent()
{
    if (auto* app = GetOwner()->GetApp())
    {
        if (auto* mgr = app->GetCameraManager())
        {
            mgr->UnregisterCamera(this);
        }
    }
}

//----------------------------------------------------------------------
// カメラ位置を Renderer に渡す
//   ※現状では Renderer で保持していないため未使用
//----------------------------------------------------------------------
//void CameraComponent::SetCameraPosition(const Vector3& pos)
//{
    // GetOwner()->GetApp()->GetRenderer()->SetCameraPosition(pos);
//}

//----------------------------------------------------------------------
// カメラの現在位置を Renderer から取得し、内部変数に反映
//   - FollowCamera / OrbitCamera などの派生クラスが View を更新
//   - この基底クラスは「現在の View 行列からカメラ位置だけ抜き出す」
//----------------------------------------------------------------------
void CameraComponent::Update(float deltaTime)
{
    if (!mIsEnabled)
    {
        return;
    }

    // Application → CameraManager → ActiveCamera を問い合わせて、
    // 自分がアクティブでなければ何もしない
    auto* app = GetOwner()->GetApp();
    if (app)
    {
        auto* mgr = app->GetCameraManager(); // ※このアクセサを後で追加
        if (mgr && mgr->GetActiveCamera() != this)
        {
            return;
        }
    }

    UpdateCamera(deltaTime);
}

void CameraComponent::SetViewMatrix(const Matrix4 &view)
{
    GetOwner()->GetApp()->GetRenderer()->SetViewMatrix(view);
}

} // namespace toy
