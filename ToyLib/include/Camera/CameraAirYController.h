#pragma once

#include "Utils/MathUtil.h"

namespace toy {

class Actor;
class GravityComponent;

//======================================================================
// CameraAirYController
//
//  空中中の Y 追従制御を共通化するためのヘルパー。
//  - 上昇中   : camera.y / target.y を固定（Hold）
//  - 落下中   : “見失いそうな時だけ” 追従（FallAssist）
//  - 着地後   : target 早め / camera 遅めで復帰（Recover）
//
//  使い方：
//    1) OnActivated / Snap などで Reset(owner, cameraPos, target)
//    2) UpdateCamera 内で
//         controller.Apply(owner, dt, cameraPos, target);
//       を呼ぶ（cameraPos/target の y を上書きする）
//======================================================================
class CameraAirYController
{
public:
    CameraAirYController() = default;

    void SetEnabled(bool enable)
    {
        mEnabled = enable;
    }

    bool IsEnabled() const
    {
        return mEnabled;
    }

    void SetRecoverSeconds(float targetSec, float cameraSec);
    void SetFallAssistSeconds(float targetSec, float cameraSec);
    void SetFallOutOfViewThreshold(float thresholdY, float hysteresisY);

    // owner/cameraPos/target の現在値で内部状態を初期化（切替直後、初回スナップ時）
    void Reset(const Actor* owner,
               const Vector3& cameraPos,
               const Vector3& target);

    // owner の Gravity を参照して、cameraPos/target の y を自然に制御する
    void Apply(const Actor* owner,
               float dt,
               Vector3& ioCameraPos,
               Vector3& ioTarget);

private:
    enum class Mode
    {
        None,
        Hold,
        FallAssist,
        Recover
    };

private:
    bool mEnabled { false };

    Mode mMode    { Mode::None };

    // Hold/Recover の保持値
    float mHoldCamY    { 0.0f };
    float mHoldTargetY { 0.0f };

    // 接地状態の履歴
    bool mPrevGrounded { true };

    // 落下時の “見失い判定”
    float mFallOutOfViewThresholdY  { 1.8f };
    float mFallOutOfViewHysteresisY { 0.4f };
    bool  mFallAssistActive         { false };

    // 追従速度（95%到達秒）
    float mFallAssistTargetSeconds { 0.18f };
    float mFallAssistCameraSeconds { 0.45f };

    float mRecoverTargetSeconds { 0.15f };
    float mRecoverCameraSeconds { 0.30f };
};

} // namespace toy
