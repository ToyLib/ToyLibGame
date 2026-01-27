#pragma once

#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>

namespace toy {

class SpriteComponent;
class MessageTextComponent;
class TextFont;

//==============================================================
// MessageBoxActor
//  - 背景(Sprite) + 本文(MessageTextComponent) を束ねた UI Actor
//  - Desc で初期設定をまとめて渡す（Scene 側の Set〇〇 連打を減らす）
//==============================================================
class MessageBoxActor : public Actor
{
public:
    // 初期設定束
    struct Desc
    {
        // ----------------------------
        // レイアウト
        // ----------------------------
        Vector3 position { Vector3(40.0f, 520.0f, 0.0f) };
        Vector2 boxSize  { Vector2(640.0f, 160.0f) };
        Vector2 padding  { Vector2(16.0f, 16.0f) };
        
        // ----------------------------
        // 見た目
        // ----------------------------
        Vector3 bgColor   { Vector3(0.05f, 0.05f, 0.08f) };
        float   bgAlpha   { 0.5f };
        
        Vector3 textColor { Vector3(1.0f, 1.0f, 1.0f) };
        int     lineGapPx { 2 };
        
        
        // ----------------------------
        // フォント（未指定ならデフォルトを使用）
        // ----------------------------
        std::shared_ptr<TextFont> font;
        std::string defaultFontPath { "Font/rounded-mplus-1c-bold.ttf" };
        int defaultFontSize { 20 };
        
        // ----------------------------
        // 入力（必要なら無効化できるように）
        // ----------------------------
        bool enableBToClose { true };
    };
    
public:
    explicit MessageBoxActor(class Application* app, const Desc& desc);
    ~MessageBoxActor() override = default;
    
    void UpdateActor(float dt) override;
    void ActorInput(const struct InputState& state) override;
    
    void Open(const std::string& text, std::function<void()> onClose = nullptr);
    void Close();
    
    bool IsOpen() const { return mOpen; }
    
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }
    
    // 実行中に差し替えたい場合用（任意）
    void ApplyDesc(const Desc& desc);
    
private:
    void ApplyLayout();
    
private:
    Desc mDesc;
    
    SpriteComponent*      mBg   { nullptr };
    MessageTextComponent* mText { nullptr };
    
    bool mOpen      { false };
    bool mEnabled   { false };

    std::function<void()> mOnClose;
};

} // namespace toy
