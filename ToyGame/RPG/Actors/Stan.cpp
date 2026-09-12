#include "Stan.h"

#include <cmath>

namespace {

toy::kit::HumanoidDesc MakeStanDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model         = "stan.gltf";
    desc.yawOffsetDeg  = 180.0f;
    desc.isRightHanded = true;
    desc.toonRender    = true;

    desc.colliderScale = Vector3(0.5f, 1.0f, 0.6f);
    desc.colliderFlags = toy::C_WALL | toy::C_ENEMY_TEAM | toy::C_HURTBOX | toy::C_FOOT | toy::C_GROUND;

    return desc;
}

//=============================================================================
// FollowBehavior
//  ターゲットの方を向いて、一定距離より離れていれば近づくだけの最小限の追従。
//  ChaseBehavior と違い索敵/見失いの概念はなく、常にターゲットへ向かう
//  （旧 FollowMoveComponent 相当）。今のところ Stan 専用（他でも使うように
//  なったら ToyKit 側の汎用 Behavior に昇格する）。
//=============================================================================
class FollowBehavior : public toy::kit::IBehavior
{
public:
    void SetTarget(toy::kit::Prefab* target) { mTarget = target; }

    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = &body;
    }

    void OnUpdate(toy::kit::Prefab& /*body*/, float deltaTime) override
    {
        if (!mTarget) return;
        MoveTowardTarget(deltaTime);
        LookAtTarget();
    }

private:
    void MoveTowardTarget(float dt)
    {
        const Vector3& self   = mBody->GetPosition();
        const Vector3& target = mTarget->GetPosition();
        const float dx   = target.x - self.x;
        const float dz   = target.z - self.z;
        const float dist = sqrtf(dx * dx + dz * dz);
        if (dist < mFollowDistance) return;

        const float step = mMoveSpeed * dt / dist;
        mBody->SetPosition(Vector3(self.x + dx * step,
                                   self.y, // Y は重力に任せる
                                   self.z + dz * step));
    }

    void LookAtTarget()
    {
        const Vector3& self   = mBody->GetPosition();
        const Vector3& target = mTarget->GetPosition();
        const float dx = target.x - self.x;
        const float dz = target.z - self.z;
        if (dx * dx + dz * dz < 0.01f) return;

        const float angle = atan2f(-dx, -dz);
        mBody->SetRotation(Quaternion(Vector3::UnitY, angle));
    }

    toy::kit::Prefab* mBody   = nullptr;
    toy::kit::Prefab* mTarget = nullptr;

    float mFollowDistance = 3.0f;
    float mMoveSpeed      = 10.0f;
};

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Stan> MakeStan(toy::Application* app, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<FollowBehavior>();
    behavior->SetTarget(target);

    auto stan = std::make_unique<Stan>(app, std::move(behavior), MakeStanDesc());
    stan->GetBody().SetPosition(Vector3(-3.0f, 0.0f, 10.0f));
    stan->GetBody().SetScale(0.5f);
    stan->GetBody().SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(-30.0f)));
    return stan;
}
