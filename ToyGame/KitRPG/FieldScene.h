#pragma once

#include "KitCore/Scene.h"
#include "KitCore/Stage.h"
#include "KitCore/Character.h"

class FieldScene : public toy::kit::Scene
{
public:
    FieldScene();
    virtual ~FieldScene() = default;

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;

private:
    toy::kit::Stage     mStage;
    toy::kit::Character mPlayer;
};
