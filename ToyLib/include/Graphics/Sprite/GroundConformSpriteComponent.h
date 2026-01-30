#pragma once
#include "Graphics/Sprite/FootSpriteComponent.h"

#include <memory>

namespace toy {

//------------------------------------------------------------------------------
// GroundConformSpriteComponent
//  - FootSprite を “地面の起伏に沿う” メッシュに差し替えて描画する派生
//  - ジャンプ中でも地面に貼り付く（OwnerのYは無視して地面Yを取る）
//  - Terrain + Collider床 両対応（PhysWorld::GetNearestGroundHitAtXZ を使用）
//
// 注意：VertexArray は動的更新が無い前提なので、必要時に作り直す。
//------------------------------------------------------------------------------
class GroundConformSpriteComponent : public FootSpriteComponent
{
public:
    GroundConformSpriteComponent(class Actor* owner,
                                 int drawOrder = 10,
                                 VisualLayer layer = VisualLayer::Effect3D);
    ~GroundConformSpriteComponent() override = default;

    // 分割数（2以上推奨）
    void SetGridDiv(int div) { mGridDiv = (div < 1) ? 1 : div; }

    // 地面から少し浮かせる（Z-fighting / めり込み対策）
    void SetGroundLift(float v) { mGroundLift = v; }

    // 中心の地面Yとの差をクランプ（ガタつき抑制）
    void SetMaxDeltaFromCenter(float v) { mMaxDeltaFromCenter = v; }

protected:
    void PreDraw() override;
    void Draw() override;                      // 旧パス互換：基本は呼ばれないが残す
    Matrix4 BuildWorldMatrix() const override; // GroundConformはIdentity

    // 新パス
    void GatherRenderItems(class RenderQueueLike& queue) override;

private:
    void RebuildGridIfNeeded();
    bool SampleGroundAtXZ(const Vector3& worldXZ, struct GroundHit& outHit) const;

private:
    // グリッド分割（1なら2x2頂点＝ただのQuad）
    int   mGridDiv { 4 };

    float mGroundLift { 0.02f };
    float mMaxDeltaFromCenter { 0.6f };

    // 直近の中心地面
    bool  mHasBase { false };
    float mBaseY   { 0.0f };

    // 再構築トリガ用キャッシュ
    Vector3 mPrevOwnerPos { Vector3::Zero };
    float   mPrevWidth { 0.0f };
    float   mPrevDepth { 0.0f };
    int     mPrevDiv   { -1 };

    // 地面沿いメッシュ（VAO）
    std::shared_ptr<class VertexArray> mGridVAO;
};

} // namespace toy
