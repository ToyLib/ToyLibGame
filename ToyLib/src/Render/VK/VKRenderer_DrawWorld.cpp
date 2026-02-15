// Render/VK/VKRenderer_DrawWorld.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"

#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/IVertexArrayBackend.h"
#include "Asset/Material/Material.h"

#include <iostream>
#include <string>

namespace toy
{

static VKPipeline* AsVKPipeline(const PipelineHandle& h)
{
    return (h.IsValidVK()) ? reinterpret_cast<VKPipeline*>(h.ptrVKPipeline) : nullptr;
}

static const char* ToStr(CullMode c)
{
    switch (c)
    {
    case CullMode::None:  return "CullNone";
    case CullMode::Front: return "CullFront";
    case CullMode::Back:  return "CullBack";
    }
    return "CullBack";
}

static const char* ToStr(FrontFace f)
{
    switch (f)
    {
    case FrontFace::CCW: return "CCW";
    case FrontFace::CW:  return "CW";
    }
    return "CCW";
}

//------------------------------------------------------------
// Pipeline resolve
//  - it.pipeline は「シェーダ系」を指す（ベース）
//  - cull/frontFace が要求されている場合は、派生パイプライン名を探す
//    例: "Mesh" -> "Mesh_CullFront_CW"
//------------------------------------------------------------
VKPipeline* VKRenderer::ResolveWorldPipelineForItem(const RenderItem& it)
{
    VKPipeline* base = AsVKPipeline(it.pipeline);
    if (!base) return nullptr;

    // 既定ならそのまま
    if (it.cull == CullMode::Back && it.frontFace == FrontFace::CCW)
    {
        return base;
    }

    // debugName をキーに派生を探す（CreateMeshPipeline 群で作る想定）
    // NOTE: mPipelines は std::map<std::string, std::unique_ptr<VKPipeline>>
    std::string key = base->debugName;
    key += "_";
    key += ToStr(it.cull);
    key += "_";
    key += ToStr(it.frontFace);

    auto found = mPipelines.find(key);
    if (found != mPipelines.end() && found->second)
    {
        return found->second.get();
    }

    // 派生が無いならフォールバック（とりあえず描けること優先）
    return base;
}

//------------------------------------------------------------
// set1 + push(world)
//------------------------------------------------------------
void VKRenderer::BindWorldCommon(VkCommandBuffer cmd,
                                const VKPipeline& p,
                                const RenderItem& it)
{
    // push constant: world only（shader: pc.uWorldTransform）
    vkCmdPushConstants(cmd,
                       p.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       (uint32_t)sizeof(Matrix4),
                       &it.world);

    // set1: scene/common (swapchain index)
    if (mWorldDescSets.empty()) return;

    VkDescriptorSet set1 = mWorldDescSets[mImageIndex];

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            1,          // set = 1
                            1,
                            &set1,
                            0, nullptr);
}

//------------------------------------------------------------
// set0: diffuse texture
//------------------------------------------------------------
void VKRenderer::BindWorldMaterial(VkCommandBuffer cmd,
                                  const VKPipeline& p,
                                  const RenderItem& it)
{
    // 「テクスチャ無し」でも sampler2D は必ず有効なものを bind しておく
    //  -> GetOrCreateWorldTexDescSet(invalid) が dummy white を返す想定
    TextureHandle texH{};

    if (it.material.ptr)
    {
        texH = it.material.ptr->GetDiffuseTextureHandle();
    }
    else
    {
        texH = TextureHandle{}; // invalid -> dummy white
    }

    VkDescriptorSet set0 = GetOrCreateWorldTexDescSet(texH);
    if (set0 == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            0,          // set = 0
                            1,
                            &set0,
                            0, nullptr);
}

void VKRenderer::DrawWorldItem_VK(const RenderItem& it)
{
    // World 描画以外はここでは無視（呼び元でも弾いてるが保険）
    if (it.pass != RenderPass::World) return;

    // pipeline resolve (cull/frontFace 反映)
    VKPipeline* pipe = ResolveWorldPipelineForItem(it);
    if (!pipe || pipe->pipeline == VK_NULL_HANDLE) return;

    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    // pipeline bind
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

    // geometry backend
    if (!it.geometry.ptr) return;

    auto* backend = it.geometry.ptr->GetBackend();
    if (!backend || !backend->IsVK()) return;

    VkBuffer vb = (VkBuffer)backend->GetVKVertexBuffer();
    VkBuffer ib = (VkBuffer)backend->GetVKIndexBuffer();

    if (vb == VK_NULL_HANDLE) return;

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);

    // per-frame UBO update（view/cam/fog etc）
    // NOTE: ここは本当は DrawBucket 側で 1回に寄せたいが、
    //       今は「確実に更新される」ことを優先して現状維持。
    UpdateWorldCommonUBO(mImageIndex);
    UpdateDirLightUBO();
    UpdatePointLightUBO();

    // per-item UBO update（material params）
    UpdateMaterialParamsUBO(it);

    // bind (set1 + push, set0)
    BindWorldCommon(cmd, *pipe, it);
    BindWorldMaterial(cmd, *pipe, it);

    // draw (indexed or non-indexed)
    if (it.indexCount > 0 && ib != VK_NULL_HANDLE)
    {
        VkIndexType indexType = (VkIndexType)backend->GetVKIndexType();
        uint32_t indexCount   = (uint32_t)it.indexCount;

        vkCmdBindIndexBuffer(cmd, ib, 0, indexType);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }
    else if (it.vertexCount > 0)
    {
        vkCmdDraw(cmd, (uint32_t)it.vertexCount, 1, 0, 0);
    }
    else
    {
        return;
    }

    AddDrawCall();
}

void VKRenderer::DrawBucket_WorldVK(const std::vector<uint32_t>& bucket)
{
    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    if (!EnsureWorldDescriptors()) return;

    //============================================================
    // Dynamic viewport / scissor
    //  - 今は flip 採用（VKの座標系/補正方針に合わせる）
    //============================================================
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = (float)mSwapchainExtent.height;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = -(float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = mSwapchainExtent;

    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    auto& items = mRenderQueue.Items();
    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;
        const RenderItem& it = items[idx];
        if (it.pass != RenderPass::World) continue;

        DrawWorldItem_VK(it);
    }
}

void VKRenderer::DrawWorldPass()
{
    DrawBucket_WorldVK(mBuckets.worldOpaque);
    DrawBucket_WorldVK(mBuckets.effectPre);
    DrawBucket_WorldVK(mBuckets.worldTransparent);
    DrawBucket_WorldVK(mBuckets.effectOverlay);
}

} // namespace toy
