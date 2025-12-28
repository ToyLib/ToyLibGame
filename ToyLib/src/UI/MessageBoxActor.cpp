#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/SpriteComponent.h"
#include "UI/MessageTextComponent.h"

#include <algorithm> // std::max

namespace toy {

//==============================================================
// MessageBoxActor
//  - 背景(Sprite) + 本文(MessageText) を束ねた “UIの塊”
//  - Scene 側から位置/サイズ/padding を Setter で注入して使う
//  - 入力：A=次へ（文字送り中なら全文表示） / B=閉じる
//==============================================================
MessageBoxActor::MessageBoxActor(Application* app, std::shared_ptr<TextFont> font)
    : Actor(app)
    , mBg(nullptr)
    , mText(nullptr)
    , mOpen((false))
    , mEnabled(false)
    , mBoxSize(Vector2(640.0f, 160.0f))
    , mPadding(Vector2(16.0f, 16.0f))
{
    SetActorID("MessageBox");

    //----------------------------------------------------------
    // 背景（半透明の矩形）
    //----------------------------------------------------------
    mBg = CreateComponent<SpriteComponent>(900, VisualLayer::UI);
    mBg->SetTexture(app->GetAssetManager()->GetWhite1x1Texture());
    mBg->SetColor(Vector3(0.05f, 0.05f, 0.08f));
    mBg->SetAlpha(0.5f);

    //----------------------------------------------------------
    // 本文（折り返し/改ページ/文字送り担当）
    //----------------------------------------------------------
    mText = CreateComponent<MessageTextComponent>(910, VisualLayer::UI);

    // フォントは外から注入できる。無ければデフォルトを使う。
    if (font)
    {
        mText->SetFont(font);
    }
    else
    {
        auto defaultFont = app->GetAssetManager()->GetFont(
            "Font/rounded-mplus-1c-bold.ttf", 20
        );
        mText->SetFont(defaultFont);
    }

    // 文字色
    mText->SetColor(Vector3(1.0f, 1.0f, 1.0f));

    //----------------------------------------------------------
    // 初期値（※決め打ちではなく“デフォルト値”）
    //  Scene側が SetBoxPosition/SetBoxSize/SetPadding で上書きできる
    //----------------------------------------------------------
    mBoxSize = Vector2(640.0f, 160.0f);
    mPadding = Vector2(16.0f, 16.0f);

    // 行間（好み）
    mText->SetLineGapPx(2);

    // 初期レイアウト反映（非表示でも設定だけ整えておく）
    ApplyLayout();

    // 初期は非表示
    SetEnabled(false);
}

//--------------------------------------------------------------
// 表示ON/OFF（Open/Closeとは別に隠したい用途にも使う）
//--------------------------------------------------------------
void MessageBoxActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;

    if (mBg)   mBg->SetVisible(enabled);
    if (mText) mText->SetVisible(enabled);
}

//--------------------------------------------------------------
// Open / Close
//--------------------------------------------------------------
void MessageBoxActor::Open(const std::string& text, std::function<void()> onClose)
{
    mOnClose = std::move(onClose);
    mOpen    = true;

    SetEnabled(true);

    // 位置/サイズ/padding が外から変更されていても正しく反映する
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

//--------------------------------------------------------------
// 入力：A=進む / B=閉じる
//--------------------------------------------------------------
void MessageBoxActor::ActorInput(const InputState& state)
{
    if (!mOpen || !mEnabled) return;

    // A: 文字送り中なら全文表示 / それ以外はページ送り / 終端なら閉じる
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

    // B: 即閉じ（必要なら無効化してもOK）
    if (state.IsButtonPressed(GameButton::B))
    {
        Close();
    }
}

//--------------------------------------------------------------
// 毎フレーム更新
//--------------------------------------------------------------
void MessageBoxActor::UpdateActor(float dt)
{
    if (!mOpen || !mEnabled) return;

    // 文字送りを MessageTextComponent の Update で回しているならここで呼ぶ
    // mText->Update(dt);
}

//--------------------------------------------------------------
// レイアウト反映
//  - 背景：mBoxSize
//  - 本文：mBoxSize + mPadding（内側計算は MessageTextComponent が行う前提）
//  - 本文の開始位置：padding分だけオフセット
//--------------------------------------------------------------
void MessageBoxActor::ApplyLayout()
{
    if (!mBg || !mText) return;

    // 背景サイズ
    mBg->SetScale(mBoxSize.x, mBoxSize.y);

    // 本文のレイアウト条件（外枠サイズ＋padding）
    mText->SetTextBoxSize(mBoxSize);
    mText->SetPadding(mPadding);

    // 本文の描画開始位置を padding だけ内側へ
    mText->SetOffset(Vector3(mPadding.x, mPadding.y, 0.0f));
}

//--------------------------------------------------------------
// Layout setters（Scene側から注入）
//--------------------------------------------------------------
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
