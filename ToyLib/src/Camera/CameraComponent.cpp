#include "Camera/CameraComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Core/Application.h"
#include "Physics/ColliderComponent.h"

namespace toy {

CameraComponent::CameraComponent(Actor* a, int updateOrder)
    : Component(a, updateOrder)
    , mIsInable(true)
{
    // カメラ計算に利用する補助アクター
    // （これを使って位置・向きの独立計算を行う）
    mCameraActor = std::make_unique<Actor>(GetOwner()->GetApp());
}

//----------------------------------------------------------------------
// View 行列を Renderer に登録
//----------------------------------------------------------------------
void CameraComponent::SetViewMatrix(const Matrix4& view)
{
    GetOwner()->GetApp()->GetRenderer()->SetViewMatrix(view);
}

//----------------------------------------------------------------------
// カメラ位置を Renderer に渡す
//   ※現状では Renderer で保持していないため未使用
//----------------------------------------------------------------------
void CameraComponent::SetCameraPosition(const Vector3& pos)
{
    // GetOwner()->GetApp()->GetRenderer()->SetCameraPosition(pos);
}

//----------------------------------------------------------------------
// カメラの現在位置を Renderer から取得し、内部変数に反映
//   - FollowCamera / OrbitCamera などの派生クラスが View を更新
//   - この基底クラスは「現在の View 行列からカメラ位置だけ抜き出す」
//----------------------------------------------------------------------
void CameraComponent::Update(float deltaTime)
{
    auto invView = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix();
    mCameraPosition = invView.GetTranslation();

    if (mIsInable)
    {
        UpdateCamera(deltaTime);
    }
}

} // namespace toy
