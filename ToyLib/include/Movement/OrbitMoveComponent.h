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

    // 入力からセットされる速度の最大値（[unit/sec]）
    void SetMaxOrbitSpeed(float s)  { mMaxOrbitSpeed  = s; } // 公転（円周方向）
    void SetMaxRadialSpeed(float s) { mMaxRadialSpeed = s; } // 近づく/離れる（半径方向）

    void SetRadiusRange(float minR, float maxR)
    {
        mMinRadius = minR;
        mMaxRadius = maxR;
    }

private:
    class Actor* mCenterActor;

    float mOrbitRadius;        // 現在の距離
    float mMinRadius;
    float mMaxRadius;

    // ProcessInput で mForwardSpeed / mRightSpeed に入れるときの上限値
    float mMaxOrbitSpeed;      // 円周方向の最大速度
    float mMaxRadialSpeed;     // 半径方向の最大速度

    float mCurrentAngle;       // デバッグ用（なくてもOK）
};

} // namespace toy
