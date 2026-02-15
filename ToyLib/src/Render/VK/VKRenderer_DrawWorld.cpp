// Render/VK/VKRenderer_DrawWorld.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/IVertexArrayBackend.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"

#include <iostream>

namespace toy {

// ------------------------------------------------------------
// PushConstants (minimum)
//  - ToyLib は row-vector (v * M) 前提なので、GL側と同じ行列をそのまま渡す。
// ------------------------------------------------------------
struct PC_World
{
    Matrix4 world;
    Matrix4 viewProj;
};

// ------------------------------------------------------------
// Helpers (internal)
// ------------------------------------------------------------
namespace {

inline VKPipeline* AsVKPipeline(const PipelineHandle& h)
{
    return (h.IsValidVK()) ? reinterpret_cast<VKPipeline*>(h.ptrVKPipeline) : nullptr;
}

inline VkCommandBuffer GetCmd(toy::VKRenderer& r)
{
    return r.GetActiveCommandBuffer(); // ★ユーザー要望：存在する前提
}

inline bool GetVKGeometry(const RenderItem& it,
                          VkBuffer& outVB,
                          VkBuffer& outIB,
                          VkIndexType& outIndexType,
                          uint32_t& outIndexCount)
{
    outVB = VK_NULL_HANDLE;
    outIB = VK_NULL_HANDLE;
    outIndexType = VK_INDEX_TYPE_UINT32;
    outIndexCount = 0;

    if (!it.geometry.ptr) return false;
    if (it.indexCount <= 0) return false;

    // VertexArrayBackend から VK バッファ取得
    // （あなたの IVertexArrayBackend に GetVKVertexBuffer 等がある前提）
    auto* backend = it.geometry.ptr->GetBackend(); // ★もし無いなら Getter を追加
    if (!backend || !backend->IsVK()) return false;

    outVB = (VkBuffer)backend->GetVKVertexBuffer();
    outIB = (VkBuffer)backend->GetVKIndexBuffer();
    outIndexType  = (VkIndexType)backend->GetVKIndexType();
    outIndexCount = (uint32_t)it.indexCount;

    if (!outVB || !outIB) return false;
    return true;
}

} // unnamed

// ------------------------------------------------------------
// VKRenderer: World common/material binding (fixed)
// ------------------------------------------------------------
void VKRenderer::BindWorldCommon(VkCommandBuffer cmd,
                                 const VKPipeline& p,
                                 const RenderItem& it)
{
    // 1) Push constants: world only（viewProj は UBO 側）
    Matrix4 world = it.world;

    vkCmdPushConstants(cmd,
                       p.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       (uint32_t)sizeof(Matrix4),
                       &world);

    // 2) Bind set1: Scene/Common UBOs（binding 0..3）
    //    EnsureWorldDescriptors() が作ったやつを使う
    if (mWorldDescSets.empty()) return;

    const uint32_t img = mImageIndex; // AcquireNextImage の index
    if (img >= (uint32_t)mWorldDescSets.size()) return;

    VkDescriptorSet set1 = mWorldDescSets[img];
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            /*firstSet*/ 1,
                            /*descriptorSetCount*/ 1,
                            &set1,
                            0, nullptr);
}

void VKRenderer::BindWorldMaterial(VkCommandBuffer cmd,
                                   const VKPipeline& p,
                                   const RenderItem& it)
{
    // set0: Diffuse sampler
    TextureHandle texH{};

    if (it.material.ptr)
    {
        texH = it.material.ptr->GetDiffuseTextureHandle();
    }
    // ない場合は「白1x1」などのダミーにするのが安全
    // texH が無効なら GetOrCreateSpriteDescSet 側でダミーを返す設計でもOK

    VkDescriptorSet set0 = GetOrCreateSpriteDescSet(texH);
    if (set0 == VK_NULL_HANDLE) return;

    // ★重要：set0 は firstSet=0
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            /*firstSet*/ 0,
                            /*descriptorSetCount*/ 1,
                            &set0,
                            0, nullptr);
}

// ------------------------------------------------------------
// VKRenderer: Draw one world item (Mesh/Skinned)
// ------------------------------------------------------------
void VKRenderer::DrawWorldItem_VK(const RenderItem& it)
{

    VKPipeline* pipe = AsVKPipeline(it.pipeline);
    if (!pipe || !pipe->pipeline || !pipe->pipelineLayout)
    {
        return;
    }
   
    
    VkCommandBuffer cmd = GetCmd(*this);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

    // Bind geometry
    VkBuffer vb = VK_NULL_HANDLE;
    VkBuffer ib = VK_NULL_HANDLE;
    VkIndexType indexType{};
    uint32_t indexCount = 0;

    if (!GetVKGeometry(it, vb, ib, indexType, indexCount))
    {
        return;
    }

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
    vkCmdBindIndexBuffer(cmd, ib, 0, indexType);

    // Common + Material
    BindWorldCommon(cmd, *pipe, it);
    BindWorldMaterial(cmd, *pipe, it);

    

    // Draw
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    AddDrawCall();
}

// ------------------------------------------------------------
// Bucket draw helpers
// ------------------------------------------------------------
void VKRenderer::DrawBucket_WorldVK(const std::vector<uint32_t>& bucket)
{
    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    // set1(=World UBO群) を確実に作る
    if (!EnsureWorldDescriptors())
    {
        return;
    }

    // フレーム共通（最低限：WorldCommonは毎フレーム）
    UpdateWorldCommonUBO(mImageIndex);
    UpdateDirLightUBO();
    UpdatePointLightUBO();

    // Mesh pipeline が dynamic viewport/scissor を使ってるなら必須
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = (float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = mSwapchainExtent;

    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // Draw
    auto& items = mRenderQueue.Items();
    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;
        const RenderItem& it = items[idx];

        if (it.pass != RenderPass::World) continue;

        DrawWorldItem_VK(it); // ←この中で set0+set1 bind するのが理想
    }
}

// ------------------------------------------------------------
// VKRenderer: DrawWorldPass
// ------------------------------------------------------------
void VKRenderer::DrawWorldPass()
{
    // World は BeginFrame() で render pass begin 済み（あなたの Core 実装に合わせる）
    // ここは「描画するだけ」にする。

    
    // 1) Opaque
    DrawBucket_WorldVK(mBuckets.worldOpaque);

    // 2) Effect Pre
    DrawBucket_WorldVK(mBuckets.effectPre);

    // 3) Transparent
    DrawBucket_WorldVK(mBuckets.worldTransparent);

    // 4) Effect Overlay
    DrawBucket_WorldVK(mBuckets.effectOverlay);
}

} // namespace toy
