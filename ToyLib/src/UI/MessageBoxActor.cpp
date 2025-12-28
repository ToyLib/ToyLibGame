#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"

#include "Graphics/Sprite/SpriteComponent.h"
#include "UI/MessageTextComponent.h"

namespace toy {

MessageBoxActor::MessageBoxActor(Application* app, const Desc& desc)
    : Actor(app)
    , mDesc(desc)
{
    SetActorID("MessageBox");

    // 位置（Actor位置＝ボックス基準）
    SetPosition(mDesc.position);

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

    // 初期レイアウト反映
    ApplyLayout();

    // 初期は非表示
    SetEnabled(false);
}

void MessageBoxActor::ApplyDesc(const Desc& desc)
{
    mDesc = desc;

    // 位置は Actor 自体
    SetPosition(mDesc.position);

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

    ApplyLayout();
}

void MessageBoxActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;
    if (mBg)   mBg->SetVisible(enabled);
    if (mText) mText->SetVisible(enabled);
}

void MessageBoxActor::Open(const std::string& text, std::function<void()> onClose)
{
    mOnClose = std::move(onClose);
    mOpen = true;

    SetEnabled(true);

    // 直前にDescが変わってても正しく反映
    ApplyLayout();

    mText->SetMessage(text);
}

void MessageBoxActor::Close()
{
    if (!mOpen) return;

    mOpen = false;
    SetEnabled(false);

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

    // 文字送りを回すならここ（MessageTextComponent側がComponent Updateで回る設計なら不要）
    // mText->Update(dt);
}

void MessageBoxActor::ApplyLayout()
{
    if (!mBg || !mText) return;

    // 背景サイズ
    mBg->SetScale(mDesc.boxSize.x, mDesc.boxSize.y);

    // 本文の折り返し条件
    mText->SetTextBoxSize(mDesc.boxSize);
    mText->SetPadding(mDesc.padding);

    // 本文開始位置（padding分だけ内側）
    mText->SetOffset(Vector3(mDesc.padding.x, mDesc.padding.y, 0.0f));
}

} // namespace toy
