#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>
#include <memory>

class TitleScene : public toy::kit::IScene
{
public:
    TitleScene();

    void ProcessInput(const struct toy::InputState& input) override;
    void Update(float deltaTime) override;
protected:
    void DefineWorld() override;
    void UnloadScene() override;
private:
    toy::MeshComponent* mLogoMesh;
    toy::Actor* mLogoActor;
    toy::ChoiceListActor* mMenu = nullptr;
    float mColor;
    float mIntensity = 1.0f;
};
