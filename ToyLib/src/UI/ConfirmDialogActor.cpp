#include "UI/ConfirmDialogActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/NineSliceSpriteComponent.h"
#include "Graphics/Sprite/TextSpriteComponent.h"
#include "UI/MessageTextComponent.h"

namespace toy {

namespace {
constexpr int kBgDrawOrder     = 900;
constexpr int kLineDrawOrder   = 901;
constexpr int kCursorDrawOrder = 950;
constexpr int kTextDrawOrder   = 1000;

// テキストスプライトの表示サイズ（論理 px）
Vector2 TextSize(const TextSpriteComponent* text)
{
    if (auto tex = text->GetTexture())
    {
        return Vector2(static_cast<float>(tex->GetWidth()), static_cast<float>(tex->GetHeight()));
    }
    return Vector2::Zero;
}
}

ConfirmDialogActor::ConfirmDialogActor(Application* app, const Desc& desc)
    : UIScreen(app)
    , mDesc(desc)
{
    SetActorID("ConfirmDialog");

    auto* assets = app->GetAssetManager();

    // フォント（未指定ならシステムフォント）
    if (!mDesc.font)
    {
        mDesc.font = app->GetSysAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
    }

    // 背景 + 枠線
    mFrame.Create(this, kBgDrawOrder, kLineDrawOrder);
    mFrame.ApplyStyle(mDesc.frame);

    SetTransition(mDesc.transition);
    SetPausesGame(mDesc.pauseGame);

    // 質問文（折り返しあり。文字送りはしない）
    mText = CreateComponent<MessageTextComponent>(kTextDrawOrder, VisualLayer::UI);
    mText->SetFont(mDesc.font);
    mText->SetColor(mDesc.textColor);
    mText->SetLineGapPx(mDesc.lineGapPx);

    // カーソル（選択中のボタンの背景）
    mCursor = CreateComponent<NineSliceSpriteComponent>(kCursorDrawOrder, VisualLayer::UI);
    if (mDesc.cursorRadius > 0)
    {
        mCursor->SetTexture(assets->GetRoundedRectTexture(mDesc.cursorRadius));
        mCursor->SetBorder(mDesc.cursorRadius);
    }
    else
    {
        mCursor->SetTexture(assets->GetWhite1x1Texture());
    }
    mCursor->SetColor(mDesc.cursorColor);
    mCursor->SetAlpha(mDesc.cursorAlpha);

    // はい / いいえ
    mYes = CreateComponent<TextSpriteComponent>(kTextDrawOrder, VisualLayer::UI);
    mYes->SetFont(mDesc.font);
    mYes->SetText(mDesc.yesText);

    mNo = CreateComponent<TextSpriteComponent>(kTextDrawOrder, VisualLayer::UI);
    mNo->SetFont(mDesc.font);
    mNo->SetText(mDesc.noText);

    RefreshButtonColors();
    SetLayout(mDesc.layout);
    SetVisible(false);
}

//=============================================================================
// Open / Close
//=============================================================================

void ConfirmDialogActor::Open(const std::string& text, ResultFn onResult, bool defaultYes)
{
    mOnResult = std::move(onResult);
    mOpen     = true;

    SetVisible(true);
    ApplyLayout();   // 折り返し幅を決めてから本文を入れる

    mText->SetMessage(text);
    mText->FinishTyping();

    SetSelectedYes(defaultYes);
    PushToUIStack();
    BeginShow();
}

void ConfirmDialogActor::Close()
{
    if (!mOpen) return;

    mOpen = false;
    RemoveFromUIStack();   // 入力はすぐ手放す（見た目はフェードアウト後に消える）
    BeginHide();
}

void ConfirmDialogActor::OnHidden()
{
    if (!mOpen)
    {
        SetVisible(false);
    }
}

void ConfirmDialogActor::Decide(bool yes)
{
    // 閉じてから通知（コールバック内でシーン切り替え / 再 Open できるように）
    auto onResult = std::move(mOnResult);
    mOnResult = nullptr;
    Close();

    if (onResult)
    {
        onResult(yes);
    }
}

//=============================================================================
// Input
//=============================================================================

void ConfirmDialogActor::ActorInput(const InputState& state)
{
    if (!mOpen) return;

    const bool toggle =
        state.IsButtonPressed(GameButton::DPadLeft) || state.IsButtonPressed(GameButton::DPadRight) ||
        state.IsButtonPressed(GameButton::DPadUp)   || state.IsButtonPressed(GameButton::DPadDown)  ||
        state.IsButtonPressed(GameButton::KeyA)     || state.IsButtonPressed(GameButton::KeyD);
    if (toggle)
    {
        SetSelectedYes(!mSelectedYes);
    }

    if (state.IsButtonPressed(GameButton::A))
    {
        Decide(mSelectedYes);
    }
    else if (mDesc.allowCancel && state.IsButtonPressed(GameButton::B))
    {
        Decide(false);
    }
}

//=============================================================================
// Layout / Visual
//=============================================================================

void ConfirmDialogActor::SetSelectedYes(bool yes)
{
    mSelectedYes = yes;
    RefreshButtonColors();
    OnLayoutChanged(GetRect());   // カーソル位置を更新
}

void ConfirmDialogActor::OnLayoutChanged(const UIRect& rect)
{
    if (!mText || !mCursor || !mYes || !mNo) return;

    const UIRect local = GetLocalRect();
    mFrame.SetRect(local);

    // ボタン行：下端に横並びで中央寄せ
    const Vector2 bs     = mDesc.buttonSize;
    const float   rowW   = bs.x * 2.0f + mDesc.buttonGap;
    const float   rowTop = local.Bottom() - mDesc.padding.y - bs.y;
    const float   rowX   = local.Left() + (local.size.x - rowW) * 0.5f;

    const UIRect yesRect { Vector2(rowX, rowTop), bs };
    const UIRect noRect  { Vector2(rowX + bs.x + mDesc.buttonGap, rowTop), bs };

    // 質問文：ボタン行より上の領域で折り返す
    mText->SetTextBoxSize(Vector2(local.size.x, rowTop));
    mText->SetPadding(mDesc.padding);
    mText->SetOffset(Vector3(mDesc.padding.x, mDesc.padding.y, 0.0f));

    // ラベルはボタンの中央に置く
    auto placeLabel = [](TextSpriteComponent* label, const UIRect& r)
    {
        const Vector2 ts = TextSize(label);
        label->SetOffset(Vector3(r.Left() + (r.size.x - ts.x) * 0.5f,
                                 r.Top()  + (r.size.y - ts.y) * 0.5f,
                                 0.0f));
    };
    placeLabel(mYes, yesRect);
    placeLabel(mNo,  noRect);

    const UIRect& sel = mSelectedYes ? yesRect : noRect;
    mCursor->SetOffset(Vector3(sel.Left(), sel.Top(), 0.0f));
    mCursor->SetSize(sel.size);
}

void ConfirmDialogActor::RefreshButtonColors()
{
    mYes->SetColor(mSelectedYes ? mDesc.selectedButtonTextColor : mDesc.buttonTextColor);
    mNo->SetColor (mSelectedYes ? mDesc.buttonTextColor : mDesc.selectedButtonTextColor);
}

void ConfirmDialogActor::SetVisible(bool visible)
{
    mFrame.SetVisible(visible);
    if (mText)   mText->SetVisible(visible);
    if (mCursor) mCursor->SetVisible(visible);
    if (mYes)    mYes->SetVisible(visible);
    if (mNo)     mNo->SetVisible(visible);
}

} // namespace toy
