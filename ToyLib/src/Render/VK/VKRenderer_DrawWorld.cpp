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
// VKRenderer: World common/material binding (minimum)
// ------------------------------------------------------------
void VKRenderer::BindWorldCommon(VkCommandBuffer cmd,
                                const VKPipeline& p,
                                const RenderItem& it)
{
    // Push constants: world + viewProj
    PC_World pc{};
    pc.world    = it.world;
    pc.viewProj = it.viewProj;

    vkCmdPushConstants(cmd,
                       p.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       (uint32_t)sizeof(PC_World),
                       &pc);
}

void VKRenderer::BindWorldMaterial(VkCommandBuffer cmd,
                                  const VKPipeline& p,
                                  const RenderItem& it)
{
    // 今は “Diffuse 1枚” だけ（Sprite と同じ combined image sampler）
    // ※将来 Scene(ライト/影/フォグ) を set1 に追加する
    TextureHandle texH{};

    if (it.material.ptr)
    {
        // Material から DiffuseMap を取る実装に合わせてここを書き換え
        // 例：texH = it.material.ptr->GetDiffuseTextureHandle();
        texH = it.material.ptr->GetDiffuseTextureHandle(); // ★仮：あなたのAPIに合わせて調整
    }

    VkDescriptorSet set0 = GetOrCreateSpriteDescSet(texH); // ★World用を用意（Sprite流用でもOK）

    if (set0)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                p.pipelineLayout,
                                0, // set=0
                                1,
                                &set0,
                                0, nullptr);
    }
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
    auto& items = mRenderQueue.Items();
    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;
        const RenderItem& it = items[idx];

        // World pass only
        if (it.pass != RenderPass::World) continue;

        // Mesh / Skinned は pipeline を分ける方針を踏襲
        // it.type を見て分岐したい場合はここでフィルタしてもOK
        DrawWorldItem_VK(it);
    }
}

// ------------------------------------------------------------
// VKRenderer: DrawWorldPass
// ------------------------------------------------------------
void VKRenderer::DrawWorldPass()
{
    // World は BeginFrame() で render pass begin 済み（あなたの Core 実装に合わせる）
    // ここは「描画するだけ」にする。

    if (!EnsureWorldDescriptors())
         return;
    
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
