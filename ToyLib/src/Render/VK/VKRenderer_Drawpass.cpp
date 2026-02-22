//======================================================================
// Render/VK/VKRenderer_Drawpass.cpp
//======================================================================
#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/RenderItemPayloads.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"

#include <iostream>
#include <cstring>

namespace toy
{

struct VKSpritePC
{
    float world[16];
    float colorAlpha[4];
};

struct VKMeshPC
{
    float world[16];
    float baseColor_useTex[4];
    float misc[4];
    float overrideColor[4];
};

static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}

static bool BindVertexArrayVK(VkCommandBuffer cmd, const GeometryHandle& gh)
{
    if (!cmd)
    {
        return false;
    }

    const VertexArray* va = gh.ptr;
    if (!va)
    {
        return false;
    }

    auto* backend = (VKVertexArrayBackend*)va->GetBackend();
    if (!backend)
    {
        return false;
    }

    VkBuffer vb = (VkBuffer)backend->GetVKVertexBuffer();
    if (vb == VK_NULL_HANDLE)
    {
        return false;
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);

    VkBuffer ib = (VkBuffer)backend->GetVKIndexBuffer();
    if (ib != VK_NULL_HANDLE)
    {
        vkCmdBindIndexBuffer(cmd, ib, 0, (VkIndexType)backend->GetVKIndexType());
    }

    return true;
}

void VKRenderer::DrawToRenderTarget(const SceneCaptureRequest& req)
{
    if (!req.rt)
    {
        return;
    }

    auto* vkrt = dynamic_cast<VKSceneRenderTarget*>(req.rt.get());
    if (!vkrt)
    {
        return;
    }

    if (mDevice == VK_NULL_HANDLE || mQueueGraphics == VK_NULL_HANDLE || mCommandPool == VK_NULL_HANDLE)
    {
        return;
    }

    if (vkrt->GetWidth() <= 0 || vkrt->GetHeight() <= 0 ||
        vkrt->GetFramebuffer() == VK_NULL_HANDLE)
    {
        const int w = (int)mScreenWidth;
        const int h = (int)mScreenHeight;
        if (!vkrt->Create(w, h))
        {
            std::cerr << "[VKRenderer] DrawToRenderTarget: vkrt->Create failed.\n";
            return;
        }
    }

    VkCommandBuffer cmd = BeginOneTimeCommands();
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    PushCameraState();
    {
        CameraState s{};
        s.view     = req.view;
        s.proj     = req.proj;
        s.invView  = req.view;
        s.invView.Invert();
        SetCameraState(s);

        UpdateSceneUBO();
    }

    VkClearValue clears[2]{};
    clears[0].color.float32[0] = mClearColor.x;
    clears[0].color.float32[1] = mClearColor.y;
    clears[0].color.float32[2] = mClearColor.z;
    clears[0].color.float32[3] = 1.0f;
    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass  = vkrt->GetRenderPass();
    rp.framebuffer = vkrt->GetFramebuffer();
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = vkrt->GetExtent();
    rp.clearValueCount   = 2;
    rp.pClearValues      = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width  = (float)rp.renderArea.extent.width;
    vp.height = (float)rp.renderArea.extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = rp.renderArea.extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // TODO: bucket draw

    vkCmdEndRenderPass(cmd);

    PopCameraState();

    EndOneTimeCommands(cmd);
}

void VKRenderer::DrawShadowPass() {}
void VKRenderer::RestoreAfterShadowPass() {}
void VKRenderer::DrawSkyPass() {}

void VKRenderer::DrawWorldPass()
{
    UpdateSceneUBO();

    DrawBucket_World(mBuckets.worldOpaque);
    DrawBucket_World(mBuckets.effectPre);

    DrawBucket_World(mBuckets.worldTransparent);
    DrawBucket_World(mBuckets.effectOverlay);
}

void VKRenderer::DrawOverlayScreenPass() {}
void VKRenderer::DrawFadePass() {}
void VKRenderer::DrawPostEffectPass() {}

void VKRenderer::DrawUIPass()
{
    UpdateSceneUBO();
    DrawBucket_World(mBuckets.ui);
}

