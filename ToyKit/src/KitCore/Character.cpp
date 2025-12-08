#include "KitCore/Character.h"
#include "Engine/Core/Actor.h"

namespace toy::kit {

Character::Character(toy::Actor* actor)
: mActor(actor)
{
}

Vector3 Character::GetPosition() const
{
    if (!mActor)
    {
        return Vector3::Zero;
    }
    return mActor->GetPosition();
}

void Character::SetPosition(const Vector3& pos)
{
    if (!mActor)
    {
        return;
    }
    mActor->SetPosition(pos);
}

void Character::AddWorldOffset(const Vector3& delta)
{
    if (!mActor)
    {
        return;
    }
    mActor->SetPosition(mActor->GetPosition() + delta);
}

void Character::Move(const Vector3& dir, float speed, float deltaTime)
{
    if (!mActor)
    {
        return;
    }

    // とりあえずシンプルに「位置を直接動かす」だけ。
    // 後で MoveComponent 連携に差し替えてもOK。
    Vector3 delta = dir * speed * deltaTime;
    AddWorldOffset(delta);
}

void Character::Update(float deltaTime)
{
    (void)deltaTime;
    // 将来的に：
    // - 入力状態に応じて Move()/Jump()/Attack() を呼ぶ
    // - アニメーション再生
    // などをここに集約していく想定
}

} // namespace toy::kit
