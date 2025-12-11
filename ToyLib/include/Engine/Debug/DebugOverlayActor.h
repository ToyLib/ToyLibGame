#pragma once

#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

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
    
    void SetTextColor(const Vector3& color) { mTextColor = color; }

private:
    class TextSpriteComponent* mTextComp;
    bool mEnabled;

    // FPS を少し滑らかにしたいとき用
    float mSmoothedFPS;
    
    Vector3 mTextColor;
};

} // namespace toy
