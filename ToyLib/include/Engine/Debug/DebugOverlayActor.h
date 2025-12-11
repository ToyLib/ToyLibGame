#pragma once

#include "Engine/Core/Actor.h"
#include "Graphics/Sprite/TextSpriteComponent.h"

namespace toy {

// 画面左上にデバッグ情報をまとめて表示する Actor
class DebugOverlayActor : public Actor
{
public:
    DebugOverlayActor(class Application* app);
    ~DebugOverlayActor() override = default;

    void UpdateActor(float deltaTime) override;

    // F3 などで ON/OFF 切り替えしたいとき用
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }

private:
    TextSpriteComponent* mTextComp = nullptr;
    bool mEnabled = true;

    // FPS を少し滑らかにしたいとき用
    float mSmoothedFPS = 0.0f;
};

} // namespace toy
