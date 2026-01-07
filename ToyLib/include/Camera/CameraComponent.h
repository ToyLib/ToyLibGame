//======================================================================
// CameraComponent
//
//  ・Actor に取り付けて「カメラ挙動」を実装する基底クラス
//  ・Follow / Orbit / FPS など各種カメラがこれを継承する
//
//  ・Update() では：
//        - CameraManager から「自分がアクティブか」を確認
//        - アクティブなら UpdateCamera() を呼び出す
//        - Renderer へ View 行列を送るのは UpdateCamera() 側の責任
//
//  ・CameraManager との関係：
//        - SetActiveCamera() 時に OnActivated() が呼ばれる
//        - prevPos / prevTarget に
//              「直前まで表示していたカメラの情報」
//          が渡されるため、
//              → そこからスムーズに遷移したい場合に活用する
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

    //------------------------------------------------------------------
    // Component::Update()
    //
    //  ・CameraManager で「自分がアクティブかどうか」を判定
    //  ・アクティブ && Enabled の場合のみ UpdateCamera() を呼ぶ
    //------------------------------------------------------------------
    void Update(float deltaTime) override;

    //------------------------------------------------------------------
    // 各カメラ固有の挙動
    //
    //  ・派生クラスで「位置更新」「ターゲット更新」
    //  ・最後に View 行列を SetViewMatrix() で登録する想定
    //------------------------------------------------------------------
    virtual void UpdateCamera(float deltaTime) {}

    //------------------------------------------------------------------
    // 有効 / 無効
    //
    //  ・CameraManager がアクティブ切替時に制御
    //  ・無効の場合は Update() でも処理されない
    //------------------------------------------------------------------
    bool GetIsEnabled() const { return mIsEnabled; }
    void SetIsEnabled(bool enable) { mIsEnabled = enable; }

    //------------------------------------------------------------------
    // 現在のカメラ位置 / 注視点
    //
    //  ・CameraManager が
    //        prevPos / prevTarget として取得するために使用
    //------------------------------------------------------------------
    const Vector3& GetCameraPosition() const { return mCameraPosition; }
    const Vector3& GetCameraTarget()   const { return mCameraTarget;   }

    //------------------------------------------------------------------
    // OnActivated()
    //
    //  ・「このカメラがアクティブになった瞬間」に CameraManager が呼ぶ
    //  ・引数：
    //        prevPos    = 直前まで使われていたカメラ位置
    //        prevTarget = 直前まで使われていた注視点
    //
    //  ・スムーズな遷移をしたい場合は、
    //        - これを使って初期状態を合わせる
    //        - そこから Lerp / Spring などで理想カメラへ寄せる
    //------------------------------------------------------------------
    virtual void OnActivated(const Vector3& prevPos,
                             const Vector3& prevTarget)
    {
        // デフォルトは何もしない
    }

protected:
    //------------------------------------------------------------------
    // 派生クラスが毎フレーム更新する値
    //
    //  ・CameraManager はこの値を参照して
    //      prevPos / prevTarget を取得する
    //------------------------------------------------------------------
    Vector3 mCameraPosition{ Vector3::Zero };
    Vector3 mCameraTarget{ Vector3::Zero };

    //------------------------------------------------------------------
    // Renderer に View 行列を渡す
    //------------------------------------------------------------------
    void SetViewMatrix(const Matrix4& view);

    //------------------------------------------------------------------
    // カメラ位置の内部保持用ヘルパ
    //------------------------------------------------------------------
    void SetCameraPosition(const Vector3& pos)
    {
        mCameraPosition = pos;
    }

    bool mIsEnabled{ true };
};

} // namespace toy
