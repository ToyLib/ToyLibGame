#pragma once

#include "Engine/Core/Actor.h"
#include <functional>
#include <string>
#include <memory>

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
    MessageBoxActor(class Application* app, std::shared_ptr<class TextFont> font = nullptr);
    ~MessageBoxActor() override = default;

    void UpdateActor(float dt) override;
    void ActorInput(const struct InputState& state) override;

    void Open(const std::string& text, std::function<void()> onClose = nullptr);
    void Close();

    bool IsOpen() const { return mOpen; }

    // 表示ON/OFF（Open/Closeとは別に隠す用途）
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }
    
    // 位置（Actor自体の位置）
    void SetBoxPosition(const Vector3& pos);

    // 背景サイズ
    void SetBoxSize(const Vector2& size);

    // テキスト用 padding（背景内余白）
    void SetPadding(const Vector2& padding);

private:
    void ApplyLayout();

private:
    SpriteComponent*      mBg   = nullptr;
    MessageTextComponent* mText = nullptr;

    bool mOpen = false;
    bool mEnabled = false;
    Vector2 mBoxSize = Vector2(640.0f, 160.0f);
    Vector2 mPadding = Vector2(16.0f, 16.0f);
    
    std::function<void()> mOnClose;
};

} // namespace toy
