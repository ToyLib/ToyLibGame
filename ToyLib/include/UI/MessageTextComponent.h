#pragma once

#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Utils/MathUtil.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <vector>
#include <cstddef> // size_t

namespace toy {

//==============================================================
// MessageTextComponent
//------------------------------------------------------------------------------
// TextSpriteComponent を拡張し、会話UI向けの
//  「折り返し」「改ページ」「文字送り（タイプライタ）」を提供する。
// 背景スプライトや入力は扱わず、“文字レイアウト専用コンポーネント” に徹する。
//
// 特徴
// ・SDL3_ttf の実測（TTF_GetStringSize）で “ピクセル幅” で折り返す
//   → 等幅前提にしないので日本語フォントでも自然に折り返せる
// ・改ページ：表示領域の高さから 1ページの最大行数を計算してページ分割
// ・文字送り：UTF-8 をバイト境界で切らないように “文字境界テーブル” を持って進める
//   → 途中で「□」が出る問題（不正UTF-8）を回避
//
// 使い方（基本）
// ・SetTextBoxSize / SetPadding / SetLineGapPx でレイアウト条件を設定
// ・SetMessage(text) で BuildPages して 1ページ目を表示開始
// ・Update(dt) を呼ぶ（Actor側から）ことで文字送りを進める
// ・Aキー等で FinishTyping() / NextPage() を呼ぶのは “上位（Actor/Scene）” の責務
//==============================================================
class MessageTextComponent : public TextSpriteComponent
{
public:
    //--------------------------------------------------------------------------
    // ctor
    //--------------------------------------------------------------------------
    // drawOrder / layer は UI 用の描画順・レイヤーを指定する。
    //--------------------------------------------------------------------------
    MessageTextComponent(class Actor* owner,
                         int drawOrder = 100,
                         VisualLayer layer = VisualLayer::UI);

    //--------------------------------------------------------------------------
    // Layout setters
    //--------------------------------------------------------------------------
    // ・TextBoxSize : “背景外枠” のサイズを渡す想定（padding は別で渡す）
    // ・Padding     : 背景内側の余白（折り返し幅・ページ行数に影響する）
    //--------------------------------------------------------------------------
    void SetTextBoxSize(const Vector2& size) { mBoxSize = size; }
    void SetPadding(const Vector2& padding) { mPadding = padding; }

    // 行間（px）
    void SetLineGapPx(int px) { mLineGapPx = px; }

    //--------------------------------------------------------------------------
    // Message API
    //--------------------------------------------------------------------------
    // テキスト差し替え：
    // ・内部でページ分割（BuildPages）
    // ・1ページ目を表示開始（ResetPage）
    // ・文字送りを開始（mTyping=true）
    //--------------------------------------------------------------------------
    void SetMessage(const std::string& text);

    //--------------------------------------------------------------------------
    // Page control
    //--------------------------------------------------------------------------
    // ・NextPage(): 次ページへ進む（ページ境界テーブルも作り直す）
    // ・ResetPage(): 先頭へ戻す
    //--------------------------------------------------------------------------
    bool HasNextPage() const;
    void NextPage();
    void ResetPage();

    // 状態参照
    int GetPageCount() const { return static_cast<int>(mPages.size()); }
    int GetCurrentPageIndex() const { return mCurrentPage; }

    //--------------------------------------------------------------------------
    // Typewriter (typing) control
    //--------------------------------------------------------------------------
    // Update(dt):
    // ・文字送り中（mTyping=true）の場合、速度に応じて表示文字数を進める
    // ・表示は UTF-8 の “文字境界” で切る（バイト切りしない）
    //
    // ※ToyLib の Component 体系に合わせて、
    //   Actor 側の UpdateActor() から呼ぶ運用でもOK。
    //--------------------------------------------------------------------------
    void Update(float deltaTime) override;

    bool IsTyping() const { return mTyping; }

    // 文字送りを即完了（全文表示）
    void FinishTyping();

    //--------------------------------------------------------------------------
    // Internal helpers (utility)
    //--------------------------------------------------------------------------
    // 現在ページの “文字境界テーブル” を構築する。
    // ・ページ切り替え／SetMessage のたびに内部から呼ぶ想定。
    // ・外部から呼ぶ必要は基本的にない（public だが内部向け）。
    //--------------------------------------------------------------------------
    void BuildCharEndsForPage();

private:
    // ページ分割（折り返し＋改ページ）
    void BuildPages();

    // UTF-8 1文字ぶんのバイト数（簡易版：1〜4バイト）
    static size_t NextUtf8CharBytes(const std::string& s, size_t i);

    // SDL3_ttf 計測：文字列の描画幅（px）
    static int MeasureWidthPx(TTF_Font* font, const std::string& utf8);

    // SDL3_ttf：フォントの行高さ（px）
    static int GetLineHeightPx(TTF_Font* font);

private:
    // 元の全文（\n を含む想定）
    std::string mRawText;

    // レイアウト条件（外部から注入）
    Vector2 mBoxSize;
    Vector2 mPadding;
    int     mLineGapPx;

    // ページ分割結果（各ページは “表示用の完成文字列”）
    std::vector<std::string> mPages;
    int mCurrentPage;

    //--------------------------------------------------------------------------
    // Typewriter state
    //--------------------------------------------------------------------------
    // mCharSpeed : 1秒あたり表示する “文字数” の目安
    // mCharAcc   : 文字送り用の蓄積（speed * dt）
    //
    // ※UTF-8 の “文字数” と substr の “バイト数” を混同しないこと！
    //   表示には mCharEnds を使って “文字数→バイト長” に変換する。
    //--------------------------------------------------------------------------
    float  mCharSpeed;     // chars/sec
    float  mCharAcc;
    bool   mTyping;

    // i文字目までの “バイト長” を格納（UTF-8境界）
    // 例：mCharEnds[0] = 1文字目までのバイト数
    std::vector<size_t> mCharEnds;

    // 表示している “文字数”（0..mCharEnds.size）
    size_t mVisibleCharCount;

    // NOTE:
    // mVisibleChars (SIZE_MAX など) を別に持つと二重管理になりやすいので、
    // ここでは mVisibleCharCount を正とする運用を推奨。
    //（既に実装済みで mVisibleChars を使っているなら、整理するタイミングで統一すると良い）
};

} // namespace toy
