#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {

//======================================================================
// CameraComponent
//   - アクターに取り付けてカメラ挙動を制御するコンポーネント
//   - Follow / Orbit / FPS などの派生クラスが実際の挙動を実装する
//   - Update() ではカメラ座標を更新し、Renderer に View 行列を反映
//======================================================================
class CameraComponent : public Component
{
public:
    CameraComponent(class Actor* owner, int updateOrder = 200);
    void Update(float deltaTime) override;
    virtual void UpdateCamera(float deltaTime) {}
    
    bool GetIsEnbale() const { return mIsInable; }
    void SetIsEnable(bool enable) { mIsInable = enable; }

protected:
    //----------------------------------------------------------------------
    // カメラのワールド座標（派生クラス側で更新する）
    //----------------------------------------------------------------------
    Vector3 mCameraPosition;

    // Renderer に View 行列を渡す
    void SetViewMatrix(const Matrix4& view);

    // カメラ位置を設定（内部保持用）
    void SetCameraPosition(const Vector3& pos);

    //----------------------------------------------------------------------
    // カメラ位置計算用の仮想アクター
    //   - カメラの向き計算や LookAt 用のヘルパーとして使用
    //   - メインアクターとは独立している
    //----------------------------------------------------------------------
    std::unique_ptr<class Actor> mCameraActor;
    
    bool mIsInable;
};

} // namespace toy
