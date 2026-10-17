#pragma once

#include "Utils/MathUtil.h"

#include <optional>

namespace toy {

class IRenderer;

//==============================================================
// UI レイアウト
//------------------------------------------------------------------------------
// 座標系：UI 論理座標（仮想解像度基準、左上原点、+Y が下）
//         SpriteComponent / TextSpriteComponent と同じ座標系
//
// 位置の決め方
//   1. 親矩形上のアンカー点を求める（anchor）
//   2. 自分の矩形上のピボット点（pivot、未指定なら anchor と同じ）を
//      アンカー点 + offset に合わせる
//
//   例）画面下中央、下端から 20px 上に 680x180 の箱
//       layout.anchor = UIAnchor::Bottom;
//       layout.offset = Vector2(0.0f, -20.0f);
//       layout.size   = Vector2(680.0f, 180.0f);
//
// Stretch：指定した軸は親いっぱいに広げ、両端に stretchMargin の余白をとる
//          （その軸の size / anchor / pivot は無視。offset は加算される）
//==============================================================
enum class UIAnchor
{
    TopLeft,    Top,    TopRight,
    Left,       Center, Right,
    BottomLeft, Bottom, BottomRight,
};

enum class UIStretch
{
    None,
    Horizontal,
    Vertical,
    Both,
};

struct UIRect
{
    Vector2 pos  { Vector2::Zero };  // 左上
    Vector2 size { Vector2::Zero };

    float Left()   const { return pos.x; }
    float Top()    const { return pos.y; }
    float Right()  const { return pos.x + size.x; }
    float Bottom() const { return pos.y + size.y; }

    // 矩形上のアンカー点
    Vector2 GetPoint(UIAnchor anchor) const;

    // 内側に余白をとった矩形
    UIRect Inset(const Vector2& padding) const;
};

struct UILayout
{
    UIAnchor anchor { UIAnchor::TopLeft };
    std::optional<UIAnchor> pivot;          // 未指定なら anchor と同じ

    Vector2 offset { Vector2::Zero };       // +X 右 / +Y 下
    Vector2 size   { Vector2::Zero };

    UIStretch stretch       { UIStretch::None };
    Vector2   stretchMargin { Vector2::Zero };

    // 親矩形から自分の矩形を求める
    UIRect Resolve(const UIRect& parent) const;
};

// UI のルート矩形（実画面を UI 論理座標に換算したもの）
// ・アスペクト比が仮想解像度と同じなら (0, 0, virtualW, virtualH)
// ・違う場合も、右端 / 下端アンカーが実画面の端に付くように広がる
UIRect GetUIRootRect(const IRenderer* renderer);

} // namespace toy
