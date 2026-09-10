#pragma once

#include "Utils/MathUtil.h"

#include <cstdint>
#include <string>

namespace toy::kit {

//=============================================================================
// CreatureDesc
//  Creature Prefab の構築情報（設計方針 5/16）。
//  JSON へシリアライズできる範囲（プリミティブ/配列/入れ子構造体）に収め、
//  実行時オブジェクトへの参照やランタイム状態は持たない。
//=============================================================================
struct CreatureDesc
{
    // メッシュ
    std::string model;
    float       scale         = 1.0f;
    float       yawOffsetDeg  = 0.0f;
    bool        toonRender    = false;
    float       contourFactor = 1.0f;

    // ルートモーションを打ち消したいボーン名（空なら何もしない）
    std::string cancelRootTranslationBone;

    // コライダー
    Vector3  colliderOffset = Vector3::Zero;
    Vector3  colliderScale  = Vector3::One;
    uint32_t colliderFlags  = 0;

    // 重力
    bool useGravity       = true;
    bool enableGroundPose = true;

    // 名前ビルボード（空文字なら非表示）
    std::string displayName;
    std::string fontPath    = "Font/rounded-mplus-1c-bold.ttf";
    float       nameYOffset = 4.0f;
    Vector3     nameColor   = Vector3(1.0f, 0.0f, 0.0f);

    // ターゲット表示スプライト（空文字なら非表示）
    std::string candidateTexture;
    std::string lockedTexture;
};

} // namespace toy::kit
