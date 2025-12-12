#pragma once

#include "Graphics/Effect/WireframeComponent.h"
#include "Engine/Render/Renderer.h"

namespace toy {

//======================================================================
// DebugWireframeComponent
//----------------------------------------------------------------------
// デバッグ用途専用のワイヤーフレーム描画コンポーネント。
//
// ・衝突判定用の AABB / OBB / Polygon などを可視化する目的で使用。
// ・通常の WireframeComponent を継承し、
//   デフォルトで描画順を後ろ（大きな DrawOrder）に設定している。
// ・VisualLayer も Effect3D をデフォルトとし、
//   ゲーム描画とは独立して重ねて表示できるようにしている。
//
// 主な用途：
// ・PhysWorld / ColliderComponent のデバッグ
// ・当たり判定や押し戻し処理の可視化
// ・開発時のみ有効にするデバッグ表示
//======================================================================
class DebugWireframeComponent : public WireframeComponent
{
public:
    //==============================================================
    // コンストラクタ
    //  owner     : このコンポーネントを所有する Actor
    //  drawOrder : 描画順（デバッグ表示なので後ろに描画される想定）
    //  layer     : 描画レイヤー（Effect3D がデフォルト）
    //==============================================================
    DebugWireframeComponent(
        class Actor* owner,
        int drawOrder = 1000,
        VisualLayer layer = VisualLayer::Effect3D
    );
    
    //==============================================================
    // Draw
    //  ワイヤーフレームの描画処理。
    //  Renderer を通してデバッグ用の描画設定で描画される。
    //==============================================================
    void Draw() override;
};

} // namespace toy
