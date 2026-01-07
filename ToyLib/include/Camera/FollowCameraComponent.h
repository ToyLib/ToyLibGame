#pragma once

#include "Camera/CameraComponent.h"

namespace toy {

//======================================================================
// SpringSettings
//
//  ・カメラ追従に使うバネ挙動のパラメータ
//      - Stiffness    : バネ定数 k（大きいほどキビキビ追従）
//      - DampingRatio : 減衰比 ζ
//           ζ = 1.0 で「臨界減衰」
//           → 振動せず、できるだけ速く目標値に収束
//======================================================================
struct SpringSettings
{
    float Stiffness    = 2000.0f;  // バネ定数 k
    float DampingRatio = 1.0f;     // 減衰比 ζ（目安：1.0）
};

//======================================================================
// UpdateSpring
//
//  ・位置/速度に対して 2 次系バネ挙動を 1 ステップ分適用する
//  ・質量 m = 1 として、運動方程式：
//
//        a = -k x - c v
//        c = 2 ζ √k
//
//    を用いた標準的な 2nd Order Dynamics
//
//  引数：
//      position : 現在位置（更新される）
//      velocity : 現在速度（更新される）
//      target   : 目標位置
//======================================================================
inline void UpdateSpring(
    Vector3& position,
    Vector3& velocity,
    const Vector3& target,
    const SpringSettings& settings,
    float deltaTime)
{
    const float k = settings.Stiffness;
    const float z = settings.DampingRatio;

    // 減衰係数 c = 2 ζ √k
    const float c = 2.0f * z * Math::Sqrt(k);

    // x = current - target
    Vector3 diff = position - target;

    // a = -k x - c v
    Vector3 accel = -k * diff - c * velocity;

    velocity += accel * deltaTime;
    position += velocity * deltaTime;
}

//======================================================================
// FollowCameraComponent
//
//  ・所有 Actor の背後かつ上空に配置される 3rd Person カメラ
//  ・「理想位置」を基準にスプリングで追従することで、
//      - ふわっとした追従感
//      - ガクつきやビクビクを抑えた自然な挙動
//    を実現する
//
//  ・LookAt のターゲット：
//      - 所有 Actor の位置＋前方方向へ少しオフセット
//======================================================================
class FollowCameraComponent : public CameraComponent
{
public:
    explicit FollowCameraComponent(Actor* owner);

    //------------------------------------------------------------------
    // カメラ更新
    //
    //  ・所有 Actor から「理想カメラ位置」を算出
    //  ・スプリングで mActualPos を理想位置へ追従させる
    //  ・最終的な位置／注視点から View 行列を設定
    //------------------------------------------------------------------
    void UpdateCamera(float deltaTime) override;

    //------------------------------------------------------------------
    // SnapToIdeal
    //
    //  ・スプリングを使わず、一瞬で理想位置へ移動させる
    //  ・テレポート直後や、初期配置などに利用
    //------------------------------------------------------------------
    void SnapToIdeal();
    
    //------------------------------------------------------------------
    // OnActivated
    //
    //  ・他のカメラからこのカメラに切り替わった瞬間に呼ばれる
    //  ・prevPos / prevTarget には「直前のカメラ」の情報が渡される
    //  ・Follow カメラでは、スプリングの開始位置を prevPos に合わせる
    //------------------------------------------------------------------
    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    //------------------------------------------------------------------
    // パラメータ設定
    //------------------------------------------------------------------
    void SetDistance(float horz, float vert)
    {
        mHorzDist = horz;
        mVertDist = vert;
    }

    void SetTargetDistance(float dist)
    {
        mTargetDist = dist;
    }

    void SetSpringSettings(const SpringSettings& s)
    {
        mSpring = s;
    }
    
private:
    //------------------------------------------------------------------
    // 所有 Actor から見た「理想のカメラ位置」を計算
    //
    //  ・位置から前方へ mHorzDist だけ後ろに下がり、
    //    mVertDist だけ上に持ち上げた位置
    //------------------------------------------------------------------
    Vector3 ComputeCameraPos() const;

private:
    // カメラの相対配置
    float mHorzDist;   // 所有 Actor から見た後方距離
    float mVertDist;   // 高さオフセット
    float mTargetDist; // LookAt の前方オフセット

    // スプリング設定
    SpringSettings mSpring;

    // スプリングによって補間される実際のカメラ位置／速度
    Vector3 mActualPos;
    Vector3 mVelocity;

    // 初回のみ SnapToIdeal() するためのフラグ
    bool mFirstUpdate;
};

} // namespace toy
