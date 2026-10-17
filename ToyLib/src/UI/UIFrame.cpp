#include "UI/UIFrame.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Sprite/NineSliceSpriteComponent.h"

#include <algorithm>

namespace toy {

void UIFrame::Create(Actor* owner, int bgDrawOrder, int lineDrawOrder)
{
    mOwner = owner;
    mBg    = owner->CreateComponent<NineSliceSpriteComponent>(bgDrawOrder,   VisualLayer::UI);
    mLine  = owner->CreateComponent<NineSliceSpriteComponent>(lineDrawOrder, VisualLayer::UI);

    ApplyStyle(mStyle);
}

void UIFrame::ApplyStyle(const UIFrameStyle& style)
{
    mStyle = style;
    if (!mBg || !mLine) return;

    auto* assets = mOwner->GetApp()->GetAssetManager();

    // 背景：角丸の塗り（radius <= 0 なら白 1x1 を中央ピースだけで伸ばす）
    if (mStyle.radius > 0)
    {
        mBg->SetTexture(assets->GetRoundedRectTexture(mStyle.radius));
        mBg->SetBorder(mStyle.radius);
    }
    else
    {
        mBg->SetTexture(assets->GetWhite1x1Texture());
        mBg->SetBorder(0);
    }
    mBg->SetColor(mStyle.bgColor);
    mBg->SetAlpha(mStyle.bgAlpha);

    // 枠線：角丸のリング（角丸なしでも半径 = 線幅のリングで四角い枠になる）
    if (mStyle.lineWidth > 0)
    {
        const int r = std::max(mStyle.radius, mStyle.lineWidth);
        mLine->SetTexture(assets->GetRoundedRectTexture(r, mStyle.lineWidth));
        mLine->SetBorder(r);
    }
    mLine->SetColor(mStyle.lineColor);
    mLine->SetAlpha(mStyle.lineAlpha);

    SetVisible(mVisible);
}

void UIFrame::SetRect(const UIRect& localRect)
{
    if (!mBg || !mLine) return;

    const Vector3 offset(localRect.pos.x, localRect.pos.y, 0.0f);

    mBg->SetOffset(offset);
    mBg->SetSize(localRect.size);

    mLine->SetOffset(offset);
    mLine->SetSize(localRect.size);
}

void UIFrame::SetVisible(bool visible)
{
    mVisible = visible;
    if (mBg)   mBg->SetVisible(visible);
    if (mLine) mLine->SetVisible(visible && mStyle.lineWidth > 0);
}

} // namespace toy
