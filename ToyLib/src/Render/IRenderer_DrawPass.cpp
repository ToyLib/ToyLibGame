//==============================================================================
// IRenderer_DrawPass.cpp
//  - RenderQueue draw helpers
//  - GL state apply
//  - Shadow / World / UI / Post passes
//  - Draw(), BuildFrameQueues(), DrawToRenderTarget()
//==============================================================================

#include "Render/IRenderer.h"

// Engine / Render
#include "Render/LightingManager.h"
#include "Render/IRenderTarget.h"


// Geometry / Visual
#include "Asset/Geometry/VertexArray.h"
#include "Graphics/VisualComponent.h"

// Core / Physics
#include "Engine/Core/Actor.h"
#include "Physics/BoundingVolumeComponent.h"

// Utils
#include "Utils/FrustumUtil.h"



// Std
#include <algorithm>
#include <iostream>

namespace toy {

//==============================================================================
// Bucket draw helpers
//  - bucket は「mRenderQueue.Items() の index 配列」
//  - 各 Pass は bucket を走査して対応する RenderItem を描画する
//==============================================================================

void IRenderer::DrawBucket_World(const std::vector<uint32_t>& bucket)
{
    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue; // safety
        }

        const RenderItem& it = items[idx];

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem(it, RenderPass::World, -1);
    }
}

void IRenderer::DrawBucket_Shadow(const std::vector<uint32_t>& bucket, int cascadeIndex)
{
    if (bucket.empty())
    {
        return;
    }

    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue; // safety
        }

        const RenderItem& it = items[idx];

        // safety: UI が混入していた場合は Shadow から除外
        if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
        {
            continue;
        }

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem(it, RenderPass::Shadow, cascadeIndex);
    }
}


//==============================================================================
// Main Draw
//==============================================================================
void IRenderer::Draw()
{
    ResetDebugCounter();

    if (!BeginFrame())
    {
        // ★BeginFrame失敗時もキューは破棄する。
        //   ここで残すと、次にレンダラーが破棄される際に mSceneCaptureQueue
        //   (IRenderer基底クラスのメンバ)がVKRenderer::Shutdown()より後に
        //   破棄され、そのshared_ptr<IRenderTarget>が既に破棄済みの
        //   VkDeviceに対してvkDestroyFramebuffer等を呼んでクラッシュする。
        mSceneCaptureQueue.clear();
        return;
    }

    for (const auto& req : mSceneCaptureQueue)
    {
        DrawToRenderTarget(req);
    }
    mSceneCaptureQueue.clear();

    BuildFrameQueues();

    if (mEnableShadow)
    {
        DrawShadowPass();
    }
    RestoreAfterShadowPass();

    DrawSkyPass();
    DrawWorldPass();
    DrawOverlayScreenPass();

    if (mPost.type != PostEffectType::None)
    {
        DrawPostEffectPass();
    }

    DrawUIPass();
    DrawFadePass();
    EndFrame();
}

//==============================================================================
// BuildFrameQueues
//  - VisualComponent から RenderItem を回収し、mRenderQueue に集約
//  - 同時に bucket に分類し、DrawPass では bucket を走査して描画する
//==============================================================================

