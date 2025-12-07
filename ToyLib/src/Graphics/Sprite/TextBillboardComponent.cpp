#include "Graphics/Sprite/TextBillboardComponent.h"
#include "Asset/Font/TextFont.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"

#include <iostream>

namespace toy {

//==============================================================
// コンストラクタ / デストラクタ
//==============================================================
TextBillboardComponent::TextBillboardComponent(Actor* owner,
                                               int drawOrder)
: BillboardComponent(owner, drawOrder)
, mText("")
, mColor(1.0f, 1.0f, 1.0f)   // デフォルトは白文字
, mFont(nullptr)
, mIsDirty(true)
{
    // BillboardComponent 側は Effect3D 固定なので、
    // 必要ならここでレイヤーを上書き

    // 必要ならここでスケールも初期値を変えておける
    // 例: ピクセル→ワールドスケール
    // SetScale(0.01f);
}

TextBillboardComponent::~TextBillboardComponent()
{
    // 特に解放処理は不要（Texture / Font は shared_ptr により管理）
}

//==============================================================
// テキスト関連 (Dirty 方式)
//==============================================================

void TextBillboardComponent::SetText(const std::string& text)
{
    if (mText == text)
    {
        return;
    }
    mText = text;
    mIsDirty = true;    // 内容変わったので再生成が必要
}

void TextBillboardComponent::SetColor(const Vector3& color)
{
    //if (mColor == color)
    //{
    //    return;
    //}
    mColor = color;
    mIsDirty = true;    // 色もテクスチャに焼き込むので再生成が必要
}

void TextBillboardComponent::SetFont(std::shared_ptr<TextFont> font)
{
    if (mFont == font)
    {
        return;
    }
    mFont = std::move(font);
    mIsDirty = true;    // フォント変更も再生成が必要
}

void TextBillboardComponent::Refresh()
{
    mIsDirty = true;
    UpdateTexture();
}

//==============================================================
// Draw
//  - 描画前に Dirty チェックしてテクスチャを更新
//==============================================================
void TextBillboardComponent::Draw()
{
    if (mIsDirty)
    {
        UpdateTexture();
    }

    // フォント未設定 / テキスト空 / テクスチャ無しなら何も描画しない
    if (!mFont || mText.empty() || !mTexture)
    {
        return;
    }

    // 通常のビルボード描画
    BillboardComponent::Draw();
}

//==============================================================
// 内部：テクスチャ更新
//==============================================================
void TextBillboardComponent::UpdateTexture()
{
    mIsDirty = false;

    //----------------------------------------------------------
    // 前提条件チェック
    //----------------------------------------------------------
    if (mText.empty() || !mFont || !mFont->IsValid())
    {
        SetTexture(nullptr);
        return;
    }

    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        SetTexture(nullptr);
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        SetTexture(nullptr);
        return;
    }

    //----------------------------------------------------------
    // Renderer にフォント描画を委譲して、文字列テクスチャを生成
    //   - 現時点では 1行前提（改行は後で対応）
    //----------------------------------------------------------
    auto tex = renderer->CreateTextTexture(mText, mColor, mFont);
    if (!tex)
    {
        // テキスト生成に失敗した場合は、描画を止める
        SetTexture(nullptr);
        return;
    }

    //----------------------------------------------------------
    // BillboardComponent(≒VisualComponent) 側にテクスチャを渡す
    //----------------------------------------------------------
    SetTexture(tex);

    // ビルボードのワールド上の大きさは、外から SetScale()（を追加してあれば）
    // で調整してもらう方針。ここでは触らない。
}

} // namespace toy
