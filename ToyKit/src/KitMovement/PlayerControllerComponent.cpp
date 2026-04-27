#include "KitMovement/PlayerControllerComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"

namespace toy::kit {

PlayerControllerComponent::PlayerControllerComponent(
    toy::Actor* owner,
    const PlayerControllerDesc& desc)
    : CharacterControllerComponent(owner)
    , mDesc(desc)
{
}

void PlayerControllerComponent::ProcessInput(const toy::InputState& state)
{
    Vector3 dir = Vector3::Zero;

    if (state.IsButtonDown(toy::GameButton::DPadUp))    dir.z += 1.0f;
    if (state.IsButtonDown(toy::GameButton::DPadDown))  dir.z -= 1.0f;
    if (state.IsButtonDown(toy::GameButton::DPadRight)) dir.x += 1.0f;
    if (state.IsButtonDown(toy::GameButton::DPadLeft))  dir.x -= 1.0f;

    if (dir.LengthSq() > 0.0f)
    {
        dir.Normalize();
    }

    mIntent.moveDir   = dir;
    mIntent.moveSpeed = mDesc.moveSpeed;
    mIntent.jump      = state.IsButtonPressed(toy::GameButton::A);
}

void PlayerControllerComponent::BuildMoveIntent(MoveIntent& outIntent)
{
    outIntent = mIntent;
}

} // namespace toy::kit