void IRenderer::BuildFrameQueues()
{
    mRenderQueue.Clear();
    mBuckets.Clear();

    // メインカメラ用 frustum（通常描画用）
    const Matrix4 vp = GetViewMatrix() * GetProjectionMatrix();
    const Frustum cameraFrustum = BuildFrustumFromMatrix(vp);

    // シャドウ用: 各カスケードのライトVPを今フレームぶん再計算し、frustumを作っておく
    // （シャドウキャスターのカリング判定に使う。DrawShadowPass 側はこの後の値をそのまま使う）
    //
    // SceneCapture（ミラー等のRTT）は DrawToRenderTarget() 内で mViewMatrix/mInvView を
    // 一時的にキャプチャ用カメラへ差し替えた上でこの BuildFrameQueues() を再度呼ぶ。
    // ここで再計算してしまうと、実際にレンダリング済みの（メインカメラ基準の）シャドウマップと
    // ライトVPがズレてキャプチャ内のシャドウ位置がおかしくなるため、キャプチャ中は再計算しない。
    if (!mIsDrawingCapture)
    {
        UpdateShadowLightMatrices();
    }

    Frustum cascadeFrustum[kShadowCascadeCount];
    for (int c = 0; c < kShadowCascadeCount; ++c)
    {
        cascadeFrustum[c] = BuildFrustumFromMatrix(GetLightSpaceMatrix(c));
    }

    for (auto* vc : mVisualComps)
    {
        if (!vc || !vc->IsVisible())
        {
            continue;
        }

        //====================================================
        // (B) Shadow caster
        //  - カスケードごとのライト視錐台とAABBを比較し、
        //    交差するカスケードのbucketにだけ積む（cull）
        //  - AABBが取れないオブジェクトは安全側に倒して全カスケードへ積む
        //  - payload も mRenderQueue に直接積まれるので消えない
        //====================================================
        if (vc->GetEnableShadow())
        {
            const uint32_t before = static_cast<uint32_t>(mRenderQueue.Items().size());

            vc->GatherShadowItems(mRenderQueue);

            const uint32_t after = static_cast<uint32_t>(mRenderQueue.Items().size());

            const BoundingVolumeComponent* shadowBV = nullptr;
            if (Actor* owner = vc->GetOwner())
            {
                shadowBV = owner->GetComponent<BoundingVolumeComponent>();
            }

            if (shadowBV)
            {
                const Cube shadowAABB = shadowBV->GetWorldAABB();

                for (int c = 0; c < kShadowCascadeCount; ++c)
                {
                    if (!FrustumIntersectsAABB(cascadeFrustum[c], shadowAABB))
                    {
                        continue;
                    }

                    for (uint32_t i = before; i < after; ++i)
                    {
                        mBuckets.shadowCaster[c].push_back(i);
                    }
                }
            }
            else
            {
                for (int c = 0; c < kShadowCascadeCount; ++c)
                {
                    for (uint32_t i = before; i < after; ++i)
                    {
                        mBuckets.shadowCaster[c].push_back(i);
                    }
                }
            }
        }

        //====================================================
        // (A) 通常描画だけ frustum cull
        //====================================================
        const VisualLayer layer = vc->GetLayer();
        const bool shouldCull =
            (layer == VisualLayer::Object3D) ||
            (layer == VisualLayer::Effect3D);

        if (shouldCull)
        {
            Actor* owner = vc->GetOwner();
            if (owner)
            {
                auto* bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    const Cube aabb = bv->GetWorldAABB();
                    if (!FrustumIntersectsAABB(cameraFrustum, aabb))
                    {
                        continue; // ★Shadowは上で積んでるので「通常描画」だけ止める
                    }
                }
            }
        }

        //====================================================
        // (C) 通常 items（World/UI/Overlay...）
        //  - payload も mRenderQueue に直接積まれるので消えない
        //====================================================
        const uint32_t before = static_cast<uint32_t>(mRenderQueue.Items().size());

        vc->GatherRenderItems(mRenderQueue);

        const uint32_t after = static_cast<uint32_t>(mRenderQueue.Items().size());
        const auto& items = mRenderQueue.Items();

        for (uint32_t i = before; i < after; ++i)
        {
            const RenderItem& it = items[i];

            // safety: RenderItems 側に Shadow が混ざってた場合
            //  - ここではAABBが手元にないため cull せず全カスケードへ積む
            if (it.pass == RenderPass::Shadow)
            {
                for (int c = 0; c < kShadowCascadeCount; ++c)
                {
                    mBuckets.shadowCaster[c].push_back(i);
                }
                continue;
            }

            // UI
            if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
            {
                mBuckets.ui.push_back(i);
                continue;
            }

            switch (it.layer)
            {
                case VisualLayer::Sky:
                    mBuckets.sky.push_back(i);
                    break;

                case VisualLayer::OverlayScreen:
                    mBuckets.overlayScreen.push_back(i);
                    break;

                case VisualLayer::Object3D:
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(i);
                    else
                        mBuckets.worldTransparent.push_back(i);
                    break;

                case VisualLayer::Effect3D:
                {
                    const bool isPre =
                        (it.type == RenderItemType::Sprite) ||
                        (it.type == RenderItemType::Debug);

                    if (isPre) mBuckets.effectPre.push_back(i);
                    else       mBuckets.effectOverlay.push_back(i);
                    break;
                }

                default:
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(i);
                    else
                        mBuckets.worldTransparent.push_back(i);
                    break;
            }
        }
    }

    // Sort
    SortBucket(mBuckets.sky);
    SortBucket(mBuckets.worldOpaque);
    SortBucket(mBuckets.effectPre);
    SortBucket(mBuckets.worldTransparent);
    SortBucket(mBuckets.effectOverlay);
    SortBucket(mBuckets.overlayScreen);
    SortBucket(mBuckets.ui);

    for (int c = 0; c < kShadowCascadeCount; ++c)
    {
        SortBucket_Shadow(mBuckets.shadowCaster[c]);
    }
}

