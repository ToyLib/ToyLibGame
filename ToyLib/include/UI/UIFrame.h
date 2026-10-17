#pragma once

#include "UI/UILayout.h"
#include "Utils/MathUtil.h"

namespace toy {

class Actor;
class NineSliceSpriteComponent;

//==============================================================
// UIFrameStyle
//  - ウィンドウの背景と枠線の見た目
//  - テクスチャはコードで生成する（画像アセット不要）
//    radius <= 0 なら角丸なしの四角（従来の見た目）
//==============================================================
struct UIFrameStyle
{
    // 背景
    Vector3 bgColor { Vector3(0.05f, 0.05f, 0.08f) };
    float   bgAlpha { 0.6f };

    // 角丸の半径（px）
    int radius { 8 };

    // 枠線（lineWidth <= 0 で非表示）
    int     lineWidth { 2 };
    Vector3 lineColor { Vector3(0.85f, 0.85f, 0.9f) };
    float   lineAlpha { 0.9f };
};

//==============================================================
// UIFrame
//  - 背景(9-slice) + 枠線(9-slice) をまとめて扱うヘルパー
//  - Component を owner の Actor に作るだけで、寿命は Actor に従う
//
// 使い方（UIScreen 派生）
//   mFrame.Create(this, 900, 901);
//   mFrame.ApplyStyle(desc.frame);
//   // OnLayoutChanged で
//   mFrame.SetRect(GetLocalRect());
//==============================================================
class UIFrame
{
public:
    void Create(Actor* owner, int bgDrawOrder, int lineDrawOrder);

    void ApplyStyle(const UIFrameStyle& style);
    const UIFrameStyle& GetStyle() const { return mStyle; }

    // 描画範囲（owner からのローカル座標）
    void SetRect(const UIRect& localRect);

    void SetVisible(bool visible);

private:
    Actor* mOwner { nullptr };
    NineSliceSpriteComponent* mBg   { nullptr };
    NineSliceSpriteComponent* mLine { nullptr };

    UIFrameStyle mStyle;
    bool mVisible { true };
};

} // namespace toy
