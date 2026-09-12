#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>

class StoryScene : public toy::kit::IScene
{
public:
    explicit StoryScene(){}
    void Update(float delatTime) override;
    void ProcessInput(const struct toy::InputState& input) override;
protected:
    void DefineWorld() override;

private:
    class toy::MessageBoxActor* mMsgActor;
    void ChangeScene();

};
