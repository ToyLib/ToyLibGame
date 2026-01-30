#pragma once

#include "Graphics/VisualComponent.h"

namespace toy {

//======================================================================
// BillboardComponent
//
// ・3D空間に配置される看板型スプライト
// ・常にカメラ方向を向く（Y軸のみ回転）
// ・木、簡易LOD、エフェクトなど
//
// ※新レンダーパス用：Draw() は基本使わず GatherRenderItems に積む
//======================================================================
class BillboardComponent : public VisualComponent
{
public:
    BillboardComponent(class Actor* a,
                       int drawOrder = 200,
                       VisualLayer layer = VisualLayer::Object3D);

    // 旧パス互換（混在期間用）: 基本は何もしない
    void Draw() override {}

    // 新パス
    void GatherRenderItems(RenderQueueLike& out) override;

    void SetScale(float s) { mScale = s; }
    float GetScale() const { return mScale; }

    // Y軸だけ向ける（木など）: true 推奨
    void SetYawOnly(bool v) { mYawOnly = v; }
    bool GetYawOnly() const { return mYawOnly; }

private:
    float mScale { 1.0f };
    bool  mYawOnly { true };
};

} // namespace toy
