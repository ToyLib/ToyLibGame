#pragma once

#include "Graphics/Sprite/SpriteComponent.h"
#include "Utils/StringUtil.h"
#include <string>
#include <memory>

namespace toy {

//==============================================================
// TextSpriteComponent
//   - テキストを「スプライトとして」描画する UI コンポーネント
//   - 内部でフォントと文字列からテクスチャを生成し、SpriteComponent の仕組みで描画
//   - UI/HUD の文字表示に特化（VisualLayer::UI 前提）
//==============================================================
class TextSpriteComponent : public SpriteComponent
{
public:
    // drawOrder は UI の中の重なり順
    // layer は基本 UI 固定だが、必要なら 2D Overlay として別レイヤーにも置ける
    TextSpriteComponent(class Actor* owner,
                        int drawOrder = 100,
                        VisualLayer layer = VisualLayer::UI);

    virtual ~TextSpriteComponent();

    //----------------------------------------------------------
    // テキスト設定（内部でテクスチャを更新）
    //----------------------------------------------------------
    void SetText(const std::string& text);

    //----------------------------------------------------------
    // フォーマット付き SetText
    //   例）SetFormat("FPS: {:.1f}", fps);
    //----------------------------------------------------------
    template<typename... Args>
    void SetFormat(const std::string& fmt, Args&&... args)
    {
        SetText(StringUtil::Format(fmt, std::forward<Args>(args)...));
    }

    //----------------------------------------------------------
    // テキストカラー (0.0〜1.0)
    //----------------------------------------------------------
    void SetColor(const Vector3& color);

    //----------------------------------------------------------
    // 使用するフォントを設定
    //  - AssetManager の shared_ptr<TextFont> をそのまま受け取る
    //  - 所有権は共有
    //----------------------------------------------------------
    void SetFont(std::shared_ptr<TextFont> font);

    //----------------------------------------------------------
    // テキスト・フォント・カラーの現在設定を元に
    // テクスチャを再生成したいときに明示的に呼ぶ
    //----------------------------------------------------------
    void Refresh();

    // アクセサ
    const std::string& GetText() const { return mText; }
    const Vector3& GetColor()   const { return mColor; }
    std::shared_ptr<class TextFont> GetFont() const { return mFont; }

private:
    //----------------------------------------------------------
    // 内部処理
    //   - mText / mFont / mColor を参照して文字テクスチャ作成
    //----------------------------------------------------------
    void UpdateTexture();

private:
    std::string mText;                      // 表示文字列
    Vector3     mColor;                     // 文字色（0〜1）
    std::shared_ptr<class TextFont> mFont;  // フォント（共有）
};

} // namespace toy
