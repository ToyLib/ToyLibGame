//======================================================================
// FixedCameraComponent.h
//======================================================================
#pragma once

#include "Camera/CameraComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//======================================================================
// FixedCameraComponent
//
//  ・位置 / 注視点を手動でセットするだけの定点カメラ
//  ・追従や入力処理は一切しない
//  ・Scene の固定アングル、イベントシーン、タイトル画面向け
//
//  使い方
//  ------------------------------------------------------------
//    auto* cam = actor->CreateComponent<toy::FixedCameraComponent>();
//    cam->LookAt(Vector3(0, 1.5f, -10), Vector3(0, 1.5f, 0));
//    app->GetCameraManager()->SetActiveCamera(cam);
//======================================================================
class FixedCameraComponent : public CameraComponent
{
public:
    explicit FixedCameraComponent(class Actor* owner, int updateOrder = 200);

    //------------------------------------------------------------------
    // CameraComponent overrides
    //------------------------------------------------------------------
    void UpdateCamera(float deltaTime) override;

    //------------------------------------------------------------------
    // 位置 / 注視点 / 上方向
    //------------------------------------------------------------------
    void LookAt(const Vector3& eye, const Vector3& target);

    void SetEyePosition(const Vector3& eye) { mCameraPosition = eye; }
    void SetTarget(const Vector3& target)   { mCameraTarget   = target; }
    void SetUpVector(const Vector3& up)     { mUpVector       = up; }

    const Vector3& GetUpVector() const { return mUpVector; }

private:
    Vector3 mUpVector { Vector3::UnitY };
};

} // namespace toy
