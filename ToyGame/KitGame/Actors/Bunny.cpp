#include "Bunny.h"

namespace {

//=============================================================================
// Desc（構築レシピ）を作る
//  Monsters/Big/Bunny.gltf のクリップ順は Ninja.gltf と同じ（同アセットパック）:
//  0 Death, 1 Duck, 2 HitReact, 3 Idle, 4 Jump, 5 Jump_Idle, 6 Jump_Land,
//  7 No, 8 Punch, 9 Run, 10 Walk, 11 Wave, 12 Weapon, 13 Yes
//=============================================================================
toy::kit::HumanoidDesc MakeBunnyDesc()
{
    toy::kit::HumanoidDesc desc;
    desc.model        = "Monsters/Big/Bunny.gltf";
    desc.scale        = 1.0f;
    desc.yawOffsetDeg = 180.0f; // モデルの正面がローカル-Z向きで作られているため、+Z(GetForward)に揃える補正

    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                        | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.maxHp = 30; // 動作確認用の仮値

    desc.displayName = "ウサギ団";
    desc.fontPath    = "Font/rounded-mplus-1c-bold.ttf";
    desc.nameYOffset = 4.0f;
    
    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    desc.enableVision     = true;
    desc.visionFovDeg     = 60.0f;
    desc.visionMaxDist    = 15.0f;
    desc.visionTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

// Noriko と同じ FleeBehavior（見つかったら逃げる）用パラメータ
toy::kit::FleeBehaviorDesc MakeBunnyFleeDesc()
{
    toy::kit::FleeBehaviorDesc desc;
    desc.loseDistance = 30.0f; // visionMaxDist より少し大きめにしてヒステリシスを持たせる
    desc.moveSpeed    = 6.0f;
    desc.idleAnim     = 3; // Idle
    desc.fleeAnim     = 9; // Run
    return desc;
}

// Wolf/Shiro と同じ ChaseBehavior（見つかったら追いかける）用パラメータ
toy::kit::ChaseBehaviorDesc MakeBunnyChaseDesc()
{
    toy::kit::ChaseBehaviorDesc desc;
    desc.loseDistance = 25.0f;
    desc.moveSpeed    = 8.0f;
    desc.stopRange    = 4.0f;
    desc.idleAnim     = 3; // Idle
    desc.chaseAnim    = 9; // Run
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Skirmisher> MakeBunny(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                       BunnyBehaviorType behaviorType)
{
    if (behaviorType == BunnyBehaviorType::Chase)
    {
        return MakeSkirmisher(app, position, target, MakeBunnyDesc(), MakeBunnyChaseDesc());
    }
    return MakeSkirmisher(app, position, target, MakeBunnyDesc(), MakeBunnyFleeDesc());
}
