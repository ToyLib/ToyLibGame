#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/SpriteComponent.h"
#include "UI/MessageTextComponent.h"

namespace toy {

MessageBoxActor::MessageBoxActor(Application* app, std::shared_ptr<TextFont> font)
    : Actor(app)
{
    SetActorID("MessageBox");

    // 背景
    mBg = CreateComponent<SpriteComponent>(900, VisualLayer::UI);
    mBg->SetTexture(app->GetAssetManager()->GetWhite1x1Texture());
    mBg->SetColor(Vector3(0.05f, 0.05f, 0.08f));
    mBg->SetAlpha(0.5f);

    // 本文
    mText = CreateComponent<MessageTextComponent>(910, VisualLayer::UI);

    if (font)
    {
        mText->SetFont(font);
    }
    else
    {
        auto defaultFont = app->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 20);
        mText->SetFont(defaultFont);
    }
    mText->SetColor(Vector3(1.0f, 1.0f, 1.0f));

    // ★ ここでは “初期値” だけセット（決め打ちはしない）
    // Scene から SetBoxPosition / SetBoxSize / SetPadding を呼んで上書きできる
    mBoxSize = Vector2(640.0f, 160.0f);
    mPadding = Vector2(16.0f, 16.0f);

    // line gap は好み。外からSetできるなら後で足してもOK
    mText->SetLineGapPx(2);

    // 初期レイアウト反映（非表示だが設定は整えておく）
    ApplyLayout();

    SetEnabled(false);
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

    // レイアウト反映（位置/サイズ/paddingを外から変えた後でも正しくなる）
    ApplyLayout();

    // ページ構築＆表示開始
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

    if (state.IsButtonPressed(GameButton::B))
    {
        Close();
    }
}

void MessageBoxActor::UpdateActor(float dt)
{
    if (!mOpen || !mEnabled) return;

    // もし MessageTextComponent 側に Update(deltaTime) を追加しているならここで呼ぶ
    // mText->Update(dt);
}

void MessageBoxActor::ApplyLayout()
{
    if (!mBg || !mText) return;

    // 背景：ボックスサイズそのまま
    mBg->SetScale(mBoxSize.x, mBoxSize.y);

    // テキスト：内側サイズ（padding差し引き）
    Vector2 inner;
    inner.x = std::max(0.0f, mBoxSize.x - mPadding.x * 2.0f);
    inner.y = std::max(0.0f, mBoxSize.y - mPadding.y * 2.0f);

    // MessageTextComponent は「表示領域」を欲しい
    // （BuildPagesで mBoxSize - padding*2 してるなら、ここは mBoxSize を渡す設計でもOKだけど
    //  混乱を避けるため “ここで内側にして渡す” に統一するのが安全）
    mText->SetTextBoxSize(Vector2(inner.x + mPadding.x * 2.0f, inner.y + mPadding.y * 2.0f));
    mText->SetPadding(mPadding);

    // テキスト表示位置：padding分だけずらす
    mText->SetOffset(Vector3(mPadding.x, mPadding.y, 0.0f));
}

void MessageBoxActor::SetBoxPosition(const Vector3& pos)
{
    SetPosition(pos);
}

void MessageBoxActor::SetBoxSize(const Vector2& size)
{
    mBoxSize = size;
    ApplyLayout();
}

void MessageBoxActor::SetPadding(const Vector2& padding)
{
    mPadding = padding;
    ApplyLayout();
}

} // namespace toy
