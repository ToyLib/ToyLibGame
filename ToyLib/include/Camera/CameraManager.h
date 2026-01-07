//======================================================================
// CameraManager
//   - 複数カメラの登録/切替を管理
//   - 「いまのアクティブカメラ」を 1 つだけ持つ
//======================================================================
#pragma once

#include "Utils/MathUtil.h"
#include <vector>
#include <algorithm>

namespace toy {

class CameraComponent;

class CameraManager
{
public:
    CameraManager() = default;
    ~CameraManager() = default;

    // カメラ登録（CameraComponent のコンストラクタから呼ぶ想定）
    void RegisterCamera(CameraComponent* cam);


    // カメラ登録解除（CameraComponent のデストラクタから呼ぶ想定）
    void UnregisterCamera(CameraComponent* cam);

    // アクティブカメラを切り替える
    void SetActiveCamera(CameraComponent* next);

    CameraComponent* GetActiveCamera() const { return mActiveCamera; }

    // デバッグ用に全カメラ取得、なども必要ならそのうち追加
    const std::vector<CameraComponent*>& GetAllCameras() const { return mCameras; }

private:
    std::vector<CameraComponent*> mCameras;
    CameraComponent* mActiveCamera{ nullptr };
};

} // namespace toy
