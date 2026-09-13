#include "Shiro.h"

namespace {

toy::kit::HumanoidDesc MakeShiroDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model                     = "Enemy/Shiro.glb";
    desc.scale                     = 3.0f;
    desc.yawOffsetDeg              = 180.0f; // モデルの正面がローカル-Z向きで作られているため、+Z(GetForward)に揃える補正
    desc.toonRender                = true;
    desc.contourColor              = Vector3(0.3f, 0.3f, 0.35f);
    desc.meshOffset                = Vector3(0.0f, 0.0f, 0.8f);
    desc.cancelRootTranslationBone = "Hip";

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.5f, 1.0f, 0.3f);
    desc.colliderFlags  = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                         | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.displayName = "SHIRO";
    desc.fontPath    = "rounded-mplus-1c-bold.ttf";
    desc.nameYOffset = 4.0f;

    desc.candidateTexture = "candidate.png";
    desc.lockedTexture    = "lockon.png";

    // 視界に Player が入ったら ChaseBehavior が追跡を始める
    desc.enableVision     = true;
    desc.visionFovDeg     = 100.0f;
    desc.visionMaxDist    = 40.0f;
    desc.visionTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

toy::kit::ChaseBehaviorDesc MakeShiroChaseDesc()
{
    toy::kit::ChaseBehaviorDesc desc;
    desc.loseDistance = 40.0f;
    desc.moveSpeed   = 6.0f;
    desc.stopRange   = 4.0f;

    desc.idleAnim  = 0; // ANIM_IDLE（Shiro.glb のクリップ順）
    desc.chaseAnim = 2; // ANIM_RUN
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Shiro> MakeShiro(toy::Application* app, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<toy::kit::ChaseBehavior>(MakeShiroChaseDesc());
    behavior->SetTarget(target);

    return std::make_unique<Shiro>(app, std::move(behavior), MakeShiroDesc());
}
