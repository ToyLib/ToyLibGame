#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

class FieldScene : public toy::kit::IScene
{
public:
    explicit FieldScene();
    
    void ProcessInput(const struct toy::InputState& input) override;
    void Update(float deltaTime) override;
protected:
    void InitScene() override;
private:
    void DeployGround();
    void DeploySky();
    std::unique_ptr<class toy::WeatherManager> mWeather;
    

};
