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

    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    desc.enableVision     = true;
    desc.visionFovDeg     = 60.0f;
    desc.visionMaxDist    = 15.0f;
    desc.visionTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

toy::kit::FleeBehaviorDesc MakeBunnyFleeDesc()
{
    toy::kit::FleeBehaviorDesc desc;
    desc.loseDistance = 30.0f; // visionMaxDist より少し大きめにしてヒステリシスを持たせる
    desc.moveSpeed    = 6.0f;
    desc.idleAnim     = 3; // Idle
    desc.fleeAnim     = 9; // Run
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Skirmisher> MakeBunny(toy::Application* app, const Vector3& position, toy::kit::Prefab* target)
{
    return MakeSkirmisher(app, position, target, MakeBunnyDesc(), MakeBunnyFleeDesc());
}
