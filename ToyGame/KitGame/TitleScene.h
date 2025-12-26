#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>
#include <memory>

class TitleScene : public toy::kit::IScene
{
public:
    void OnEnter(const toy::kit::SceneContext& ctx) override;

    void Update(float dt) override;
    
    void ProcessInput(const struct toy::InputState& input) override;
private:
    toy::MeshComponent* mLogoMesh;
    toy::Actor* mLogoActor;
};
