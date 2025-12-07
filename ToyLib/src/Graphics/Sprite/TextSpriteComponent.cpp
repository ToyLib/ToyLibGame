#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Asset/Font/TextFont.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"

namespace toy {

//==============================================================
// コンストラクタ / デストラクタ
//==============================================================
TextSpriteComponent::TextSpriteComponent(Actor* owner, int drawOrder, VisualLayer layer)
: SpriteComponent(owner, drawOrder, layer)
, mText("")
, mColor(1.0f, 1.0f, 1.0f)   // デフォルトは白文字
, mFont(nullptr)
{
}

TextSpriteComponent::~TextSpriteComponent()
{
    // 特に解放処理は不要（Texture / Font は shared_ptr により管理）
}

//==============================================================
// テキスト関連
//==============================================================

// テキスト内容を変更すると、テクスチャを再生成
void TextSpriteComponent::SetText(const std::string& text)
{
    // 変更なしなら早期リターン（余計な再生成を避ける）
    if (mText == text)
    {
        return;
    }

    mText = text;
    UpdateTexture();
}

// 文字色を変更したら、テクスチャを再生成
void TextSpriteComponent::SetColor(const Vector3& color)
{
    mColor = color;
    UpdateTexture();
}

// フォントを差し替えたら、テキスト用テクスチャを作り直す
void TextSpriteComponent::SetFont(std::shared_ptr<TextFont> font)
{
    mFont = font;
    UpdateTexture();
}

// 明示的に「今の設定で作り直したい」場合に呼ぶ
void TextSpriteComponent::Refresh()
{
    UpdateTexture();
}

//==============================================================
// 内部：テクスチャ更新
//==============================================================
void TextSpriteComponent::UpdateTexture()
{
    //----------------------------------------------------------
    // 前提条件チェック
    //   ・テキストが空
    //   ・フォント未設定
    //   ・フォントが無効（ロード失敗など）
    // いずれかなら、TextSprite としては「何も表示しない」状態にする
    //----------------------------------------------------------
    if (mText.empty() || !mFont || !mFont->IsValid())
    {
        SetTexture(nullptr);
        return;
    }

    auto* app      = GetOwner()->GetApp();
    auto* renderer = app->GetRenderer();

    //----------------------------------------------------------
    // Renderer にフォント描画を委譲して、文字列テクスチャを生成
    //----------------------------------------------------------
    auto tex = renderer->CreateTextTexture(mText, mColor, mFont);
    if (!tex)
    {
        // テキスト生成に失敗した場合は、描画を止める
        SetTexture(nullptr);
        return;
    }

    //----------------------------------------------------------
    // SpriteComponent 側にテクスチャを渡す
    //   - SpriteComponent::SetTexture() が内部で
    //     mTexWidth / mTexHeight を更新してくれる
    //----------------------------------------------------------
    SetTexture(tex);

    // サイズスケーリングしたい場合は、外側から
    //   SetScale(w, h);
    // を明示的に呼ぶ前提（ここでは触らない）
}

} // namespace toy
