#include "Wolf.h"

namespace {

toy::kit::HumanoidDesc MakeWolfDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model         = "Enemy/wolf.gltf";
    desc.meshDrawOrder = 1000;

    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                        | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.candidateTexture = "target_scope.png"; // ロック中は専用スプライト無し（元仕様）

    desc.ambientSound       = "growling.wav";
    desc.ambientSoundVolume = 0.2f;

    desc.speechText     = "Bow \nwow !";
    desc.speechFontPath = "rounded-mplus-1c-bold.ttf";
    desc.speechColor    = Vector3(1.0f, 0.0f, 0.0f);

    return desc;
}

toy::kit::ChaseBehaviorDesc MakeWolfChaseDesc()
{
    toy::kit::ChaseBehaviorDesc desc;
    desc.detectRange = 30.0f;
    desc.moveSpeed   = 8.0f;
    desc.stopRange   = 4.0f;

    desc.idleAnim  = 2; // ANIM_IDLE（wolf.gltf のクリップ順）
    desc.chaseAnim = 3; // ANIM_RUN
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Wolf> MakeWolf(toy::Application* app, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<toy::kit::ChaseBehavior>(MakeWolfChaseDesc());
    behavior->SetTarget(target);

    auto wolf = std::make_unique<Wolf>(app, std::move(behavior), MakeWolfDesc());
    wolf->GetBody().SetScale(3.0f);
    return wolf;
}
