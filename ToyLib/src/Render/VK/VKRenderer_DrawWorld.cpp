// Render/VK/VKRenderer_DrawWorld.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"

#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/IVertexArrayBackend.h"
#include "Asset/Material/Material.h"

#include <iostream>

namespace toy
{

static VKPipeline* AsVKPipeline(const PipelineHandle& h)
{
    return (h.IsValidVK()) ? reinterpret_cast<VKPipeline*>(h.ptrVKPipeline) : nullptr;
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
                            1,          // ★ set = 1
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
    TextureHandle texH{};

    if (it.material.ptr)
    {
        texH = it.material.ptr->GetDiffuseTextureHandle();
    }
    else
    {
        texH.ptr = nullptr; // -> dummy white
    }

    VkDescriptorSet set0 = GetOrCreateWorldTexDescSet(texH);
    if (set0 == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            0,          // ★ set = 0
                            1,
                            &set0,
                            0, nullptr);
}

void VKRenderer::DrawWorldItem_VK(const RenderItem& it)
{
    VKPipeline* pipe = AsVKPipeline(it.pipeline);
    if (!pipe || pipe->pipeline == VK_NULL_HANDLE) return;

    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    // pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

    // geometry backend
    if (!it.geometry.ptr) return;
    if (it.indexCount <= 0) return;

    auto* backend = it.geometry.ptr->GetBackend();
    if (!backend || !backend->IsVK()) return;

    VkBuffer vb = (VkBuffer)backend->GetVKVertexBuffer();
    VkBuffer ib = (VkBuffer)backend->GetVKIndexBuffer();
    VkIndexType indexType = (VkIndexType)backend->GetVKIndexType();
    uint32_t indexCount = (uint32_t)it.indexCount;

    if (vb == VK_NULL_HANDLE || ib == VK_NULL_HANDLE || indexCount == 0) return;

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
    vkCmdBindIndexBuffer(cmd, ib, 0, indexType);

    // per-frame UBO update（view/cam/fog etc）
    UpdateWorldCommonUBO(mImageIndex);
    UpdateDirLightUBO();
    UpdatePointLightUBO();

    // per-item UBO update（material params）
    UpdateMaterialParamsUBO(it);

    // bind (set1 + push, set0)
    BindWorldCommon(cmd, *pipe, it);
    BindWorldMaterial(cmd, *pipe, it);

    // draw
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    AddDrawCall();
}

void VKRenderer::DrawBucket_WorldVK(const std::vector<uint32_t>& bucket)
{
    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    if (!EnsureWorldDescriptors()) return;

    //============================================================
    // Dynamic viewport / scissor
    //  - あなたが「上下反転」と言っていたので flip を採用
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
