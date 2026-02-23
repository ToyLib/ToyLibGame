//======================================================================
// Render/VK/VKRenderer_Drawpass.cpp
//
// 方針（確定）:
//  - SceneSet は World/UI で分離して常に使う
//      mSceneSet      : World 用 set=0
//      mSceneSet_UI   : UI 用   set=0
//  - BeginFrame() で World UBO 更新（Core側）
//  - DrawUIPass() で UI UBO 更新（ここ）
//  - DrawItem() は it.pass で SceneSet を切り替える
//  - viewProj は UBO 経由（PushConstant にしない）
//======================================================================
#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/RenderItemPayloads.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Asset/Material/Material.h"

#include <iostream>
#include <cstring>

namespace toy
{

//--------------------------------------------------------------
// PushConstants
//--------------------------------------------------------------
struct VKSpritePC
{
    float world[16];
    float colorAlpha[4];
};

struct VKMeshPC
{
    float world[16];
    float baseColor_useTex[4];  // ★ w = useTex
    float misc[4];
    float overrideColor[4];
};

static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}

//--------------------------------------------------------------
// VertexArray bind (VK backend)
//--------------------------------------------------------------
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

//======================================================================
// DrawToRenderTarget (SceneCapture)
//  - いまは “最低限枠だけ”
//  - 重要: camera state を差し替えたあと UpdateSceneUBO_World() を呼ぶ
//======================================================================
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

    //----------------------------------------------------------
    // camera override + World UBO update
    //----------------------------------------------------------
    PushCameraState();
    {
        CameraState s{};
        s.view     = req.view;
        s.proj     = req.proj;
        s.invView  = req.view;
        s.invView.Invert();
        SetCameraState(s);

        // ★ camera state を反映した world view/proj を UBOへ
        UpdateSceneUBO_World();
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

    // TODO: buckets draw (world/overlay等)

    vkCmdEndRenderPass(cmd);

    PopCameraState();

    EndOneTimeCommands(cmd);
}

//======================================================================
// Pass stubs
//======================================================================
void VKRenderer::DrawShadowPass() {}
void VKRenderer::RestoreAfterShadowPass() {}
void VKRenderer::DrawSkyPass() {}

