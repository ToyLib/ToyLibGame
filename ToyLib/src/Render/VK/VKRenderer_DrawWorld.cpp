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
    case CullMode::None:  return "CullNone";   // ★ここを CullNone に戻す
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

static const char* GetPipelineBaseName(const RenderItem& it)
{
    // ★ここは “it.pipeline の debugName” に依存しないのが安全
    // まずは Mesh / SkinnedMesh だけ対応（増やすならここに足す）
    switch (it.type)
    {
    case RenderItemType::Mesh:       return "Mesh";
    case RenderItemType::SkinnedMesh:return "SkinnedMesh";
    default:                         return nullptr;
    }
}

//------------------------------------------------------------
// Pipeline resolve
//  - Mesh/SkinnedMesh の cull/frontFace を派生パイプラインで吸収
//  - 例: "Mesh" -> "Mesh_CullFront_CCW"
//------------------------------------------------------------
VKPipeline* VKRenderer::ResolveWorldPipelineForItem(const RenderItem& it)
{
    // Mesh 系だけ派生解決（それ以外は it.pipeline をそのまま使う）
    const char* baseName = GetPipelineBaseName(it);
    if (!baseName)
    {
        return AsVKPipeline(it.pipeline);
    }

    // 既定（Back + CCW）なら “Mesh” をそのまま引く方針にする
    // ※ CreateMeshPipeline 側で "Mesh" を作ってないなら、
    //    ここは "Mesh_CullBack_CCW" を既定にする（下で対応済み）
    std::string key;
    if (it.cull == CullMode::Back && it.frontFace == FrontFace::CCW)
    {
        key = baseName;
        auto f0 = mPipelines.find(key);
        if (f0 != mPipelines.end() && f0->second) return f0->second.get();

        // "Mesh" が無い運用ならこちらが既定
        key = std::string(baseName) + "_CullBack_CCW";
        auto f1 = mPipelines.find(key);
        if (f1 != mPipelines.end() && f1->second) return f1->second.get();

        // 最後の手段：it.pipeline
        return AsVKPipeline(it.pipeline);
    }

    // 派生キー
    key  = baseName;
    key += "_";
    key += ToStr(it.cull);
    key += "_";
    key += ToStr(it.frontFace);

    auto found = mPipelines.find(key);
    if (found != mPipelines.end() && found->second)
    {
        return found->second.get();
    }

    // 派生が無いなら、既定へフォールバック
    key = std::string(baseName) + "_CullBack_CCW";
    auto fb = mPipelines.find(key);
    if (fb != mPipelines.end() && fb->second) return fb->second.get();

    // 最後：it.pipeline
    return AsVKPipeline(it.pipeline);
}

//------------------------------------------------------------
// set1 + push(world)
//------------------------------------------------------------
void VKRenderer::BindWorldCommon(VkCommandBuffer cmd,
                                 const VKPipeline& p,
                                 const RenderItem& it)
{
    if (cmd == VK_NULL_HANDLE) return;
    if (p.pipelineLayout == VK_NULL_HANDLE) return;

    // push constants（既存どおり）
    PushConstants_Mesh pc{};
    pc.pcWorld = it.world;

    Vector3 diffuse(0.8f, 0.8f, 0.8f);
    float   specPower = 32.0f;
    int     useTex = 0;
    int     overrideCol = (it.overrideColor ? 1 : 0);

    if (it.material.ptr)
    {
        diffuse   = it.material.ptr->GetDiffuseColor();
        specPower = it.material.ptr->GetSpecPower();

        const bool wantUseTex = it.material.ptr->WantsUseTexture();
        const bool hasMap     = it.material.ptr->HasDiffuseMap();
        useTex = (wantUseTex && hasMap) ? 1 : 0;
    }

    Vector3 ucol(0.0f, 0.0f, 0.0f);
    if (overrideCol != 0)
    {
        ucol = it.overrideColorValue;
        useTex = 0;
    }

    pc.pcDiffuse[0] = diffuse.x;
    pc.pcDiffuse[1] = diffuse.y;
    pc.pcDiffuse[2] = diffuse.z;
    pc.pcDiffuse[3] = 1.0f;

    pc.pcUniform[0] = ucol.x;
    pc.pcUniform[1] = ucol.y;
    pc.pcUniform[2] = ucol.z;
    pc.pcUniform[3] = 1.0f;

    pc.pcFlagsSpec[0] = (float)useTex;
    pc.pcFlagsSpec[1] = (float)overrideCol;
    pc.pcFlagsSpec[2] = specPower;
    pc.pcFlagsSpec[3] = 0.0f;

    vkCmdPushConstants(
        cmd,
        p.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        (uint32_t)sizeof(PushConstants_Mesh),
        &pc
    );

    //========================================================
    // set=1 を type で分岐
    //========================================================
    VkDescriptorSet set1 = VK_NULL_HANDLE;

    if (it.type == RenderItemType::SkinnedMesh)
    {
        // ★Skinned は専用 set（binding=1 を含む）
        if (mSkinnedFrames.empty()) return;
        if (mImageIndex >= (uint32_t)mSkinnedFrames.size()) return;

        set1 = mSkinnedFrames[mImageIndex].descSet2_Bone;
    }
    else
    {
        // Mesh は従来どおり
        if (mWorldFrames.empty()) return;
        if (mImageIndex >= (uint32_t)mWorldFrames.size()) return;

        set1 = mWorldFrames[mImageIndex].descSet1_Common;
    }

    if (set1 == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        p.pipelineLayout,
        1,
        1,
        &set1,
        0, nullptr
    );
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
        texH = TextureHandle{}; // invalid -> dummy white
    }

    VkDescriptorSet set0 = GetOrCreateWorldTexDescSet(texH);
    if (set0 == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.pipelineLayout,
                            0,
                            1,
                            &set0,
                            0, nullptr);
}

