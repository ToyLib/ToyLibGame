#pragma once
#include "ToyLib.h"
#include "ToyKit.h"

class OutdoorStage
{
public:
    OutdoorStage(toy::Application* app);
    ~OutdoorStage();
    
    void InitStage();
    void Update(float deltaTime);
    
private:
    toy::Application* mApp;
    std::unique_ptr<class toy::kit::WeatherManager> mWeather;
    
    void DeployBrick(const Vector3& pos, bool bWall = false);
    void DeployHouse(const Vector3& pos);
    void DeployFire(const Vector3& pos);
    
    void DeployGround();
    void DeploySky();

};
