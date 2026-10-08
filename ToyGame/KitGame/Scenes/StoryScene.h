#pragma once

#include "KitCore/IScene.h"
#include "ToyLib.h"
#include <iostream>
#include <memory>

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
    void DeploySky();
    std::unique_ptr<class toy::WeatherManager> mWeather;
};
