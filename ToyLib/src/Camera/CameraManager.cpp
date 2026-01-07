#include "Camera/CameraManager.h"
#include "Camera/CameraComponent.h"

namespace toy {

void CameraManager::RegisterCamera(CameraComponent *cam)
{
    if (!cam) return;
    mCameras.push_back(cam);
}


void CameraManager::UnregisterCamera(CameraComponent* cam)
{
    if (!cam) return;
    auto it = std::find(mCameras.begin(), mCameras.end(), cam);
    if (it != mCameras.end())
    {
        if (mActiveCamera == cam)
        {
            mActiveCamera = nullptr;
        }
        mCameras.erase(it);
    }
}

void CameraManager::SetActiveCamera(CameraComponent *next)
{
    if (mActiveCamera == next)
    {
        return;
    }

    Vector3 prevPos    = Vector3::Zero;
    Vector3 prevTarget = Vector3::Zero;

    if (mActiveCamera)
    {
        prevPos    = mActiveCamera->GetCameraPosition();
        prevTarget = mActiveCamera->GetCameraTarget();
        mActiveCamera->SetIsEnabled(false);
    }

    mActiveCamera = next;

    if (mActiveCamera)
    {
        // 前カメラの位置/ターゲットを渡す
        mActiveCamera->OnActivated(prevPos, prevTarget);
        mActiveCamera->SetIsEnabled(true);
    }
}

} // namespace toy
