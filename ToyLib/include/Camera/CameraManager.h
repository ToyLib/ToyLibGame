//======================================================================
// CameraManager
//
//  ・複数の CameraComponent を登録・管理する
//  ・「現在アクティブなカメラ」は常に 1 つだけ
//  ・カメラ切替時：
//       - 直前カメラの位置/注視点を次カメラへ渡す
//       - 必要なら OnActivated() 内でスムーズな遷移を実装可能
//
//  ※ CameraComponent 側の想定
//     - RegisterCamera() / UnregisterCamera() は
//       CameraComponent の ctor / dtor から呼ばれる想定
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

    //------------------------------------------------------------------
    // カメラ登録
    //  - CameraComponent のコンストラクタから呼ぶ想定
    //------------------------------------------------------------------
    void RegisterCamera(CameraComponent* cam);

    //------------------------------------------------------------------
    // カメラ登録解除
    //  - CameraComponent のデストラクタから呼ぶ想定
    //  - アクティブ中のカメラが消える場合は Active をクリア
    //------------------------------------------------------------------
    void UnregisterCamera(CameraComponent* cam);

    //------------------------------------------------------------------
    // アクティブカメラ切替
    //
    //  ・同じカメラを再指定した場合は何もしない
    //  ・前カメラの
    //        - 位置 (CameraPosition)
    //        - 注視点 (CameraTarget)
    //    を取得して、
    //        → 次カメラの OnActivated(prevPos, prevTarget)
    //    に渡す
    //
    //  ・前カメラは Disable
    //  ・新カメラは Enable
    //------------------------------------------------------------------
    void SetActiveCamera(CameraComponent* next);

    //------------------------------------------------------------------
    // 現在アクティブなカメラを取得
    //------------------------------------------------------------------
    CameraComponent* GetActiveCamera() const { return mActiveCamera; }

    //------------------------------------------------------------------
    // 登録済みカメラ一覧（デバッグ用途など）
    //------------------------------------------------------------------
    const std::vector<CameraComponent*>& GetAllCameras() const { return mCameras; }

private:
    std::vector<CameraComponent*> mCameras{};
    CameraComponent* mActiveCamera { nullptr };
};

} // namespace toy
