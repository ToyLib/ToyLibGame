#include "UI/MessageTextComponent.h"

#include "Asset/Font/TextFont.h"
#include "Utils/StringUtil.h"

#include <algorithm> // std::max

namespace toy {

//==============================================================
// ctor
//==============================================================
MessageTextComponent::MessageTextComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : TextSpriteComponent(owner, drawOrder, layer)
    , mBoxSize(Vector2(640.0f, 160.0f))
    , mPadding(Vector2(16.0f, 16.0f))
    , mLineGapPx(2)
    , mCurrentPage(0)
    , mCharSpeed(30.0f)     // chars/sec
    , mCharAcc(0.0f)
    , mTyping(false)
    , mVisibleCharCount(0)
{
}

//==============================================================
// SetMessage
//  - 生テキストを受け取ってページ分割（BuildPages）
//  - 先頭ページへ戻し、文字送りを開始（ResetPage）
//==============================================================
void MessageTextComponent::SetMessage(const std::string& text)
{
    mRawText = text;
    BuildPages();
    ResetPage();
}

//==============================================================
// Page control
//==============================================================
bool MessageTextComponent::HasNextPage() const
{
    return mCurrentPage + 1 < static_cast<int>(mPages.size());
}

void MessageTextComponent::NextPage()
{
    if (!HasNextPage()) return;

    ++mCurrentPage;

    // 次ページの文字境界テーブルを作り直して、文字送りを開始
    if (!mPages.empty())
    {
        BuildCharEndsForPage();
        mVisibleCharCount = 0;
        mCharAcc = 0.0f;
        mTyping = true;

        SetText(""); // 最初は空（タイプライタ開始）
    }
}

void MessageTextComponent::ResetPage()
{
    mCurrentPage = 0;

    // 先頭ページの文字境界テーブルを作って、文字送りを開始
    if (!mPages.empty())
    {
        BuildCharEndsForPage();
        mVisibleCharCount = 0;
        mCharAcc = 0.0f;
        mTyping = true;

        SetText(""); // 最初は空（タイプライタ開始）
    }
}

//==============================================================
// SDL3_ttf: text measurement
//  - ピクセル幅で折り返すために利用
//==============================================================
int MessageTextComponent::MeasureWidthPx(TTF_Font* font, const std::string& utf8)
{
    if (!font || utf8.empty())
    {
        return 0;
    }
    int w = 0, h = 0;

    // SDL3_ttf: true = success
    if (!TTF_GetStringSize(font, utf8.c_str(), utf8.size(), &w, &h))
    {
        return 0;
    }

    return w;
}

int MessageTextComponent::GetLineHeightPx(TTF_Font* font)
{
    if (!font) return 0;
    return TTF_GetFontHeight(font);
}

//==============================================================
// UTF-8 helper
//  - 先頭バイトから「この文字のバイト長」を返す簡易版（1〜4）
//  - 日本語の会話UI用途ならこれで十分実用的
//==============================================================
size_t MessageTextComponent::NextUtf8CharBytes(const std::string& s, size_t i)
{
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if ((c & 0x80) == 0x00)
    {
        return 1;
    }
    if ((c & 0xE0) == 0xC0)
    {
        return 2;
    }
    if ((c & 0xF0) == 0xE0)
    {
        return 3;
    }
    if ((c & 0xF8) == 0xF0)
    {
        return 4;
    }
    return 1;
}

//==============================================================
// Update (typewriter)
//  - 文字送り（タイプライタ）中のみ動く
//  - UTF-8 をバイト途中で切らないように、mCharEnds を使って substr する
//    → 不正UTF-8による「□」表示を防止
//==============================================================
void MessageTextComponent::Update(float dt)
{
    if (!mTyping)
    {
        return;
    }
    if (mPages.empty() || mCurrentPage >= static_cast<int>(mPages.size()))
    {
        return;
    }
    const std::string& page = mPages[mCurrentPage];

    // “文字数” を dt で進める（小数は mCharAcc に蓄積）
    mCharAcc += dt * mCharSpeed;
    const size_t target = static_cast<size_t>(mCharAcc);

    // ページ末尾まで到達したら完了（全文表示）
    if (target >= mCharEnds.size())
    {
        mVisibleCharCount = mCharEnds.size();
        mTyping = false;
        SetText(page);
        return;
    }

    mVisibleCharCount = target;

    // 0文字なら空
    if (mVisibleCharCount == 0)
    {
        SetText("");
        return;
    }

    // “文字数 -> バイト長” に変換して substr
    const size_t byteLen = mCharEnds[mVisibleCharCount - 1];
    SetText(page.substr(0, byteLen));
}

//==============================================================
// BuildPages
//  - 生テキスト(mRawText)を、表示領域(mBoxSize - padding)に収まるように
//    「折り返し」「改ページ」して mPages に格納する
//
// 処理手順
//  1) 明示改行 '\n' で分割（空行も保持）
//  2) 各行を “ピクセル幅” で折り返して wrapped に積む（UTF-8 1文字単位）
//  3) 表示高さから算出した linesPerPage ごとに Join してページ化
//==============================================================
void MessageTextComponent::BuildPages()
{
    mPages.clear();
    mCurrentPage = 0;
    
    // フォントが無ければ何もできない
    auto fontPtr = GetFont();
    if (!fontPtr || !fontPtr->IsValid())
    {
        return;
    }
    TTF_Font* font = fontPtr->GetNativeFont();
    if (!font)
    {
        return;
    }
    // テキスト表示領域（背景外枠 - padding*2）
    const float textWidthF  = mBoxSize.x - mPadding.x * 2.0f;
    const float textHeightF = mBoxSize.y - mPadding.y * 2.0f;
    if (textWidthF <= 0.0f || textHeightF <= 0.0f)
    {
        return;
    }
    const int maxWidthPx = static_cast<int>(textWidthF);

    // 行高さ（フォント）＋行間
    const int lineHeight = GetLineHeightPx(font);
    const int stepY      = std::max(1, lineHeight + mLineGapPx);

    // 1ページあたりの最大行数
    const int linesPerPage = static_cast<int>(textHeightF / static_cast<float>(stepY));
    if (linesPerPage <= 0)
    {
        return;
    }
    
    // 1) 明示改行で分割（空行保持）
    auto rawLines = StringUtil::Split(mRawText, '\n');

    // 2) 折り返し後の行配列
    std::vector<std::string> wrapped;
    wrapped.reserve(rawLines.size() * 2);

    for (auto line : rawLines)
    {
        // Windows改行対策（"\r\n" の '\r' を除去）
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // 空行はそのまま1行として保持
        if (line.empty())
        {
            wrapped.emplace_back("");
            continue;
        }

        std::string cur;
        size_t i = 0;

        // UTF-8 を 1文字単位で増やしながら “ピクセル幅” を超えたら折り返す
        while (i < line.size())
        {
            const size_t n   = NextUtf8CharBytes(line, i);
            const std::string add = line.substr(i, n);

            const std::string next = cur + add;

            // 1文字も入らないほど狭い場合：その文字だけで1行にする
            if (cur.empty() && MeasureWidthPx(font, add) > maxWidthPx)
            {
                wrapped.emplace_back(add);
                i += n;
                continue;
            }

            // 幅オーバーなら現在行を確定して改行（この文字は次行で再処理）
            if (!cur.empty() && MeasureWidthPx(font, next) > maxWidthPx)
            {
                wrapped.emplace_back(cur);
                cur.clear();
                continue;
            }

            // 収まるなら採用
            cur = next;
            i += n;
        }

        // 行末に残った分を追加
        if (!cur.empty())
        {
            wrapped.emplace_back(cur);
        }
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

//==============================================================
// BuildCharEndsForPage
//  - 現在ページ文字列の UTF-8 “文字境界” を作る
//  - mCharEnds[k] = k+1文字目までの “バイト長”
//    → substr(0, mCharEnds[x]) で安全に切れる
//==============================================================
void MessageTextComponent::BuildCharEndsForPage()
{
    mCharEnds.clear();

    if (mPages.empty() || mCurrentPage >= static_cast<int>(mPages.size()))
    {
        return;
    }
    const std::string& page = mPages[mCurrentPage];

    size_t i = 0;
    while (i < page.size())
    {
        const size_t n = NextUtf8CharBytes(page, i);
        i += n;
        mCharEnds.push_back(i);
    }
}

//==============================================================
// FinishTyping
//  - 文字送り中なら即完了（全文表示）
//==============================================================
void MessageTextComponent::FinishTyping()
{
    if (!mTyping)
    {
        return;
    }
    mTyping = false;

    if (mPages.empty() || mCurrentPage >= static_cast<int>(mPages.size()))
    {
        SetText("");
        return;
    }

    SetText(mPages[mCurrentPage]);
}

} // namespace toy
