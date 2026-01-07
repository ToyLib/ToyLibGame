//======================================================================
// CameraComponent.h
//======================================================================
#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include <memory>

namespace toy {

class CameraComponent : public Component
{
public:
    CameraComponent(class Actor* owner, int updateOrder = 200);
    virtual ~CameraComponent();

    void Update(float deltaTime) override;

    // 各カメラ挙動はここで実装
    virtual void UpdateCamera(float deltaTime) {}

    // 有効/無効
    bool GetIsEnabled() const { return mIsEnabled; }
    void SetIsEnabled(bool enable) { mIsEnabled = enable; }

    // 現在のカメラ位置/ターゲット（Manager が参照する）
    const Vector3& GetCameraPosition() const { return mCameraPosition; }
    const Vector3& GetCameraTarget()   const { return mCameraTarget;   }

    // カメラが「アクティブになった瞬間」に呼ぶフック
    // （前カメラの現在位置/ターゲットが渡される）
    virtual void OnActivated(const Vector3& prevPos,
                             const Vector3& prevTarget)
    {
        // デフォルトは何もしない
    }

protected:
    //----------------------------------------------------------------------
    // カメラのワールド座標と注視点（派生クラスが毎フレーム更新）
    //----------------------------------------------------------------------
    Vector3 mCameraPosition{ Vector3::Zero };
    Vector3 mCameraTarget{ Vector3::Zero };

    // Renderer に View 行列を渡す
    void SetViewMatrix(const Matrix4& view);

    // 内部保持用
    void SetCameraPosition(const Vector3& pos)
    {
        mCameraPosition = pos;
    }


    bool mIsEnabled{ true };
};

} // namespace toy
