#pragma once

#include "UI/UIScreen.h"
#include "UI/UIFrame.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>

namespace toy {

class NineSliceSpriteComponent;
class TextSpriteComponent;
class MessageTextComponent;
class TextFont;

//==============================================================
// ConfirmDialogActor
//  - はい / いいえ の確認ダイアログ（既定：画面中央）
//  - 背景・枠線(UIFrame) + 質問文(折り返しあり) + 横並びの はい / いいえ
//  - Open で UIStack に積み、決定 / キャンセルで閉じてから onResult を呼ぶ
//
// 操作
//  - 選択：左右（上下・A / D キーも可）
//  - 決定：A    キャンセル：B（= いいえ。Desc.allowCancel が true のとき）
//==============================================================
class ConfirmDialogActor : public UIScreen
{
public:
    struct Desc
    {
        // ----------------------------
        // レイアウト
        // ----------------------------
        UILayout layout  { DefaultLayout() };
        Vector2  padding { Vector2(20.0f, 16.0f) };

        // ボタン（はい / いいえ）
        Vector2 buttonSize { Vector2(120.0f, 36.0f) };
        float   buttonGap  { 24.0f };

        // ----------------------------
        // 見た目
        // ----------------------------
        UIFrameStyle frame      { DefaultFrame() };
        UITransition transition { DefaultTransition() };

        Vector3 textColor { Vector3(1.0f, 1.0f, 1.0f) };
        int     lineGapPx { 2 };

        Vector3 cursorColor  { Vector3(0.3f, 0.5f, 0.9f) };
        float   cursorAlpha  { 0.5f };
        int     cursorRadius { 4 };

        Vector3 buttonTextColor         { Vector3(0.7f, 0.7f, 0.7f) };
        Vector3 selectedButtonTextColor { Vector3(1.0f, 1.0f, 1.0f) };

        std::string yesText { "はい" };
        std::string noText  { "いいえ" };

        // ----------------------------
        // フォント（未指定ならシステムフォントを使用）
        // ----------------------------
        std::shared_ptr<TextFont> font;
        std::string defaultFontPath { "rounded-mplus-1c-bold.ttf" };
        int defaultFontSize { 22 };

        // ----------------------------
        // 操作
        // ----------------------------
        bool allowCancel { true };   // B で「いいえ」
        bool pauseGame   { false };  // 開いている間ゲームを止める

        static UIFrameStyle DefaultFrame()
        {
            UIFrameStyle f;
            f.bgAlpha = 0.85f;
            return f;
        }

        static UITransition DefaultTransition()
        {
            UITransition t;
            t.slideFrom = Vector2(0.0f, 8.0f);
            return t;
        }

        static UILayout DefaultLayout()
        {
            UILayout l;
            l.anchor = UIAnchor::Center;
            l.size   = Vector2(400.0f, 160.0f);
            return l;
        }
    };

    using ResultFn = std::function<void(bool yes)>;

public:
    ConfirmDialogActor(class Application* app, const Desc& desc);
    ~ConfirmDialogActor() override = default;

    void ActorInput(const struct InputState& state) override;

    // defaultYes：開いたときに「はい」を選んだ状態にするか
    void Open(const std::string& text, ResultFn onResult, bool defaultYes = true);
    void Close();

    bool IsOpen() const { return mOpen; }

protected:
    void OnLayoutChanged(const UIRect& rect) override;
    void OnHidden() override;

private:
    void Decide(bool yes);
    void SetSelectedYes(bool yes);
    void SetVisible(bool visible);
    void RefreshButtonColors();

private:
    Desc mDesc;

    UIFrame                   mFrame;
    MessageTextComponent*     mText   { nullptr };
    NineSliceSpriteComponent* mCursor { nullptr };
    TextSpriteComponent*      mYes    { nullptr };
    TextSpriteComponent*      mNo     { nullptr };

    bool mSelectedYes { true };
    bool mOpen        { false };

    ResultFn mOnResult;
};

} // namespace toy