//------------------------------------------------------------
// set=2 : BonePalette (Skinned only)
//------------------------------------------------------------
void VKRenderer::BindSkinnedBones(VkCommandBuffer cmd,
                                  const VKPipeline& p)
{
    if (cmd == VK_NULL_HANDLE) return;
    if (p.pipelineLayout == VK_NULL_HANDLE) return;

    // per-swapchain-image
    if (mSkinnedFrames.empty()) return;
    if (mImageIndex >= (uint32_t)mSkinnedFrames.size()) return;

    const VkDescriptorSet set2 = mSkinnedFrames[mImageIndex].descSet2_Bone; // ★名前はあなたの struct に合わせて
    if (set2 == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        p.pipelineLayout,
        2,          // ★set=2
        1,
        &set2,
        0, nullptr
    );
}

void VKRenderer::DrawWorldItem_VK(const RenderItem& it)
{
    if (it.pass != RenderPass::World) return;

    VKPipeline* pipe = ResolveWorldPipelineForItem(it);
    if (!pipe || pipe->pipeline == VK_NULL_HANDLE) return;

    VkCommandBuffer cmd = GetActiveCommandBuffer();
    if (cmd == VK_NULL_HANDLE) return;

    // pipeline bind
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

    // geometry
    if (!it.geometry.ptr) return;
    auto* backend = it.geometry.ptr->GetBackend();
    if (!backend || !backend->IsVK()) return;

    VkBuffer vb = (VkBuffer)backend->GetVKVertexBuffer();
    VkBuffer ib = (VkBuffer)backend->GetVKIndexBuffer();
    if (vb == VK_NULL_HANDLE) return;

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);

    // ------------------------------------------------------
    // set=1 + push(world/material flags)  &  set=0(texture)
    // ------------------------------------------------------
    BindWorldCommon(cmd, *pipe, it);
    BindWorldMaterial(cmd, *pipe, it);

    // ------------------------------------------------------
    // ★SkinnedMesh だけ set=2(BonePalette) を追加 bind
    // ------------------------------------------------------
    if (it.type == RenderItemType::SkinnedMesh)
    {
        BindSkinnedBones(cmd, *pipe);
    }

    // draw
    if (it.indexCount > 0 && ib != VK_NULL_HANDLE)
    {
        VkIndexType indexType = (VkIndexType)backend->GetVKIndexType();
        vkCmdBindIndexBuffer(cmd, ib, 0, indexType);
        vkCmdDrawIndexed(cmd, (uint32_t)it.indexCount, 1, 0, 0, 0);
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

    // set=1 UBO & descriptor set を保証
    if (!EnsureWorldDescriptors()) return;

    // ★Skinned がこの bucket に居るなら set=2 を準備
    {
        bool hasSkinned = false;
        auto& items = mRenderQueue.Items();
        for (uint32_t idx : bucket)
        {
            if (idx >= items.size()) continue;
            if (items[idx].pass != RenderPass::World) continue;
            if (items[idx].type == RenderItemType::SkinnedMesh)
            {
                hasSkinned = true;
                break;
            }
        }

        if (hasSkinned)
        {
            if (!EnsureSkinnedDescriptors())
            {
                // ここで return するかどうかは方針次第
                // Skinnedだけ落とすなら return せず描けるものだけ描く、でもOK
                // いまは安全側
                return;
            }
        }
    }

    // per-frame UBO update（この bucket で 1回だけ）
    UpdateWorldCommonUBO(mImageIndex);
    UpdateDirLightUBO(mImageIndex);
    UpdatePointLightUBO(mImageIndex);

    // viewport / scissor
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
