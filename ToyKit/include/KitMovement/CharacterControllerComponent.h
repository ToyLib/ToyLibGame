#pragma once
#include "Engine/Core/Component.h"
#include "KitMovement/MoveIntent.h"

namespace toy::kit {

class CharacterControllerComponent : public toy::Component
{
public:
    using Component::Component;

    virtual void BuildMoveIntent(MoveIntent& outIntent) = 0;
};

} // namespace toy::kit
