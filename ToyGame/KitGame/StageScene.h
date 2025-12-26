#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>

class StageScene : public toy::kit::IScene
{
public:
    explicit StageScene(){}// : mStageNo(stageNo) {}
    void Update(float delatTime) {std::cout << "Scene Stage " << delatTime << std::endl;}
private:
    int mStageNo = 1;
};
