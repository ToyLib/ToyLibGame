#include "UI/UILayout.h"

#include "Render/IRenderer.h"

namespace toy {

namespace {

// アンカーの正規化座標（0, 0.5, 1）
Vector2 AnchorRatio(UIAnchor anchor)
{
    switch (anchor)
    {
        case UIAnchor::TopLeft:     return Vector2(0.0f, 0.0f);
        case UIAnchor::Top:         return Vector2(0.5f, 0.0f);
        case UIAnchor::TopRight:    return Vector2(1.0f, 0.0f);
        case UIAnchor::Left:        return Vector2(0.0f, 0.5f);
        case UIAnchor::Center:      return Vector2(0.5f, 0.5f);
        case UIAnchor::Right:       return Vector2(1.0f, 0.5f);
        case UIAnchor::BottomLeft:  return Vector2(0.0f, 1.0f);
        case UIAnchor::Bottom:      return Vector2(0.5f, 1.0f);
        case UIAnchor::BottomRight: return Vector2(1.0f, 1.0f);
    }
    return Vector2(0.0f, 0.0f);
}

} // namespace

//=============================================================================
// UIRect
//=============================================================================

Vector2 UIRect::GetPoint(UIAnchor anchor) const
{
    const Vector2 r = AnchorRatio(anchor);
    return Vector2(pos.x + size.x * r.x, pos.y + size.y * r.y);
}

UIRect UIRect::Inset(const Vector2& padding) const
{
    UIRect r;
    r.pos  = Vector2(pos.x + padding.x, pos.y + padding.y);
    r.size = Vector2(Math::Max(0.0f, size.x - padding.x * 2.0f),
                     Math::Max(0.0f, size.y - padding.y * 2.0f));
    return r;
}

//=============================================================================
// UILayout
//=============================================================================

UIRect UILayout::Resolve(const UIRect& parent) const
{
    const Vector2 anchorPt = parent.GetPoint(anchor);
    const Vector2 pivotR   = AnchorRatio(pivot.value_or(anchor));

    UIRect r;
    r.size = size;
    r.pos  = Vector2(anchorPt.x + offset.x - size.x * pivotR.x,
                     anchorPt.y + offset.y - size.y * pivotR.y);

    const bool stretchX = (stretch == UIStretch::Horizontal || stretch == UIStretch::Both);
    const bool stretchY = (stretch == UIStretch::Vertical   || stretch == UIStretch::Both);

    if (stretchX)
    {
        r.pos.x  = parent.pos.x + stretchMargin.x + offset.x;
        r.size.x = Math::Max(0.0f, parent.size.x - stretchMargin.x * 2.0f);
    }
    if (stretchY)
    {
        r.pos.y  = parent.pos.y + stretchMargin.y + offset.y;
        r.size.y = Math::Max(0.0f, parent.size.y - stretchMargin.y * 2.0f);
    }

    return r;
}

//=============================================================================
// Root
//=============================================================================

UIRect GetUIRootRect(const IRenderer* renderer)
{
    UIRect root;

    if (!renderer)
    {
        root.size = Vector2(1280.0f, 720.0f);
        return root;
    }

    const UIScaleInfo ui = renderer->GetUIScaleInfo();
    const float scale = (ui.scale > 0.0f) ? ui.scale : 1.0f;

    // SpriteComponent は「offset + 論理座標 * scale」で画面に置くので、
    // 実画面全体は論理座標で (screen - offset) / scale の大きさになる
    root.pos  = Vector2(-ui.offsetX / scale, -ui.offsetY / scale);
    root.size = Vector2(ui.screenW / scale, ui.screenH / scale);
    return root;
}

} // namespace toy
