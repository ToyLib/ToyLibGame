#pragma once

#include "Engine/Core/Actor.h"
#include <functional>
#include <string>

namespace toy {

class SpriteComponent;
class MessageTextComponent;

//==============================================================
// MessageBoxActor
//  - UIの塊（背景スプライト + MessageTextComponent）
//  - A: 次ページ / 終端なら閉じる
//  - B: 即閉じ（任意）
//  - Scene から Open/Close を呼ぶだけで使える
//==============================================================
class MessageBoxActor : public Actor
{
public:
    MessageBoxActor(class Application* app);
    ~MessageBoxActor() override = default;

    void UpdateActor(float dt) override;
    void ActorInput(const struct InputState& state) override;

    void Open(const std::string& text, std::function<void()> onClose = nullptr);
    void Close();

    bool IsOpen() const { return mOpen; }

    // 表示ON/OFF（Open/Closeとは別に隠す用途）
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }

private:
    void ApplyLayout();

private:
    SpriteComponent*      mBg   = nullptr;
    MessageTextComponent* mText = nullptr;

    bool mOpen = false;
    bool mEnabled = false;

    std::function<void()> mOnClose;
};

} // namespace toy
