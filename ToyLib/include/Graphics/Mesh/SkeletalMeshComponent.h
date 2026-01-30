#pragma once

#include "Graphics/Mesh/MeshComponent.h"
#include <memory>
#include <vector>

namespace toy {

class AnimationPlayer;
class Mesh;
class RenderQueueLike;

constexpr size_t MAX_SKELETON_BONES = 96;

//------------------------------------------------------------
// SkeletalMeshComponent
//  - ボーンアニメーション付きメッシュの描画コンポーネント
//  - 新描画パス：GatherRenderItems / GatherShadowItems でRenderItemを積む
//------------------------------------------------------------
class SkeletalMeshComponent : public MeshComponent
{
public:
    SkeletalMeshComponent(class Actor* a,
                          int drawOrder = 100,
                          VisualLayer layer = VisualLayer::Effect3D);

    // 旧描画パス廃止（過渡期呼び出し対策で空実装）
    void Draw() override;
    void DrawShadow(int cascadeIndex) override;

    // 新描画パス
    void GatherRenderItems(RenderQueueLike& out) override;
    void GatherShadowItems(RenderQueueLike& out) override;

    void Update(float deltaTime) override;

    void SetAnimID(unsigned int animID, bool mode) override;
    void SetMesh(std::shared_ptr<class Mesh> m) override;

    AnimationPlayer* GetAnimPlayer() { return mAnimPlayer.get(); }

private:
    std::unique_ptr<AnimationPlayer> mAnimPlayer;
};

} // namespace toy
