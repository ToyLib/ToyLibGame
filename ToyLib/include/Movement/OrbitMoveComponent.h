#pragma once

#include "Movement/MoveComponent.h"

namespace toy {

class OrbitMoveComponent : public MoveComponent
{
public:
    OrbitMoveComponent(class Actor* owner, int updateOrder = 10);

    void Update(float deltaTime) override;
    void ProcessInput(const struct InputState& state) override;

    void SetCenterActor(class Actor* center) { mCenterActor = center; }

    // 歩く（公転）スピード [m/s]
    void SetOrbitLinearSpeed(float s) { mOrbitLinearSpeed = s; }

    // 近づく/離れる速度 [m/s]
    void SetZoomSpeed(float s) { mZoomSpeed = s; }

    void SetRadiusRange(float minR, float maxR)
    {
        mMinRadius = minR;
        mMaxRadius = maxR;
    }

private:
    class Actor* mCenterActor;

    float mOrbitRadius;        // 現在の距離
    float mOrbitLinearSpeed;   // 公転の線速度（「歩く速さ」）
    float mZoomSpeed;          // 半径方向の変化速度（近づく/離れる）

    float mMinRadius;
    float mMaxRadius;

    // 入力値（-1.0f 〜 +1.0f）
    float mOrbitInput;         // 左右（公転）
    float mZoomInput;          // 上下（近づく/離れる）

    float mCurrentAngle;       // 参考用・デバッグ用（必須ではない）
};

} // namespace toy
