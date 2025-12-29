#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>

class StageScene : public toy::kit::IScene
{
public:
    explicit StageScene(){}
    void Update(float delatTime) override;
    void ProcessInput(const struct toy::InputState& input) override;
protected:
    void InitScene() override;
    
private:
    class toy::MessageBoxActor* mMsgActor;
    void ChangeScene();

};
