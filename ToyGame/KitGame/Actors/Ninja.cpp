#include "Ninja.h"

namespace {

//=============================================================================
// Desc（構築レシピ）を作る
//  Monsters/Big/Ninja.gltf のクリップ順: 0 Death, 1 Duck, 2 HitReact, 3 Idle,
//  4 Jump, 5 Jump_Idle, 6 Jump_Land, 7 No, 8 Punch, 9 Run, 10 Walk, 11 Wave,
//  12 Weapon, 13 Yes
//=============================================================================
toy::kit::HumanoidDesc MakeNinjaDesc()
{
    toy::kit::HumanoidDesc desc;
    desc.model        = "Monsters/Big/Ninja.gltf";
    desc.scale        = 1.0f;
    desc.yawOffsetDeg = 180.0f; // モデルの正面がローカル-Z向きで作られているため、+Z(GetForward)に揃える補正

    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                        | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    // 視界に Player が入ったら選択された Behavior（Flee/Chase）が動き出す
    desc.enableVision     = true;
    desc.visionFovDeg     = 60.0f;
    desc.visionMaxDist    = 15.0f;
    desc.visionTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

// Noriko と同じ FleeBehavior（見つかったら逃げる）用パラメータ
toy::kit::FleeBehaviorDesc MakeNinjaFleeDesc()
{
    toy::kit::FleeBehaviorDesc desc;
    desc.loseDistance = 30.0f; // visionMaxDist より少し大きめにしてヒステリシスを持たせる
    desc.moveSpeed    = 6.0f;
    desc.idleAnim     = 3;  // Idle
    desc.fleeAnim     = 9;  // Run
    return desc;
}

// Wolf/Shiro と同じ ChaseBehavior（見つかったら追いかける）用パラメータ
toy::kit::ChaseBehaviorDesc MakeNinjaChaseDesc()
{
    toy::kit::ChaseBehaviorDesc desc;
    desc.loseDistance = 25.0f;
    desc.moveSpeed   = 8.0f;
    desc.stopRange   = 4.0f;
    desc.idleAnim    = 3; // Idle
    desc.chaseAnim   = 9; // Run
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
// MakeNinja — Factory 関数
//  behaviorType で Noriko の FleeBehavior と Wolf/Shiro の ChaseBehavior の
//  どちらを使うかを選ぶ。Behavior 本体はどちらも ToyKit 側の汎用実装で、
//  ここでは Desc の組み立てと SetTarget だけを行う。
//-----------------------------------------------------------------------------
std::unique_ptr<Ninja> MakeNinja(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                  Ninja::BehaviorType behaviorType)
{
    std::unique_ptr<toy::kit::IBehavior> behavior;
    switch (behaviorType)
    {
    case Ninja::BehaviorType::Flee:
    {
        auto flee = std::make_unique<toy::kit::FleeBehavior>(MakeNinjaFleeDesc());
        flee->SetTarget(target);
        behavior = std::move(flee);
        break;
    }
    case Ninja::BehaviorType::Chase:
    {
        auto chase = std::make_unique<toy::kit::ChaseBehavior>(MakeNinjaChaseDesc());
        chase->SetTarget(target);
        behavior = std::move(chase);
        break;
    }
    }

    auto ninja = std::make_unique<Ninja>(app, std::move(behavior), MakeNinjaDesc());
    ninja->GetBody().SetPosition(position);
    return ninja;
}
