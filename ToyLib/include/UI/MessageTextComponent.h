#pragma once

#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Utils/MathUtil.h"

#include <string>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

namespace toy {

//==============================================================
// MessageTextComponent
//  - TextSpriteComponent を拡張し、テキストの折り返し/改ページを提供
//  - 背景や入力は扱わない（純粋にテキストレイアウト担当）
//  - SDL3_ttf の実測（TTF_GetStringSize）でピクセル幅折り返し
//==============================================================
class MessageTextComponent : public TextSpriteComponent
{
public:
    MessageTextComponent(class Actor* owner,
                         int drawOrder = 100,
                         VisualLayer layer = VisualLayer::UI);

    // ボックス内の表示領域（背景スプライトの内側サイズ）
    void SetTextBoxSize(const Vector2& size) { mBoxSize = size; }
    void SetPadding(const Vector2& padding) { mPadding = padding; }

    // 行間（px）
    void SetLineGapPx(int px) { mLineGapPx = px; }

    // テキスト差し替え（BuildPagesして先頭ページを表示）
    void SetMessage(const std::string& text);

    // ページ制御
    bool HasNextPage() const;
    void NextPage();
    void ResetPage();

    // 状態参照
    int GetPageCount() const { return (int)mPages.size(); }
    int GetCurrentPageIndex() const { return mCurrentPage; }

private:
    void BuildPages();

    // UTF-8 1文字ぶんのバイト数（最低限）
    static size_t NextUtf8CharBytes(const std::string& s, size_t i);

    // SDL3_ttf 計測
    static int MeasureWidthPx(TTF_Font* font, const std::string& utf8);
    static int GetLineHeightPx(TTF_Font* font);

private:
    std::string mRawText;

    Vector2 mBoxSize = Vector2(640.0f, 160.0f);
    Vector2 mPadding = Vector2(16.0f, 16.0f);

    int mLineGapPx = 2;

    std::vector<std::string> mPages;
    int mCurrentPage = 0;
};

} // namespace toy
