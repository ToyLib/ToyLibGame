#pragma once

#include <iostream>
#include "KitCore/IScene.h"

class TitleScene : public toy::kit::IScene
{
public:
    void OnEnter(const toy::kit::SceneContext& ctx) override
    {
        IScene::OnEnter(ctx);
        std::cout << "[TitleScene] Enter\n";
    }

    void Update(float dt) override
    {
        std::cout << "[TitleScene] Update dt=" << dt << "\n";
    }
};
