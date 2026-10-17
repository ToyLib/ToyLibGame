#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"

#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/SpriteComponent.h"
#include "Graphics/Sprite/TextSpriteComponent.h"
#include "UI/MessageTextComponent.h"

#include <cmath>

namespace toy {

MessageBoxActor::MessageBoxActor(Application* app, const Desc& desc)
    : UIScreen(app)
    , mDesc(desc)
{
    SetActorID("MessageBox");

    // 背景
    mBg = CreateComponent<SpriteComponent>(900, VisualLayer::UI);
    mBg->SetTexture(app->GetAssetManager()->GetWhite1x1Texture());
    mBg->SetColor(mDesc.bgColor);
    mBg->SetAlpha(mDesc.bgAlpha);

    // 本文
    mText = CreateComponent<MessageTextComponent>(9001, VisualLayer::UI);

    // フォント（未指定ならデフォルト）
    if (mDesc.font)
    {
        mText->SetFont(mDesc.font);
    }
    else
    {
        auto f = app->GetSysAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
        mText->SetFont(f);
        mDesc.font = f; // 次回以降のために保持（任意）
    }

    // 色・行間
    mText->SetColor(mDesc.textColor);
    mText->SetLineGapPx(mDesc.lineGapPx);

    // 送りインジケータ（本文と同じフォント・色）
    mIndicator = CreateComponent<TextSpriteComponent>(9002, VisualLayer::UI);
    ApplyIndicatorStyle();

    // 初期レイアウト反映（位置・背景サイズ・折り返し幅）
    SetLayout(mDesc.layout);

    // 初期は非表示
    SetEnabled(false);
}

void MessageBoxActor::ApplyDesc(const Desc& desc)
{
    mDesc = desc;

    if (mBg)
    {
        mBg->SetColor(mDesc.bgColor);
        mBg->SetAlpha(mDesc.bgAlpha);
        // drawOrder/layer変更は再生成が必要なのでここでは触らない（必要なら作り直し関数を用意）
    }

    if (mText)
    {
        if (mDesc.font)
        {
            mText->SetFont(mDesc.font);
        }
        else
        {
            auto f = GetApp()->GetAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
            mText->SetFont(f);
            mDesc.font = f;
        }

        mText->SetColor(mDesc.textColor);
        mText->SetLineGapPx(mDesc.lineGapPx);
    }

    ApplyIndicatorStyle();
    SetLayout(mDesc.layout);
}

void MessageBoxActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;
    if (mBg)   mBg->SetVisible(enabled);
    if (mText) mText->SetVisible(enabled);
    if (mIndicator) mIndicator->SetVisible(false);   // 表示は UpdateIndicator が決める
}

void MessageBoxActor::Open(const std::string& text, std::function<void()> onClose)
{
    mOnClose = std::move(onClose);
    mOpen = true;

    SetEnabled(true);
    PushToUIStack();

    // 直前にDescが変わってても正しく反映
    ApplyLayout();

    mText->SetMessage(text);
}

void MessageBoxActor::Close()
{
    if (!mOpen) return;

    mOpen = false;
    SetEnabled(false);
    RemoveFromUIStack();

    if (mOnClose)
    {
        mOnClose();
    }
}

void MessageBoxActor::ActorInput(const InputState& state)
{
    if (!mOpen || !mEnabled) return;

    if (state.IsButtonPressed(GameButton::A))
    {
        if (mText->IsTyping())
        {
            mText->FinishTyping();
        }
        else if (mText->HasNextPage())
        {
            mText->NextPage();
        }
        else
        {
            Close();
        }
    }

    if (mDesc.enableBToClose && state.IsButtonPressed(GameButton::B))
    {
        Close();
    }
}

void MessageBoxActor::UpdateActor(float dt)
{
    if (!mOpen || !mEnabled) return;

    UpdateIndicator(dt);
}

//------------------------------------------------------------------------------
// 送りインジケータ
//  - 文字送り中は隠し、ページを出し切ったら右下で上下に揺らす
//------------------------------------------------------------------------------
void MessageBoxActor::ApplyIndicatorStyle()
{
    if (!mIndicator) return;

    mIndicator->SetFont(mDesc.font);
    mIndicator->SetColor(mDesc.textColor);
    mIndicator->SetText(mDesc.indicatorText);
}

void MessageBoxActor::UpdateIndicator(float dt)
{
    if (!mIndicator) return;

    const bool show = mDesc.showIndicator && !mText->IsTyping();
    if (!show)
    {
        mIndicator->SetVisible(false);
        mIndicatorTime = 0.0f;   // 出るたびに同じ位相から揺らす
        return;
    }

    mIndicatorTime += dt;
    const float bob = std::sin(mIndicatorTime * mDesc.indicatorBobSpeed) * mDesc.indicatorBobPx;

    mIndicator->SetOffset(mIndicatorBase + Vector3(0.0f, bob, 0.0f));
    mIndicator->SetVisible(true);
}

void MessageBoxActor::OnLayoutChanged(const UIRect& rect)
{
    if (!mBg || !mText) return;

    // 背景サイズ（Actor 位置 = 矩形の左上）
    mBg->SetScale(rect.size.x, rect.size.y);

    // 本文の折り返し条件
    mText->SetTextBoxSize(rect.size);
    mText->SetPadding(mDesc.padding);

    // 本文開始位置（padding分だけ内側）
    mText->SetOffset(Vector3(mDesc.padding.x, mDesc.padding.y, 0.0f));

    // インジケータ：padding の内側、右下
    if (mIndicator)
    {
        Vector2 indSize = Vector2::Zero;
        if (auto tex = mIndicator->GetTexture())
        {
            indSize = Vector2(static_cast<float>(tex->GetWidth()), static_cast<float>(tex->GetHeight()));
        }
        mIndicatorBase = Vector3(rect.size.x - mDesc.padding.x - indSize.x,
                                 rect.size.y - mDesc.padding.y - indSize.y,
                                 0.0f);
    }
}

} // namespace toy
