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

    // タイトル / ストーリー中はポーズメニューを開かない
    bool AllowsPause() const override { return false; }
protected:
    void DefineWorld() override;

private:
    class toy::MessageBoxActor* mMsgActor;
    std::string mStory;
    void OpenStory();
    void ChangeScene();
    void DeploySky();
    std::unique_ptr<class toy::WeatherManager> mWeather;
};
