#pragma once

#include "Graphics/Effect/WireframeComponent.h"

namespace toy {

//======================================================================
// DebugWireframeComponent
//----------------------------------------------------------------------
// ・デバッグ用途専用のワイヤーフレーム描画コンポーネント
// ・通常の WireframeComponent を継承し、デフォルトの drawOrder/layer を変える
// ・さらに「表示フラグ(GetVisibleDebugWire) がONの時だけ描画する」
//   という旧挙動を、新パスでは GatherRenderItems() 側で再現する
//======================================================================
class DebugWireframeComponent : public WireframeComponent
{
public:
    DebugWireframeComponent(
        class Actor* owner,
        int drawOrder = 1000,
        VisualLayer layer = VisualLayer::Effect3D
    );

    //==============================================================
    // GatherRenderItems（新パス）
    // 旧Draw()のガード（GetVisibleDebugWire）をここへ移植し、
    // OKなら基底 WireframeComponent の GatherRenderItems に委譲する
    //==============================================================
    void GatherRenderItems(RenderQueueLike& queue) override;
};

} // namespace toy
