//======================================================================
// Render/VK/VKRenderer_Drawpass.cpp
//  - Draw phases / DrawToRenderTarget / DrawItem
//  - DrawItem: Sprite / Mesh / SkinnedMesh
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

//==============================================================
// Sprite PushConstants (80 bytes)
//  - mat4 world (64)
//  - vec4 colorAlpha (16)
//==============================================================
struct VKSpritePC
{
    float world[16];
    float colorAlpha[4];
};

//==============================================================
// Mesh/Skinned PushConstants (112 bytes)
//  - mat4 world (64)
//  - vec4 baseColor_useTex (16)  : rgb + useTex(0/1)
//  - vec4 misc (16)              : specPower, toon(0/1), overrideEnabled(0/1), alpha
//  - vec4 overrideColor (16)     : rgb + pad
//==============================================================
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

//--------------------------------------------------------------
// RTT: DrawToRenderTarget
//  - まずは安全に「別submit」で描く（ビルド通し優先）
//--------------------------------------------------------------
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

    // camera override
    PushCameraState();
    {
        CameraState s{};
        s.view     = req.view;
        s.proj     = req.proj;
        s.invView  = req.view;
        s.invView.Invert();
        SetCameraState(s);

        // SceneUBO は view/proj から毎回詰める運用に寄せる
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

    // TODO:
    // ここで bucket を回して DrawItem(...) を呼ぶ
    // 現状は clear-only でOK（RTT 経路は後で拡張）

    vkCmdEndRenderPass(cmd);

    PopCameraState();

    EndOneTimeCommands(cmd);
}

//--------------------------------------------------------------
// Draw phases（GL と同じ構造で拡張していく）
//--------------------------------------------------------------
void VKRenderer::DrawShadowPass() {}
void VKRenderer::RestoreAfterShadowPass() {}
void VKRenderer::DrawSkyPass() {}

void VKRenderer::DrawWorldPass()
{
    // SceneUBO（view/proj）を最新化
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
    // UI も SceneUBO（view/proj）を使うなら、ここで更新してもOK
    UpdateSceneUBO();

    DrawBucket_World(mBuckets.ui);
}

//--------------------------------------------------------------
// DrawItem (bucketed draw path)
//  - Sprite(Texture) / Mesh / SkinnedMesh の最低限
//  - depth/blend/cull 等は「いまは pipeline preset 固定」
//--------------------------------------------------------------
void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    (void)pass;
    (void)cascadeIndex;

    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (!cmd)
    {
        return;
    }

    // 共通: Geometry
    if (!BindVertexArrayVK(cmd, it.geometry))
    {
        return;
    }

    // 共通: Scene set
    if (mSceneSet == VK_NULL_HANDLE)
    {
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

        VkDescriptorSet baseMapSet = GetOrCreateBaseMapSet(it.texture.ptr, "Sprite");
        if (baseMapSet == VK_NULL_HANDLE)
        {
            return;
        }

        VkDescriptorSet sets[2] =
        {
            mSceneSet,     // set=0
            baseMapSet     // set=1
        };

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

        // baseMap (set=1)
        if (!it.texture.ptr)
        {
            // 「まず表示経路」優先：テクスチャ無しは一旦スキップ
            return;
        }

        VkDescriptorSet baseMapSet = GetOrCreateBaseMapSet(it.texture.ptr, "Mesh");
        if (baseMapSet == VK_NULL_HANDLE)
        {
            return;
        }

        VkDescriptorSet sets[2] =
        {
            mSceneSet,    // set=0
            baseMapSet    // set=1
        };

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

        // 最小：payload未整備でも描ける固定値
        pc.baseColor_useTex[0] = 1.0f;
        pc.baseColor_useTex[1] = 1.0f;
        pc.baseColor_useTex[2] = 1.0f;
        pc.baseColor_useTex[3] = 1.0f; // useTex=1

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
    // SkinnedMesh
    //==========================================================
    if (it.type == RenderItemType::SkinnedMesh)
    {
        VKPipeline* pipe = mPipelines.Get("SkinnedMesh");
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        // baseMap (set=1)
        if (!it.texture.ptr)
        {
            return;
        }

        VkDescriptorSet baseMapSet = GetOrCreateBaseMapSet(it.texture.ptr, "SkinnedMesh");
        if (baseMapSet == VK_NULL_HANDLE)
        {
            return;
        }

        // set=2 (palette UBO/SSBO) が RenderItem/Payload にまだ無いなら、ここは次の段階。
        // いまは「経路を壊さない」ためにスキップでOK。
        (void)baseMapSet;

        return;
    }

    // 未対応は何もしない
}

//--------------------------------------------------------------
// PipelineHandle
//--------------------------------------------------------------
PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    (void)name;
    return {};
}

} // namespace toy
