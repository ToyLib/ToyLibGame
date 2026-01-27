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
    void ActorInput(const struct InputState& state) override;
    
    // F3 などで ON/OFF 切り替えしたいとき用
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }
    // ワイヤーフレームのON/OFF
    void SetWireVisible(bool visible);
    bool IsWireVisible() const { return mWireVisible; }
    
    void SetTextColor(const Vector3& color) { mTextColor = color; }
    
private:
    class TextSpriteComponent* mTextComp { nullptr };
    class SpriteComponent*     mBgSprite { nullptr };
    bool mEnabled                        { false };
    bool mWireVisible                    { false };
    
    // FPS を少し滑らかにしたいとき用
    float mSmoothedFPS { 0.0f };
    
    Vector3 mTextColor { 0.3f, 1.0f, 0.3f };
};

} // namespace toy
