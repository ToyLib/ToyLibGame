#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/IRenderer.h"
#include "Engine/Core/Application.h"

namespace toy {

CameraComponent::CameraComponent(Actor* owner, int updateOrder)
    : Component(owner, updateOrder)
{
    //------------------------------------------------------------------
    // CameraManager へ登録
    //
    //  ・各 CameraComponent は自動的に Manager に登録される
    //  ・登録解除はデストラクタで行う
    //------------------------------------------------------------------
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
    //------------------------------------------------------------------
    // CameraManager から登録解除
    //------------------------------------------------------------------
    if (auto* app = GetOwner()->GetApp())
    {
        if (auto* mgr = app->GetCameraManager())
        {
            mgr->UnregisterCamera(this);
        }
    }
}



//======================================================================
// Update
//
//  ・カメラの共通 Update エントリポイント
//  ・挙動の本体は UpdateCamera()（派生クラス側）
//
//  ・処理条件：
//      - Enabled である
//      - CameraManager の ActiveCamera が自分である
//
//  ・条件を満たしたときのみ UpdateCamera() を呼ぶ
//======================================================================
void CameraComponent::Update(float deltaTime)
{
    if (!mIsEnabled)
    {
        return;
    }

    // Application → CameraManager へ問い合わせ
    if (auto* app = GetOwner()->GetApp())
    {
        if (auto* mgr = app->GetCameraManager())
        {
            // 自分がアクティブでなければ何もしない
            if (mgr->GetActiveCamera() != this)
            {
                return;
            }
        }
    }

    // 実際のカメラ処理（派生クラス）
    UpdateCamera(deltaTime);
}


//======================================================================
// SetViewMatrix
//
//  ・Renderer へ View 行列を登録する共通ヘルパ
//  ・派生カメラは、UpdateCamera() の最後でこれを呼ぶ想定
//======================================================================
void CameraComponent::SetViewMatrix(const Matrix4 &view)
{
    if (auto* app = GetOwner()->GetApp())
    {
        if (auto* renderer = app->GetRenderer())
        {
            renderer->SetViewMatrix(view);
        }
    }
}

} // namespace toy
