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
    void InitField();
    void DeployGround();
    void DeploySky();
    void DeployBrick(Vector3 pos);
    void DeployFire(Vector3 pos);
    std::unique_ptr<class toy::WeatherManager> mWeather;
    
    class toy::TextSpriteComponent* mTextComp;
    
    toy::DebugDrawActor* mDebugDrawActor;
    toy::Actor* mPlayerActor;
};