//==============================================================================
// Bucket sort
//==============================================================================

void IRenderer::SortBucket(std::vector<uint32_t>& bucket)
{
    if (bucket.size() <= 1)
    {
        return;
    }

    auto& items = mRenderQueue.Items();

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&items](uint32_t ia, uint32_t ib)
        {
            // safety: 範囲外は末尾へ
            const bool aValid = (ia < items.size());
            const bool bValid = (ib < items.size());
            if (aValid != bValid) return aValid;
            if (!aValid && !bValid) return false;

            const RenderItem& a = items[ia];
            const RenderItem& b = items[ib];

            // 1) RenderPass（enum順）
            if (a.pass != b.pass) return a.pass < b.pass;

            // 2) VisualLayer（enum順）
            if (a.layer != b.layer) return a.layer < b.layer;

            // 3) BlendMode：Opaque を先に（Z 確定）
            const bool aOpaque = (a.blend == BlendMode::Opaque);
            const bool bOpaque = (b.blend == BlendMode::Opaque);
            if (aOpaque != bOpaque) return aOpaque;

            // 4) DrawOrder
            if (a.drawOrder != b.drawOrder) return a.drawOrder < b.drawOrder;

            // 5) 完全一致：stable_sort で投入順維持
            return false;
        }
    );
}

void IRenderer::SortBucket_Shadow(std::vector<uint32_t>& bucket)
{
    auto& items = mRenderQueue.Items();

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&](uint32_t a, uint32_t b)
        {
            const RenderItem& A = items[a];
            const RenderItem& B = items[b];

            // 0) safety: Shadow 以外が混入していたら後ろへ
            const bool aShadow = (A.pass == RenderPass::Shadow);
            const bool bShadow = (B.pass == RenderPass::Shadow);
            if (aShadow != bShadow)
            {
                return aShadow;
            }
            
            // 1) pipeline/shader でまとめる（SetActive / bind 削減）
            //    バックエンド非依存: GetPipelineSortKey() を各 Renderer がオーバーライド
            //      GL → ptrGLShader アドレス（SetActive 削減）
            //      VK → ptrVKPipeline アドレス（bind 削減）
            const uint64_t aKey = GetPipelineSortKey(A.pipeline);
            const uint64_t bKey = GetPipelineSortKey(B.pipeline);
            if (aKey != bKey)
            {
                return aKey < bKey;
            }
            
            // 2) geometry でまとめる（VAO bind 削減）
            if (A.type != RenderItemType::Particle)
            {
                if (A.geometry.ptr != B.geometry.ptr)
                {
                    return A.geometry.ptr < B.geometry.ptr;
                }
            }

            // 3) Skinned をまとめる（任意）
            const bool aSkinned = (A.type == RenderItemType::SkinnedMesh);
            const bool bSkinned = (B.type == RenderItemType::SkinnedMesh);
            if (aSkinned != bSkinned)
            {
                return aSkinned;
            }
            
            return false; // stable_sort に任せる（投入順維持）
        }
    );
}


} // namespace toy
