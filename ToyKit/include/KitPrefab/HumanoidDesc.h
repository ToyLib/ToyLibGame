#pragma once

#include "Utils/MathUtil.h"

#include <cstdint>
#include <string>

namespace toy::kit {

//=============================================================================
// HumanoidDesc
//  Humanoid Prefab の構築情報（設計方針 5/16）。
//=============================================================================
struct HumanoidDesc
{
    // メッシュ
    std::string model;
    float       scale         = 1.0f;
    float       yawOffsetDeg  = 0.0f;
    bool        toonRender    = false;
    float       contourFactor = 1.0f;
    Vector3     contourColor  = Vector3(0.2f, 0.2f, 0.2f);

    // コライダー
    Vector3  colliderOffset = Vector3::Zero;
    Vector3  colliderScale  = Vector3::One;
    uint32_t colliderFlags  = 0;

    // 重力
    bool useGravity       = true;
    bool enableGroundPose = false;

    // 索敵（ロックオン候補探索）
    float sensorFovDeg           = 60.0f;
    float sensorMaxDist          = 40.0f;
    float sensorNearOverrideDist = 20.0f;

    // ロック解除条件
    float lockBreakDist    = 35.0f;
    float lockLostGraceSec = 0.7f;

    // 足音（空文字なら無効）
    std::string footstepSound;

    // カメラ
    bool freezeCameraYInAir = true;
};

} // namespace toy::kit
