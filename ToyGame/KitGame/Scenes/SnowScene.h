#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

class SnowScene : public toy::kit::IScene
{
public:
    explicit SnowScene();
    
    void ProcessInput(const struct toy::InputState& input) override;
    void Update(float deltaTime) override;
protected:
    void InitScene() override;
private:
    void InitField();
    void DeployGround();
    void DeploySky();
    void DeployBrick(Vector3 pos);
    void DeployFire(Vector3 pos);
    std::unique_ptr<class toy::WeatherManager> mWeather;
    
    class toy::TextSpriteComponent* mTextComp;
    
    toy::Actor* mPlayerActor;
    toy::Actor* mBackCamera;
};
