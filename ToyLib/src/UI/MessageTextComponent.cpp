#include "UI/MessageTextComponent.h"

#include "Asset/Font/TextFont.h"
#include "Utils/StringUtil.h"

#include <algorithm>

namespace toy {

MessageTextComponent::MessageTextComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : TextSpriteComponent(owner, drawOrder, layer)
{
}

void MessageTextComponent::SetMessage(const std::string& text)
{
    mRawText = text;
    BuildPages();
    ResetPage();
}

bool MessageTextComponent::HasNextPage() const
{
    return mCurrentPage + 1 < (int)mPages.size();
}

void MessageTextComponent::NextPage()
{
    if (!HasNextPage()) return;
    ++mCurrentPage;
    SetText(mPages[mCurrentPage]);
}

void MessageTextComponent::ResetPage()
{
    mCurrentPage = 0;
    if (!mPages.empty())
        SetText(mPages[0]);
    else
        SetText("");
}

//----------------------------------
// SDL3_ttf 計測
//----------------------------------
int MessageTextComponent::MeasureWidthPx(TTF_Font* font, const std::string& utf8)
{
    if (!font || utf8.empty()) return 0;

    int w = 0, h = 0;

    // SDL3_ttf: true=success
    if (!TTF_GetStringSize(font, utf8.c_str(), utf8.size(), &w, &h))
    {
        // 失敗時
        return 0;
    }

    return w;
}

int MessageTextComponent::GetLineHeightPx(TTF_Font* font)
{
    if (!font) return 0;
    return TTF_GetFontHeight(font);
}

//----------------------------------
// UTF-8 next
//----------------------------------
size_t MessageTextComponent::NextUtf8CharBytes(const std::string& s, size_t i)
{
    const unsigned char c = (unsigned char)s[i];
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

//----------------------------------
// BuildPages（本体）
//----------------------------------
void MessageTextComponent::BuildPages()
{
    mPages.clear();
    mCurrentPage = 0;

    auto fontPtr = GetFont();
    if (!fontPtr || !fontPtr->IsValid())
        return;

    TTF_Font* font = fontPtr->GetNativeFont();
    if (!font) return;

    const float textWidthF  = mBoxSize.x - mPadding.x * 2.0f;
    const float textHeightF = mBoxSize.y - mPadding.y * 2.0f;
    if (textWidthF <= 0.0f || textHeightF <= 0.0f)
        return;

    const int maxWidthPx = (int)textWidthF;

    const int lineHeight = GetLineHeightPx(font);
    const int stepY      = std::max(1, lineHeight + mLineGapPx);

    const int linesPerPage = (int)(textHeightF / (float)stepY);
    if (linesPerPage <= 0) return;

    // 1) 明示改行で分割（空行保持）
    auto rawLines = StringUtil::Split(mRawText, '\n');

    // 2) 折り返し後の行配列
    std::vector<std::string> wrapped;
    wrapped.reserve(rawLines.size() * 2);

    for (auto line : rawLines)
    {
        // Windows改行対策
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
        {
            wrapped.emplace_back("");
            continue;
        }

        std::string cur;
        size_t i = 0;

        while (i < line.size())
        {
            const size_t n = NextUtf8CharBytes(line, i);
            const std::string add = line.substr(i, n);

            // 追加したらどうなる？
            std::string next = cur + add;

            // 1文字も入らないほど狭い場合：その文字だけで1行
            if (cur.empty() && MeasureWidthPx(font, add) > maxWidthPx)
            {
                wrapped.emplace_back(add);
                i += n;
                continue;
            }

            // 幅オーバーなら確定して改行（文字は次行で再処理）
            if (!cur.empty() && MeasureWidthPx(font, next) > maxWidthPx)
            {
                wrapped.emplace_back(cur);
                cur.clear();
                continue;
            }

            // 収まるなら採用
            cur = std::move(next);
            i += n;
        }

        if (!cur.empty())
            wrapped.emplace_back(cur);
    }

    // 3) ページ化（linesPerPageごとに Join）
    for (size_t idx = 0; idx < wrapped.size(); )
    {
        std::vector<std::string> pageLines;
        pageLines.reserve(linesPerPage);

        for (int n = 0; n < linesPerPage && idx < wrapped.size(); ++n, ++idx)
        {
            pageLines.push_back(wrapped[idx]);
        }

        mPages.push_back(StringUtil::Join(pageLines, "\n"));
    }
}

} // namespace toy
