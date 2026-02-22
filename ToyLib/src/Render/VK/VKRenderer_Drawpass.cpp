//======================================================================
// Render/VK/VKRenderer_Drawpass.cpp
//  - Draw phases / DrawToRenderTarget / DrawItem
//  - いまは “build通し＆clear-only” を維持しつつ、Sprite(Texture) を最低限描けるようにする
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
// Sprite PushConstants (must match Sprite shaders)
//  - mat4 world (64)
//  - vec4 colorAlpha (16)
//==============================================================
struct VKSpritePC
{
    float world[16];
    float colorAlpha[4];
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

    // だいたい GL と同じ構造でOK（Bucketは IRenderer 側で作られてる）
    DrawBucket_World(mBuckets.worldOpaque);
    DrawBucket_World(mBuckets.effectPre);

    // 透明は後ろ
    DrawBucket_World(mBuckets.worldTransparent);
    DrawBucket_World(mBuckets.effectOverlay);
}

void VKRenderer::DrawOverlayScreenPass() {}
void VKRenderer::DrawFadePass() {}
void VKRenderer::DrawPostEffectPass() {}

void VKRenderer::DrawUIPass()
{
    // UI も SceneUBO（view/proj）を使うなら、ここで更新してもOK
    // （WorldPassで更新済みなら不要だが、将来 pass ごとに変える可能性があるなら安全寄り）
    UpdateSceneUBO();

    DrawBucket_World(mBuckets.ui); // UIも DrawItem は pass=UI として来る
}

//--------------------------------------------------------------
// DrawItem (bucketed draw path)
//  - まずは Sprite(Texture) のみ「最低限」実装
//  - 他 type は “何もしない” で build 通しを維持
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

    //==========================================================
    // Sprite(Texture) : minimum
    //==========================================================
    if (it.type != RenderItemType::Sprite)
    {
        return; // build通し維持：他は後で追加
    }

    // Pipeline
    VKPipeline* pipe = mPipelines.Get("Sprite");
    if (!pipe || !pipe->IsValid())
    {
        return;
    }

    // Geometry (VertexArray -> VK buffers)
    if (!BindVertexArrayVK(cmd, it.geometry))
    {
        return;
    }

    // Descriptors
    if (mSceneSet == VK_NULL_HANDLE)
    {
        return;
    }

    VkDescriptorSet texSet = GetOrCreateSpriteTextureSet(it.texture.ptr);
    if (texSet == VK_NULL_HANDLE)
    {
        return;
    }

    VkDescriptorSet sets[2] =
    {
        mSceneSet, // set=0
        texSet     // set=1
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

    // PushConstants: world + colorAlpha
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
        sizeof(VKSpritePC),
        &pc);

    // Draw
    if (it.indexCount > 0)
    {
        vkCmdDrawIndexed(
            cmd,
            static_cast<uint32_t>(it.indexCount),
            1,
            0, 0, 0);
        AddDrawCall();
    }
    else if (it.vertexCount > 0)
    {
        vkCmdDraw(
            cmd,
            static_cast<uint32_t>(it.vertexCount),
            1,
            0, 0);
        AddDrawCall();
    }

    // NOTE:
    // depth/blend/cull など RenderItem の state は
    // いまは pipeline 固定（Preset）で運用。
    // 将来、PipelineKey を組んで切り替える or dynamic state 対応で拡張する。
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
