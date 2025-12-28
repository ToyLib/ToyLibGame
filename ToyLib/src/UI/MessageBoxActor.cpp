#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/SpriteComponent.h"
#include "UI/MessageTextComponent.h" // ←配置に合わせてパス調整

namespace toy {

MessageBoxActor::MessageBoxActor(Application* app)
    : Actor(app)
{
    SetActorID("MessageBox");

    // 例：UIを左下に置く（座標系はToyLibのUIルールに合わせて調整）
    SetPosition(Vector3(40.0f, 520.0f, 0.0f));

    // 背景
    mBg = CreateComponent<SpriteComponent>(900, VisualLayer::UI);
    mBg->SetTexture(app->GetAssetManager()->GetWhite1x1Texture());
    mBg->SetColor(Vector3(0.05f, 0.05f, 0.08f));
    mBg->SetAlpha(0.75f);

    // 本文（ページ化テキスト）
    mText = CreateComponent<MessageTextComponent>(910, VisualLayer::UI);

    auto font = app->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 20);
    mText->SetFont(font);
    mText->SetColor(Vector3(1.0f, 1.0f, 1.0f));

    // メッセージ枠サイズ（固定）
    mText->SetTextBoxSize(Vector2(640.0f, 160.0f));
    mText->SetPadding(Vector2(16.0f, 16.0f));
    mText->SetLineGapPx(2);

    // padding分、テキストを内側へ（SpriteComponentにLocalOffset追加済み前提）
    //mText->SetLocalOffset(Vector3(16.0f, 16.0f, 0.0f));

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
    mText->SetMessage(text);

    ApplyLayout();
}

void MessageBoxActor::Close()
{
    if (!mOpen) return;

    mOpen = false;
    SetEnabled(false);

    if (mOnClose)
        mOnClose();
}

void MessageBoxActor::ActorInput(const InputState& state)
{
    if (!mOpen || !mEnabled) return;

    // A: 次ページ or 閉じる
    if (state.IsButtonPressed(GameButton::A))
    {
        if (mText->HasNextPage())
        {
            mText->NextPage();
            ApplyLayout();
        }
        else
        {
            Close();
        }
    }

    // B: 即閉じ（任意）
    if (state.IsButtonPressed(GameButton::B))
    {
        Close();
    }
}

void MessageBoxActor::UpdateActor(float dt)
{
    if (!mOpen || !mEnabled) return;

    // 文字送り等を入れるならここ
}

void MessageBoxActor::ApplyLayout()
{
    // メッセージボックスは固定サイズが扱いやすい（必要ならテキスト量で伸縮も可）
    const float w = 640.0f;
    const float h = 160.0f;

    mBg->SetScale(w, h);
}

} // namespace toy
