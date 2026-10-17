#pragma once

#include "UI/UIScreen.h"
#include "UI/UIFrame.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toy {

class NineSliceSpriteComponent;
class TextSpriteComponent;
class TextFont;

//==============================================================
// ChoiceListActor
//  - 縦並びの選択肢リスト（タイトルメニュー / 会話の選択肢 / はい・いいえ 等）
//  - 背景・枠線(UIFrame) + カーソル帯(9-slice) + 項目(TextSprite × N)
//  - Open で UIStack に積み、決定 / キャンセルで閉じてからコールバックを呼ぶ
//    （コールバック内で次の UI を開いたりシーンを切り替えてよい）
//
// 操作
//  - 上下：DPadUp / DPadDown（W / S も可）
//  - 決定：A    キャンセル：B（Desc.allowCancel が true のとき）
//==============================================================
class ChoiceListActor : public UIScreen
{
public:
    struct Desc
    {
        // ----------------------------
        // レイアウト
        //  - layout.size.y <= 0 なら項目数から高さを自動計算する
        // ----------------------------
        UILayout layout     { DefaultLayout() };
        Vector2  padding    { Vector2(16.0f, 12.0f) };
        float    itemHeight { 36.0f };

        // ----------------------------
        // 見た目
        // ----------------------------
        UIFrameStyle frame      { DefaultFrame() };       // 背景 + 枠線（角丸）
        UITransition transition { DefaultTransition() };  // 開閉アニメーション

        Vector3 cursorColor { Vector3(0.3f, 0.5f, 0.9f) };
        float   cursorAlpha { 0.5f };
        int     cursorRadius { 4 };   // カーソル帯の角丸（0 で四角）

        Vector3 textColor         { Vector3(0.7f, 0.7f, 0.7f) };
        Vector3 selectedTextColor { Vector3(1.0f, 1.0f, 1.0f) };

        // ----------------------------
        // フォント（未指定ならシステムフォントを使用）
        // ----------------------------
        std::shared_ptr<TextFont> font;
        std::string defaultFontPath { "rounded-mplus-1c-bold.ttf" };
        int defaultFontSize { 22 };

        // ----------------------------
        // 操作
        // ----------------------------
        bool allowCancel { true };   // B でキャンセル
        bool wrap        { true };   // 端で反対側へ回り込む

        static UIFrameStyle DefaultFrame()
        {
            UIFrameStyle f;
            f.bgAlpha = 0.7f;
            return f;
        }

        static UITransition DefaultTransition()
        {
            UITransition t;
            t.slideFrom = Vector2(0.0f, 8.0f);
            return t;
        }

        static UILayout DefaultLayout()
        {
            UILayout l;
            l.anchor = UIAnchor::Center;
            l.size   = Vector2(260.0f, 0.0f);
            return l;
        }
    };

    using DecideFn = std::function<void(int index)>;
    using CancelFn = std::function<void()>;

public:
    ChoiceListActor(class Application* app, const Desc& desc);
    ~ChoiceListActor() override = default;

    void ActorInput(const struct InputState& state) override;

    void Open(const std::vector<std::string>& items,
              DecideFn onDecide,
              CancelFn onCancel = nullptr,
              int initialIndex = 0);
    void Close();

    bool IsOpen() const { return mOpen; }

    // 表示位置の差し替え（次の Open から反映。他の UI に添えて出す場合など）
    void SetListLayout(const UILayout& layout) { mDesc.layout = layout; }
    const UILayout& GetListLayout() const { return mDesc.layout; }   // size.y <= 0 なら高さ自動

    void SetSelectedIndex(int index);
    int  GetSelectedIndex() const { return mSelected; }
    int  GetItemCount() const { return static_cast<int>(mItems.size()); }

protected:
    void OnLayoutChanged(const UIRect& rect) override;
    void OnHidden() override;

private:
    void Decide();
    void Cancel();
    void MoveCursor(int delta);

    void SetVisible(bool visible);
    void RefreshItemColors();
    UILayout ComputeLayout() const;

private:
    Desc mDesc;

    UIFrame                   mFrame;
    NineSliceSpriteComponent* mCursor { nullptr };

    // 項目テキスト（Open のたびに不足分だけ作る。余りは非表示で使い回す）
    std::vector<TextSpriteComponent*> mItemTexts;
    std::vector<std::string>          mItems;

    int  mSelected { 0 };
    bool mOpen     { false };

    DecideFn mOnDecide;
    CancelFn mOnCancel;
};

} // namespace toy
