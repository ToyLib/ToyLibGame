#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>

class StageScene : public toy::kit::IScene
{
public:
    explicit StageScene(){}// : mStageNo(stageNo) {}
    void Update(float delatTime) override;
    
protected:
    void InitScene() override;
    
private:
    int mStageNo = 1;
};
