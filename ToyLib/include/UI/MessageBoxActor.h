#pragma once

#include "UI/UIScreen.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>

namespace toy {

class SpriteComponent;
class TextSpriteComponent;
class MessageTextComponent;
class TextFont;

//==============================================================
// MessageBoxActor
//  - 背景(Sprite) + 本文(MessageTextComponent) を束ねた UI Actor
//  - Desc で初期設定をまとめて渡す（Scene 側の Set〇〇 連打を減らす）
//  - Open で UIStack に積み、Close で外す（表示中は Modal として入力を占有）
//==============================================================
class MessageBoxActor : public UIScreen
{
public:
    // 初期設定束
    struct Desc
    {
        // ----------------------------
        // レイアウト（既定：画面下中央、下端から 40px 上）
        // ----------------------------
        UILayout layout  { DefaultLayout() };
        Vector2  padding { Vector2(16.0f, 16.0f) };
        
        // ----------------------------
        // 見た目
        // ----------------------------
        Vector3 bgColor   { Vector3(0.05f, 0.05f, 0.08f) };
        float   bgAlpha   { 0.5f };
        
        Vector3 textColor { Vector3(1.0f, 1.0f, 1.0f) };
        int     lineGapPx { 2 };
        
        
        // ----------------------------
        // 送りインジケータ（文字送りが終わったら右下に表示）
        // ----------------------------
        bool        showIndicator     { true };
        std::string indicatorText     { "▼" };
        float       indicatorBobPx    { 3.0f };   // 上下に揺れる幅
        float       indicatorBobSpeed { 6.0f };   // 揺れの速さ（rad/sec）
        
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

        static UILayout DefaultLayout()
        {
            UILayout l;
            l.anchor = UIAnchor::Bottom;
            l.offset = Vector2(0.0f, -40.0f);
            l.size   = Vector2(640.0f, 160.0f);
            return l;
        }
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
    
protected:
    void OnLayoutChanged(const UIRect& rect) override;
    
private:
    void ApplyIndicatorStyle();
    void UpdateIndicator(float dt);
    
private:
    Desc mDesc;
    
    SpriteComponent*      mBg   { nullptr };
    MessageTextComponent* mText { nullptr };
    TextSpriteComponent*  mIndicator { nullptr };
    
    // インジケータの基準位置（ローカル）と揺れの経過時間
    Vector3 mIndicatorBase { Vector3::Zero };
    float   mIndicatorTime { 0.0f };
    
    bool mOpen      { false };
    bool mEnabled   { false };

    std::function<void()> mOnClose;
};

} // namespace toy
