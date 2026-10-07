//======================================================================
// FixedCameraComponent.cpp
//======================================================================
#include "Camera/FixedCameraComponent.h"

#include <cmath>

namespace toy {

//======================================================================
// Constructor
//======================================================================
FixedCameraComponent::FixedCameraComponent(Actor* owner, int updateOrder)
    : CameraComponent(owner, updateOrder)
{
    // 初期値：原点から +Z 方向を見る
    mCameraPosition = Vector3::Zero;
    mCameraTarget   = Vector3::UnitZ;
}


//======================================================================
// LookAt
//======================================================================
void FixedCameraComponent::LookAt(const Vector3& eye,
                                  const Vector3& target)
{
    mCameraPosition = eye;
    mCameraTarget   = target;
}


//======================================================================
// UpdateCamera
//
//  ・セットされた位置 / 注視点から View 行列を作って登録するだけ
//======================================================================
void FixedCameraComponent::UpdateCamera(float /*deltaTime*/)
{
    Vector3 eye = mCameraPosition;
    Vector3 at  = mCameraTarget;
    Vector3 up  = mUpVector;

    Vector3 forward = at - eye;

    if (forward.IsZero())
    {
        forward = Vector3::UnitZ;
        at      = eye + forward;
    }

    forward.Normalize();

    // 視線と上方向がほぼ平行なら上方向を差し替える
    if (std::fabs(Vector3::Dot(forward, up)) > 0.99f)
    {
        up = Vector3::UnitX;

        if (std::fabs(Vector3::Dot(forward, up)) > 0.99f)
        {
            up = Vector3::UnitZ;
        }
    }

    SetViewMatrix(Matrix4::CreateLookAt(eye, at, up));
}

} // namespace toy
