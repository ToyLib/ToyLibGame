#pragma once
#include "KitMovement/CharacterControllerComponent.h"
#include "Engine/Runtime/InputSystem.h"

namespace toy::kit {

struct PlayerControllerDesc
{
    float moveSpeed = 6.0f;
};

class PlayerControllerComponent final
    : public CharacterControllerComponent
{
public:
    PlayerControllerComponent(class toy::Actor* owner,
                              const PlayerControllerDesc& desc = {});

    void ProcessInput(const toy::InputState& state) override;
    void BuildMoveIntent(MoveIntent& outIntent) override;

private:
    PlayerControllerDesc mDesc;
    MoveIntent           mIntent;
};

} // namespace toy::kit
