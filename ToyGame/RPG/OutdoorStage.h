#pragma once
#include "ToyLib.h"

class OutdoorStage
{
public:
    OutdoorStage(toy::Application* app);
    ~OutdoorStage();
    
    void InitStage();
    
private:
    toy::Application* mApp;
    
    void DeployBrick(const Vector3& pos, bool bWall = false);
    void DeployHouse(const Vector3& pos);
    
    void DeployGround();

};
