#pragma once

#include "UI/UIScreen.h"
#include "UI/UIFrame.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toy {

class TextSpriteComponent;
class MessageTextComponent;
class ChoiceListActor;
class TextFont;

//==============================================================
// MessageBoxActor
//  - 背景・枠線(UIFrame) + 本文(MessageTextComponent) + 送りインジケータを束ねた UI Actor
//  - Desc で初期設定をまとめて渡す（Scene 側の Set〇〇 連打を減らす）
//  - Open で UIStack に積み、Close で外す（表示中は Modal として入力を占有）
//
// 選択肢付きメッセージ
//  - AttachChoiceList() で選択肢リストを渡しておき、OpenWithChoices() で開く
//  - 最後のページを出し切ると、メッセージの右上に選択肢を出す
//    （選択中もメッセージは表示したまま。入力は選択肢リストが受け取る）
//  - 決定するとメッセージを閉じてから onDecide(index) を呼ぶ
//  - 選択肢リストをキャンセルしたら、A でもう一度出せる
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
        UIFrameStyle frame { DefaultFrame() };   // 背景 + 枠線（角丸）
        UITransition transition;                 // 開閉アニメーション（下から浮き上がる）
        
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
        bool enableBToClose { true };   // 選択肢付きのときは無効（選ばずに閉じられないように）
        bool pauseGame      { false };  // 開いている間ゲームを止める

        // ----------------------------
        // 選択肢付きメッセージ
        // ----------------------------
        float choiceGap { 8.0f };   // メッセージと選択肢リストの間隔（px）

        static UIFrameStyle DefaultFrame()
        {
            UIFrameStyle f;
            f.bgAlpha = 0.5f;
            return f;
        }

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
    
    using DecideFn = std::function<void(int index)>;

    void Open(const std::string& text, std::function<void()> onClose = nullptr);
    void Close();

    // 選択肢付き（事前に AttachChoiceList が必要）
    void OpenWithChoices(const std::string& text,
                         const std::vector<std::string>& choices,
                         DecideFn onDecide);

    // 選択肢に使うリスト（寿命は呼び出し側＝Scene が管理する）
    void AttachChoiceList(ChoiceListActor* list) { mChoiceList = list; }
    
    bool IsOpen() const { return mOpen; }
    
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }
    
    // 実行中に差し替えたい場合用（任意）
    void ApplyDesc(const Desc& desc);
    
protected:
    void OnLayoutChanged(const UIRect& rect) override;
    void OnHidden() override;
    
private:
    void ApplyIndicatorStyle();
    void UpdateIndicator(float dt);

    bool HasPendingChoices() const { return !mChoices.empty(); }
    bool IsOnLastPageReady() const;
    void ShowChoices();
    
private:
    Desc mDesc;
    
    UIFrame               mFrame;
    MessageTextComponent* mText { nullptr };
    TextSpriteComponent*  mIndicator { nullptr };
    
    // インジケータの基準位置（ローカル）と揺れの経過時間
    Vector3 mIndicatorBase { Vector3::Zero };
    float   mIndicatorTime { 0.0f };
    
    bool mOpen      { false };
    bool mEnabled   { false };

    std::function<void()> mOnClose;

    // 選択肢付きメッセージ
    ChoiceListActor*         mChoiceList { nullptr };
    std::vector<std::string> mChoices;
    DecideFn                 mOnDecide;
    bool mChoiceShown     { false };   // 選択肢リストを表示中
    bool mChoiceCancelled { false };   // キャンセルされた（A で出し直すまで自動で出さない）
};

} // namespace toy