//======================================================================
// DrawItem
//======================================================================
void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    (void)pass;
    (void)cascadeIndex;

    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    // debug
    // std::cerr << "[VK] DrawItem type=" << (int)it.type
    //           << " vtx=" << it.vertexCount << " idx=" << it.indexCount
    //           << " geo=" << (void*)it.geometry.ptr << " tex=" << (void*)it.texture.ptr << "\n";

    if (!BindVertexArrayVK(cmd, it.geometry))
    {
        return;
    }

    // set=0 Scene
    if (mSceneSet == VK_NULL_HANDLE)
    {
        return;
    }

    // set=1 BaseMap (nullでもfallbackが返る)
    const char* pipelineName = nullptr;
    if (it.type == RenderItemType::Sprite)      pipelineName = "Sprite";
    if (it.type == RenderItemType::Mesh)        pipelineName = "Mesh";
    if (it.type == RenderItemType::SkinnedMesh) pipelineName = "SkinnedMesh";

    VkDescriptorSet baseMapSet = GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
    if (baseMapSet == VK_NULL_HANDLE)
    {
        // fallbackが無い状態は描けない
        return;
    }

    //==========================================================
    // Sprite
    //==========================================================
    if (it.type == RenderItemType::Sprite)
    {
        VKPipeline* pipe = mPipelines.Get("Sprite");
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        VkDescriptorSet sets[2] = { mSceneSet, baseMapSet };

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0,
            2,
            sets,
            0,
            nullptr);

        Vector3 color(1.0f, 1.0f, 1.0f);
        float   alpha = 1.0f;

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const SpritePayload& sp = GetSpritePayload(it.payloadIndex);
            color = sp.color;
            alpha = sp.alpha;
        }

        VKSpritePC pc{};
        StoreMat4(pc.world, it.world);
        pc.colorAlpha[0] = color.x;
        pc.colorAlpha[1] = color.y;
        pc.colorAlpha[2] = color.z;
        pc.colorAlpha[3] = alpha;

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            (uint32_t)sizeof(VKSpritePC),
            &pc);

        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, (uint32_t)it.indexCount, 1, 0, 0, 0);
            AddDrawCall();
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, (uint32_t)it.vertexCount, 1, 0, 0);
            AddDrawCall();
        }

        return;
    }

    //==========================================================
    // Mesh
    //==========================================================
    if (it.type == RenderItemType::Mesh)
    {
        
        VKPipeline* pipe = mPipelines.Get("Mesh");
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        VkDescriptorSet sets[2] = { mSceneSet, baseMapSet };

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0,
            2,
            sets,
            0,
            nullptr);

        VKMeshPC pc{};
        StoreMat4(pc.world, it.world);

        // fallbackが白なので useTex=1 でも破綻しない
        pc.baseColor_useTex[0] = 1.0f;
        pc.baseColor_useTex[1] = 1.0f;
        pc.baseColor_useTex[2] = 1.0f;
        pc.baseColor_useTex[3] = 1.0f;

        pc.misc[0] = 64.0f; // specPower
        pc.misc[1] = 0.0f;  // toon
        pc.misc[2] = 0.0f;  // overrideEnabled
        pc.misc[3] = 1.0f;  // alpha

        pc.overrideColor[0] = 1.0f;
        pc.overrideColor[1] = 0.0f;
        pc.overrideColor[2] = 1.0f;
        pc.overrideColor[3] = 1.0f;

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            (uint32_t)sizeof(VKMeshPC),
            &pc);

        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, (uint32_t)it.indexCount, 1, 0, 0, 0);
            AddDrawCall();
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, (uint32_t)it.vertexCount, 1, 0, 0);
            AddDrawCall();
        }
        

        return;
    }

    //==========================================================
    // SkinnedMesh (set=2 が未整備なら描かない方針のまま)
    //==========================================================
    if (it.type == RenderItemType::SkinnedMesh)
    {
        VKPipeline* pipe = mPipelines.Get("SkinnedMesh");
        if (!pipe || !pipe->IsValid())
        {
            return;
        }
        return;
    }
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    (void)name;
    return {};
}

} // namespace toy
