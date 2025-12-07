#pragma once

#include "Graphics/Sprite/BillboardComponent.h"
#include "Utils/StringUtil.h"
#include "Utils/MathUtil.h"

#include <string>
#include <memory>

namespace toy {

//==============================================================
// TextBillboardComponent
//   - テキストを「ビルボードとして」3D空間に描画するコンポーネント
//   - 内部でフォントと文字列からテクスチャを生成し、
//     BillboardComponent の仕組みで描画
//   - テキスト/色/フォント変更時に Dirty フラグを立て、
//     Draw() 時にまとめてテクスチャを更新する。
//==============================================================
class TextBillboardComponent : public BillboardComponent
{
public:
    // drawOrder は 3D レイヤー内の描画順
    // layer は 3D テキストを置きたいレイヤー（デフォルト Effect3D）
    TextBillboardComponent(class Actor* owner,
                           int drawOrder = 100);

    virtual ~TextBillboardComponent();

    //----------------------------------------------------------
    // テキスト設定（内部で Dirty フラグを立てる）
    //----------------------------------------------------------
    void SetText(const std::string& text);

    //----------------------------------------------------------
    // フォーマット付き SetText
    //   例）SetFormat("HP: {}/{}", hp, maxHp);
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
    //----------------------------------------------------------
    void SetFont(std::shared_ptr<class TextFont> font);

    //----------------------------------------------------------
    // テキスト・フォント・カラーの現在設定を元に
    // テクスチャを再生成したいときに明示的に呼ぶ
    //----------------------------------------------------------
    void Refresh();

    // アクセサ
    const std::string& GetText() const { return mText; }
    const Vector3&     GetColor() const { return mColor; }
    std::shared_ptr<class TextFont> GetFont() const { return mFont; }

    //----------------------------------------------------------
    // Draw オーバーライド
    //  - 描画前に Dirty フラグを見て UpdateTexture() を呼ぶ
    //----------------------------------------------------------
    void Draw() override;

private:
    //----------------------------------------------------------
    // 内部：テクスチャ更新
    //   - mText / mFont / mColor を参照して文字テクスチャ作成
    //----------------------------------------------------------
    void UpdateTexture();

private:
    std::string mText;                      // 表示文字列
    Vector3     mColor;                     // 文字色（0〜1）
    std::shared_ptr<class TextFont> mFont;  // フォント（共有）

    bool        mIsDirty;                   // テクスチャ再生成が必要かどうか
};

} // namespace toy
