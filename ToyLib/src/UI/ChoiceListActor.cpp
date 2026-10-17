#include "UI/ChoiceListActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/NineSliceSpriteComponent.h"
#include "Graphics/Sprite/TextSpriteComponent.h"

namespace toy {

namespace {
constexpr int kBgDrawOrder     = 900;
constexpr int kLineDrawOrder   = 901;
constexpr int kCursorDrawOrder = 950;
constexpr int kTextDrawOrder   = 1000;
}

ChoiceListActor::ChoiceListActor(Application* app, const Desc& desc)
    : UIScreen(app)
    , mDesc(desc)
{
    SetActorID("ChoiceList");

    auto* assets = app->GetAssetManager();

    // 背景 + 枠線
    mFrame.Create(this, kBgDrawOrder, kLineDrawOrder);
    mFrame.ApplyStyle(mDesc.frame);

    // カーソル帯（角丸）
    mCursor = CreateComponent<NineSliceSpriteComponent>(kCursorDrawOrder, VisualLayer::UI);
    if (mDesc.cursorRadius > 0)
    {
        mCursor->SetTexture(assets->GetRoundedRectTexture(mDesc.cursorRadius));
        mCursor->SetBorder(mDesc.cursorRadius);
    }
    else
    {
        mCursor->SetTexture(assets->GetWhite1x1Texture());
    }
    mCursor->SetColor(mDesc.cursorColor);
    mCursor->SetAlpha(mDesc.cursorAlpha);

    // フォント（未指定ならシステムフォント）
    if (!mDesc.font)
    {
        mDesc.font = app->GetSysAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
    }

    SetVisible(false);
}

//=============================================================================
// Open / Close
//=============================================================================

void ChoiceListActor::Open(const std::vector<std::string>& items,
                           DecideFn onDecide,
                           CancelFn onCancel,
                           int initialIndex)
{
    mItems    = items;
    mOnDecide = std::move(onDecide);
    mOnCancel = std::move(onCancel);
    mOpen     = true;

    // 項目テキストを必要数そろえる
    while (mItemTexts.size() < mItems.size())
    {
        auto* text = CreateComponent<TextSpriteComponent>(kTextDrawOrder, VisualLayer::UI);
        text->SetFont(mDesc.font);
        mItemTexts.push_back(text);
    }
    for (size_t i = 0; i < mItems.size(); ++i)
    {
        mItemTexts[i]->SetText(mItems[i]);
    }

    mSelected = mItems.empty() ? 0 : Math::Clamp(initialIndex, 0, GetItemCount() - 1);

    SetVisible(true);
    SetLayout(ComputeLayout());   // 項目数で高さが変わるので毎回解決し直す
    RefreshItemColors();
    PushToUIStack();
}

void ChoiceListActor::Close()
{
    if (!mOpen) return;

    mOpen = false;
    SetVisible(false);
    RemoveFromUIStack();
}

//=============================================================================
// Selection
//=============================================================================

void ChoiceListActor::SetSelectedIndex(int index)
{
    if (mItems.empty()) return;

    mSelected = Math::Clamp(index, 0, GetItemCount() - 1);
    RefreshItemColors();
    OnLayoutChanged(GetRect());   // カーソル位置を更新
}

void ChoiceListActor::MoveCursor(int delta)
{
    const int count = GetItemCount();
    if (count == 0) return;

    int next = mSelected + delta;
    if (mDesc.wrap)
    {
        next = (next % count + count) % count;
    }
    else
    {
        next = Math::Clamp(next, 0, count - 1);
    }

    if (next != mSelected)
    {
        SetSelectedIndex(next);
    }
}

void ChoiceListActor::Decide()
{
    if (mItems.empty()) return;

    // 閉じてから通知（コールバック内で再 Open / 次の UI を開けるように）
    const int index = mSelected;
    auto onDecide = mOnDecide;
    Close();

    if (onDecide)
    {
        onDecide(index);
    }
}

void ChoiceListActor::Cancel()
{
    auto onCancel = mOnCancel;
    Close();

    if (onCancel)
    {
        onCancel();
    }
}

//=============================================================================
// Input
//=============================================================================

void ChoiceListActor::ActorInput(const InputState& state)
{
    if (!mOpen) return;

    if (state.IsButtonPressed(GameButton::DPadUp) || state.IsButtonPressed(GameButton::KeyW))
    {
        MoveCursor(-1);
    }
    else if (state.IsButtonPressed(GameButton::DPadDown) || state.IsButtonPressed(GameButton::KeyS))
    {
        MoveCursor(+1);
    }

    if (state.IsButtonPressed(GameButton::A))
    {
        Decide();
    }
    else if (mDesc.allowCancel && state.IsButtonPressed(GameButton::B))
    {
        Cancel();
    }
}

//=============================================================================
// Layout / Visual
//=============================================================================

UILayout ChoiceListActor::ComputeLayout() const
{
    UILayout layout = mDesc.layout;
    if (layout.size.y <= 0.0f)
    {
        layout.size.y = mDesc.padding.y * 2.0f + mDesc.itemHeight * static_cast<float>(mItems.size());
    }
    return layout;
}

void ChoiceListActor::OnLayoutChanged(const UIRect& rect)
{
    if (!mCursor) return;

    mFrame.SetRect(GetLocalRect());

    // 項目は内側の矩形に上から並べる（ローカル座標）
    // カーソル帯が枠線に重ならないよう、左右も少し内側へ
    const float sideInset = static_cast<float>(mDesc.frame.lineWidth) + 4.0f;
    const UIRect inner = GetLocalRect().Inset(Vector2(sideInset, mDesc.padding.y));

    for (size_t i = 0; i < mItems.size(); ++i)
    {
        auto* text = mItemTexts[i];
        const float rowTop = inner.Top() + mDesc.itemHeight * static_cast<float>(i);

        // 行の中で縦中央に寄せる
        float textH = 0.0f;
        if (auto tex = text->GetTexture())
        {
            textH = static_cast<float>(tex->GetHeight());
        }
        const float y = rowTop + (mDesc.itemHeight - textH) * 0.5f;

        text->SetOffset(Vector3(GetLocalRect().Left() + mDesc.padding.x, y, 0.0f));
    }

    // カーソル帯：選択行いっぱい
    const float cursorTop = inner.Top() + mDesc.itemHeight * static_cast<float>(mSelected);
    mCursor->SetSize(Vector2(inner.size.x, mDesc.itemHeight));
    mCursor->SetOffset(Vector3(inner.Left(), cursorTop, 0.0f));
}

void ChoiceListActor::RefreshItemColors()
{
    for (size_t i = 0; i < mItems.size(); ++i)
    {
        const bool selected = (static_cast<int>(i) == mSelected);
        mItemTexts[i]->SetColor(selected ? mDesc.selectedTextColor : mDesc.textColor);
    }
}

void ChoiceListActor::SetVisible(bool visible)
{
    const bool hasItems = !mItems.empty();

    mFrame.SetVisible(visible);
    if (mCursor) mCursor->SetVisible(visible && hasItems);

    for (size_t i = 0; i < mItemTexts.size(); ++i)
    {
        // 今回使わない項目は常に隠す
        mItemTexts[i]->SetVisible(visible && i < mItems.size());
    }
}

} // namespace toy
