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
    int         meshDrawOrder = 100;
    float       scale         = 1.0f;
    float       yawOffsetDeg  = 0.0f;
    bool        toonRender    = false;
    float       contourFactor = 1.0f;
    Vector3     contourColor  = Vector3(0.2f, 0.2f, 0.2f);
    Vector3     meshOffset    = Vector3::Zero;

    // ルートモーションを打ち消したいボーン名（空なら何もしない）
    std::string cancelRootTranslationBone;

    // コライダー
    Vector3  colliderOffset = Vector3::Zero;
    Vector3  colliderScale  = Vector3::One;
    uint32_t colliderFlags  = 0;

    // 重力
    bool useGravity       = true;
    bool enableGroundPose = false;

    //-------------------------------------------------------------------
    // ロックオン戦闘（Field/Battle切換え・追従カメラ・索敵）
    //  プレイヤーが操作する Humanoid だけ true にする。
    //  NPC の Humanoid（索敵/追跡は Game Logic 側の AI が行う）では false のまま。
    //-------------------------------------------------------------------
    bool enableLockOnCombat = false;

    float sensorFovDeg           = 60.0f;
    float sensorMaxDist          = 40.0f;
    float sensorNearOverrideDist = 20.0f;

    float lockBreakDist    = 35.0f;
    float lockLostGraceSec = 0.7f;

    bool freezeCameraYInAir = true;

    // 足音（空文字なら無効）
    std::string footstepSound;

    // 名前ビルボード（空文字なら非表示）
    std::string displayName;
    std::string fontPath    = "Font/rounded-mplus-1c-bold.ttf";
    float       nameYOffset = 4.0f;
    Vector3     nameColor   = Vector3(1.0f, 0.0f, 0.0f);

    // ターゲット表示スプライト（空文字なら非表示）
    std::string candidateTexture;
    std::string lockedTexture;

    // アンビエントサウンド（空文字なら無効。唸り声など常時ループ再生）
    std::string ambientSound;
    float       ambientSoundVolume = 1.0f;

    // 常時表示テキスト（空文字なら非表示。吹き出し等）
    std::string speechText;
    std::string speechFontPath = "Font/rounded-mplus-1c-bold.ttf";
    Vector3     speechColor    = Vector3(1.0f, 1.0f, 1.0f);
};

} // namespace toy::kit