void VKRenderer::DrawWorldPass()
{
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
    const float sw = mScreenWidth;
    const float sh = mScreenHeight;

    const Matrix4 uiVP = Matrix4::CreateSimpleViewProj(sw, sh);

    // ★UI用 UBO に viewProj を入れる（Worldとは別）
    UpdateSceneUBO_UI(uiVP);

    // ★既存の描画経路を使う（新しい関数は増やさない）
    DrawBucket_World(mBuckets.ui);
}

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

    if (!BindVertexArrayVK(cmd, it.geometry))
    {
        return;
    }

    //----------------------------------------------------------
    // SceneSet 選択（World / UI）
    //----------------------------------------------------------
    const bool isUI = (it.pass == RenderPass::UI);

    VkDescriptorSet sceneSet = VK_NULL_HANDLE;
    if (isUI)
    {
        if (mFrameIndex >= mSceneSet_UI.size())
        {
            return;
        }
        sceneSet = mSceneSet_UI[mFrameIndex];
    }
    else
    {
        if (mFrameIndex >= mSceneSet.size())
        {
            return;
        }
        sceneSet = mSceneSet[mFrameIndex];
    }

    if (sceneSet == VK_NULL_HANDLE)
    {
        return;
    }

    //----------------------------------------------------------
    // Pipeline name（FrontFace で Mesh / Mesh_CW を分岐）
    //----------------------------------------------------------
    const char* pipelineName = nullptr;

    switch (it.type)
    {
        case RenderItemType::Sprite:
            pipelineName = "Sprite";
            break;

        case RenderItemType::Mesh:
            pipelineName = (it.frontFace == FrontFace::CCW) ? "Mesh" : "Mesh_CW";
            break;

        case RenderItemType::SkinnedMesh:
            pipelineName = "SkinnedMesh";
            break;

        default:
            return;
    }

    //----------------------------------------------------------
    // BaseMap set=1（※ pipelineName を必ず使う）
    //----------------------------------------------------------
    VkDescriptorSet baseMapSet = GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
    if (baseMapSet == VK_NULL_HANDLE)
    {
        return;
    }

    //----------------------------------------------------------
    // Sprite
    //----------------------------------------------------------
    if (it.type == RenderItemType::Sprite)
    {
        VKPipeline* pipe = mPipelines.Get("Sprite");
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        VkDescriptorSet sets[2] = { sceneSet, baseMapSet };

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

        Vector3 color(1, 1, 1);
        float alpha = 1.0f;

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

        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
            AddDrawCall();
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);
            AddDrawCall();
        }

        return;
    }

    //----------------------------------------------------------
    // Mesh
    //----------------------------------------------------------
    if (it.type == RenderItemType::Mesh)
    {
        // ★ここが一番大事：pipelineName を使う
        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        // set0=setScene, set1=baseMap
        VkDescriptorSet sets[2] = { sceneSet, VK_NULL_HANDLE };

        //------------------------------------------------------
        // Material / payload
        //------------------------------------------------------
        Vector3 baseColor(1.0f, 1.0f, 1.0f);
        float specPower = 64.0f;
        float alpha     = 1.0f;

        float toon            = 0.0f;
        float overrideEnabled = 0.0f;
        Vector3 overrideColor(0.0f, 0.0f, 0.0f);

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const MeshPayload& mp = GetMeshPayload(it.payloadIndex);
            toon            = mp.toon ? 1.0f : 0.0f;
            overrideEnabled = mp.overrideColor ? 1.0f : 0.0f;
            overrideColor   = mp.overrideColorValue;
        }

        //------------------------------------------------------
        // Texture / useTex
        //  - 実テクスチャがある時だけ useTex=1
        //  - DS は tex=nullptr なら fallback だが useTex は 0
        //------------------------------------------------------
        const Texture* diffuseTex = it.texture.ptr;
        const bool hasRealTex = (diffuseTex != nullptr);
        float useTex = 0.0f;

        Material* mat = it.material.ptr; // あなたの現状の形に合わせている
        if (mat)
        {
            baseColor = mat->GetDiffuseColor();
            specPower = mat->GetSpecPower();

            // WantsUseTexture の意思も見るならここ
            if (mat->WantsUseTexture() && hasRealTex)
            {
                useTex = 1.0f;
            }
        }
        else
        {
            // matが無い場合は “実テクスチャがあるなら使う” のほうが自然ならこれ
            // useTex = hasRealTex ? 1.0f : 0.0f;
        }

        // ★BaseMapSetも pipelineName を使う（layout違い対策）
        baseMapSet = GetOrCreateBaseMapSet(diffuseTex, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE)
        {
            return;
        }
        sets[1] = baseMapSet;

        //------------------------------------------------------
        // Bind & DS
        //------------------------------------------------------
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

        //------------------------------------------------------
        // PushConstants
        //------------------------------------------------------
        VKMeshPC pc{};
        StoreMat4(pc.world, it.world);

        pc.baseColor_useTex[0] = baseColor.x;
        pc.baseColor_useTex[1] = baseColor.y;
        pc.baseColor_useTex[2] = baseColor.z;
        pc.baseColor_useTex[3] = useTex;

        pc.misc[0] = specPower;
        pc.misc[1] = toon;
        pc.misc[2] = overrideEnabled;
        pc.misc[3] = alpha;

        pc.overrideColor[0] = overrideColor.x;
        pc.overrideColor[1] = overrideColor.y;
        pc.overrideColor[2] = overrideColor.z;
        pc.overrideColor[3] = 1.0f;

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(VKMeshPC),
            &pc);

        //------------------------------------------------------
        // Draw
        //------------------------------------------------------
        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
            AddDrawCall();
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);
            AddDrawCall();
        }

        return;
    }

    //----------------------------------------------------------
    // SkinnedMesh（未実装）
    //----------------------------------------------------------
    // if (it.type == RenderItemType::SkinnedMesh) { ... }
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    (void)name;
    return {};
}

} // namespace toy
