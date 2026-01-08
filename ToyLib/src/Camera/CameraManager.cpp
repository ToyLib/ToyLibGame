#include "Camera/CameraManager.h"
#include "Camera/CameraComponent.h"

namespace toy {

void CameraManager::RegisterCamera(CameraComponent *cam)
{
    if (!cam)
    {
        return;
    }
    mCameras.push_back(cam);
}

void CameraManager::UnregisterCamera(CameraComponent* cam)
{
    if (!cam)
    {
        return;
    }
    
    auto it = std::find(mCameras.begin(), mCameras.end(), cam);
    if (it != mCameras.end())
    {
        // アクティブカメラが消える場合は無効化
        if (mActiveCamera == cam)
        {
            mActiveCamera = nullptr;
        }

        mCameras.erase(it);
    }
}

//------------------------------------------------------------------
// SetActiveCamera
//
//  ・アクティブカメラを切替える
//  ・前カメラ → 次カメラへ
//      「最後に描画していた位置/注視点」を引き継ぐため
//      prevPos / prevTarget を渡す
//------------------------------------------------------------------
void CameraManager::SetActiveCamera(CameraComponent *next)
{
    // 既にアクティブなら何もしない
    if (mActiveCamera == next)
    {
        return;
    }

    Vector3 prevPos    = Vector3::Zero;
    Vector3 prevTarget = Vector3::Zero;

    // 旧カメラの情報を取得
    if (mActiveCamera)
    {
        prevPos    = mActiveCamera->GetCameraPosition();
        prevTarget = mActiveCamera->GetCameraTarget();

        // 旧カメラは無効化
        mActiveCamera->SetIsEnabled(false);
    }

    // 切替
    mActiveCamera = next;

    if (mActiveCamera)
    {
        // ★重要：
        //   切替時に「前カメラの位置/注視点」を渡す
        //   → Follow / Orbit / FPS などが
        //      OnActivated() 内でスムーズ遷移を実装できる
        mActiveCamera->OnActivated(prevPos, prevTarget);

        // 新カメラは有効化
        mActiveCamera->SetIsEnabled(true);
    }
}

} // namespace toy
