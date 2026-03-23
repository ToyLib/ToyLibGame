//======================================================================
// Render/VK/VKRenderer_Drawpass.cpp
//
// 方針（確定）:
//  - SceneSet は World/UI で分離して常に使う
//      mSceneSet      : World 用 set=0
//      mSceneSet_UI   : UI 用   set=0
//  - BeginFrame() で World UBO 更新（Core側）
//  - DrawUIPass() で UI UBO 更新（ここ）
//  - DrawItem() は 引数 pass で SceneSet を切り替える
//  - viewProj は UBO 経由（PushConstant にしない）
//  - Skinned は AcquireSkinnedSet()（set=2）で “draw単位で安全に更新”
//
// 重要（Mac 真っ黒対策）:
//  - Swapchain の renderpass は World + UI を “同一 renderpass” 内で描く
//    -> DrawWorldPass() で Begin するが End しない
//    -> EndSwapchainRenderPassIfNeeded() を EndFrame() 前に呼ぶ
//======================================================================
#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPushConstants.h"

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


static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}

//--------------------------------------------------------------
// VertexArray bind (VK backend)
//--------------------------------------------------------------
static bool BindVertexArrayVK(VkCommandBuffer cmd, const GeometryHandle& gh)
{
    if (!cmd) return false;

    const VertexArray* va = gh.ptr;
    if (!va) return false;

    auto* backend = (VKVertexArrayBackend*)va->GetBackend();
    if (!backend) return false;

    VkBuffer vb = (VkBuffer)backend->GetVKVertexBuffer();
    if (vb == VK_NULL_HANDLE) return false;

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
// Swapchain renderpass helpers
//--------------------------------------------------------------
static inline bool IsValidExtent(const VkExtent2D& e)
{
    return e.width > 0 && e.height > 0;
}

void VKRenderer::BeginSwapchainRenderPassIfNeeded()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty()) return;
    if (mIsInRenderPass) return;

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE) return;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent{};

    if (mRenderToSceneRTThisFrame)
    {
        auto* vkrt = dynamic_cast<VKSceneRenderTarget*>(mSceneRT.get());
        if (!vkrt) return;

        renderPass  = vkrt->GetRenderPass();
        framebuffer = vkrt->GetFramebuffer();
        extent      = vkrt->GetExtent();
    }
    else
    {
        if (!IsValidExtent(mSwapchainExtent)) return;
        if (mRenderPass == VK_NULL_HANDLE) return;
        if (mImageIndex >= mFramebuffers.size()) return;
        if (mFramebuffers[mImageIndex] == VK_NULL_HANDLE) return;

        renderPass  = mRenderPass;
        framebuffer = mFramebuffers[mImageIndex];
        extent      = mSwapchainExtent;
    }

    VkClearValue clears[2]{};
    clears[0].color.float32[0] = mClearColor.x;
    clears[0].color.float32[1] = mClearColor.y;
    clears[0].color.float32[2] = mClearColor.z;
    clears[0].color.float32[3] = 1.0f;
    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass;
    rp.framebuffer = framebuffer;
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = extent;
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    mIsInRenderPass = true;

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = (float)extent.height;
    vp.width  = (float)extent.width;
    vp.height = -(float)extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

void VKRenderer::EndSwapchainRenderPassIfNeeded()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty()) return;
    if (!mIsInRenderPass) return;

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE) return;

    vkCmdEndRenderPass(cmd);
    mIsInRenderPass = false;
}


//======================================================================
// Passes
//======================================================================
void VKRenderer::DrawSkyPass()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    if (mBuckets.sky.empty())
    {
        return;
    }

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    DrawBucket_Sky(mBuckets.sky);
}

void VKRenderer::DrawWorldPass()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty()) return;

    BeginSwapchainRenderPassIfNeeded();

    if (!mIsInRenderPass)
    {
        return;
    }

    DrawBucket_World(mBuckets.worldOpaque);
    DrawBucket_World(mBuckets.effectPre);
    DrawBucket_World(mBuckets.worldTransparent);
    DrawBucket_World(mBuckets.effectOverlay);
}

void VKRenderer::DrawOverlayScreenPass()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    if (mBuckets.overlayScreen.empty())
    {
        return;
    }

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    DrawBucket_OverlayScreen(mBuckets.overlayScreen);
}

void VKRenderer::DrawFadePass()
{
    if (!mEnableFade)
    {
        return;
    }

    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    if (!mFullScreenQuad)
    {
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    GeometryHandle gh{};
    gh.ptr = mFullScreenQuad.get();

    if (!BindVertexArrayVK(cmd, gh))
    {
        return;
    }

    VKPipeline* pipe = mPipelines.Get("Fade");
    if (!pipe || !pipe->IsValid())
    {
        return;
    }

    pipe->Bind(cmd);

    VKFadePC pc{};
    pc.colorAlpha[0] = mFadeColor.x;
    pc.colorAlpha[1] = mFadeColor.y;
    pc.colorAlpha[2] = mFadeColor.z;
    pc.colorAlpha[3] = mFadeAlpha;

    vkCmdPushConstants(
        cmd,
        pipe->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(VKFadePC),
        &pc);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    AddDrawCall();
}


void VKRenderer::DrawUIPass()
{
    const float sw = mScreenWidth;
    const float sh = mScreenHeight;

    const Matrix4 uiVP = Matrix4::CreateSimpleViewProj(sw, sh);
    UpdateSceneUBO_UI(uiVP);

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    DrawBucket_UI(mBuckets.ui);
    
    // 念のため World 用 UBO に戻す
    UpdateSceneUBO_World();
}

//======================================================================
// DrawItem
//======================================================================
void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    (void)cascadeIndex;

    if (mDevice == VK_NULL_HANDLE || mFrames.empty()) return;

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE) return;

    
    if (it.type != RenderItemType::Particle)
    {
        BindVertexArrayVK(cmd, it.geometry);
    }
    
    //----------------------------------------------------------
    // SceneSet 選択（World / UI / Shadow）
    //----------------------------------------------------------
    const bool isUI     = (pass == RenderPass::UI);
    const bool isShadow = (pass == RenderPass::Shadow);

    VkDescriptorSet sceneSet = VK_NULL_HANDLE;

    if (isShadow)
    {
        if (cascadeIndex < 0 || cascadeIndex >= kShadowCascadeCount) return;
        if (mFrameIndex >= mShadowSceneSet[cascadeIndex].size()) return;
        sceneSet = mShadowSceneSet[cascadeIndex][mFrameIndex];
    }
    else if (isUI)
    {
        if (mFrameIndex >= mSceneSet_UI.size()) return;
        sceneSet = mSceneSet_UI[mFrameIndex];
    }
    else
    {
        if (mIsDrawingCapture)
        {
            if (mFrameIndex >= mSceneSet_Capture.size()) return;
            if (mActiveCaptureSlot < 0) return;
            if ((size_t)mActiveCaptureSlot >= mSceneSet_Capture[mFrameIndex].size()) return;

            sceneSet = mSceneSet_Capture[mFrameIndex][mActiveCaptureSlot];
        }
        else
        {
            if (mFrameIndex >= mSceneSet.size()) return;
            sceneSet = mSceneSet[mFrameIndex];
        }
    }

    //----------------------------------------------------------
    // Pipeline name
    //----------------------------------------------------------
    const char* pipelineName = nullptr;

    if (isShadow)
    {
        switch (it.type)
        {
            case RenderItemType::Mesh:        pipelineName = "ShadowMesh";    break;
            case RenderItemType::SkinnedMesh: pipelineName = "ShadowSkinned"; break;
            default: return;
        }
    }
    else
    {
        switch (it.type)
        {
            case RenderItemType::Sprite:
                pipelineName = "Sprite";
                break;
            case RenderItemType::Mesh:
                pipelineName = (it.frontFace == FrontFace::CCW) ? "Mesh" : "Mesh_CW";
                break;
            case RenderItemType::SkinnedMesh:
                pipelineName = (it.frontFace == FrontFace::CCW) ? "SkinnedMesh" : "SkinnedMesh_CW";
                break;
            case RenderItemType::UnlitQuad:
                pipelineName = "UnlitQuad";
                break;
            case RenderItemType::SkyDome:
                pipelineName = "SkyDome";
                break;
            case RenderItemType::Overlay:
                pipelineName = (it.blend == BlendMode::Additive)
                    ? "WeatherOverlayAdd"
                    : "WeatherOverlay";
                break;
            case RenderItemType::Surface:
                pipelineName = "RenderSurface";
                break;
            case RenderItemType::Debug:
                pipelineName = "UnlitWire";
                break;
            case RenderItemType::Particle:
                pipelineName = (it.blend == BlendMode::Additive)
                    ? "Particle"
                    : "Particle_Alpha";
                    break;
            default:
                return;
        }
    }

    //----------------------------------------------------------
    // Shadow pass
    //----------------------------------------------------------
    if (isShadow)
    {
        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid()) return;

        pipe->Bind(cmd);

        if (it.type == RenderItemType::Mesh)
        {
            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipe->GetPipelineLayout(),
                0, 1, &sceneSet,
                0, nullptr);

            VKShadowPC pc{};
            StoreMat4(pc.world, it.world);

            vkCmdPushConstants(
                cmd, pipe->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0, sizeof(VKShadowPC), &pc);

            if (it.indexCount > 0)
                vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
            else if (it.vertexCount > 0)
                vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

            AddDrawCall();
            return;
        }

        if (it.type == RenderItemType::SkinnedMesh)
        {
            VkDescriptorSet skinnedSet =
                AcquireSkinnedSet(it.matrixPalette,
                                  (uint32_t)it.paletteCount,
                                  pipelineName);
            if (skinnedSet == VK_NULL_HANDLE) return;

            VkDescriptorSet emptySet1 = GetOrCreateEmptySet(pipelineName, 1);
            if (emptySet1 == VK_NULL_HANDLE) return;

            VkDescriptorSet sets[3] = { sceneSet, emptySet1, skinnedSet };

            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipe->GetPipelineLayout(),
                0, 3, sets,
                0, nullptr);

            VKShadowPC pc{};
            StoreMat4(pc.world, it.world);

            vkCmdPushConstants(
                cmd, pipe->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0, sizeof(VKShadowPC), &pc);

            if (it.indexCount > 0)
                vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
            else if (it.vertexCount > 0)
                vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

            AddDrawCall();
            return;
        }

        return;
    }

    //----------------------------------------------------------
    // Sprite
    //----------------------------------------------------------
    if (it.type == RenderItemType::Sprite)
    {
        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE) return;

        VKPipeline* pipe = mPipelines.Get("Sprite");
        if (!pipe || !pipe->IsValid()) return;

        VkDescriptorSet sets[2] = { sceneSet, baseMapSet };

        pipe->Bind(cmd);
        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 2, sets,
            0, nullptr);

        Vector3 color(1,1,1);
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
            cmd, pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(VKSpritePC), &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // Mesh
    //----------------------------------------------------------
    if (it.type == RenderItemType::Mesh)
    {
        VkDescriptorSet shadowSet = GetShadowMapSetForCurrentFrame();
        if (shadowSet == VK_NULL_HANDLE) return;

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid()) return;

        Material* mat = it.material.ptr;
        const Texture* diffuseTex =
            (mat) ? mat->GetDiffuseMap().get() : nullptr;

        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(diffuseTex, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE) return;

        VkDescriptorSet emptySet2 =
            GetOrCreateEmptySet(pipelineName, 2);
        if (emptySet2 == VK_NULL_HANDLE) return;

        VkDescriptorSet sets[4] =
        {
            sceneSet,
            baseMapSet,
            emptySet2,
            shadowSet
        };

        pipe->Bind(cmd);
        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 4, sets,
            0, nullptr);

        Vector3 baseColor(1,1,1);
        float specPower = 64.0f;
        float alpha = 1.0f;

        float toon = 0.0f;
        float overrideEnabled = 0.0f;
        Vector3 overrideColor(0,0,0);

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const MeshPayload& mp = GetMeshPayload(it.payloadIndex);
            toon = mp.toon ? 1.0f : 0.0f;
            overrideEnabled = mp.overrideColor ? 1.0f : 0.0f;
            overrideColor = mp.overrideColorValue;
        }

        float useTex = 0.0f;

        if (mat)
        {
            baseColor = mat->GetDiffuseColor();
            specPower = mat->GetSpecPower();

            if (mat->WantsUseTexture() && diffuseTex)
                useTex = 1.0f;
        }

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
            cmd, pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(VKMeshPC), &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // SkinnedMesh
    //----------------------------------------------------------
    if (it.type == RenderItemType::SkinnedMesh)
    {
        VkDescriptorSet shadowSet = GetShadowMapSetForCurrentFrame();
        if (shadowSet == VK_NULL_HANDLE) return;

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid())
        {
            pipe = mPipelines.Get("SkinnedMesh");
            if (!pipe || !pipe->IsValid()) return;
            pipelineName = "SkinnedMesh";
        }

        VkDescriptorSet skinnedSet =
            AcquireSkinnedSet(it.matrixPalette,
                              (uint32_t)it.paletteCount,
                              pipelineName);
        if (skinnedSet == VK_NULL_HANDLE) return;

        Material* mat = it.material.ptr;
        const Texture* diffuseTex =
            (mat) ? mat->GetDiffuseMap().get() : nullptr;

        Vector3 baseColor(1.0f, 1.0f, 1.0f);
        float   specPower = 64.0f;
        float   alpha     = 1.0f;

        float toon            = 0.0f;
        float overrideEnabled = 0.0f;
        Vector3 overrideColor(0.0f, 0.0f, 0.0f);

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const SkinnedMeshPayload& sp = GetSkinnedMeshPayload(it.payloadIndex);
            toon            = sp.toon ? 1.0f : 0.0f;
            overrideEnabled = sp.overrideColor ? 1.0f : 0.0f;
            overrideColor   = sp.overrideColorValue;
        }

        float useTex = 0.0f;

        if (mat)
        {
            baseColor = mat->GetDiffuseColor();
            specPower = mat->GetSpecPower();

            if (mat->WantsUseTexture() && diffuseTex)
                useTex = 1.0f;
        }

        if (overrideEnabled > 0.5f)
        {
            useTex     = 0.0f;
            baseColor  = overrideColor;
            diffuseTex = nullptr;
        }

        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(diffuseTex, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE) return;

        VkDescriptorSet sets[4] =
        {
            sceneSet,
            baseMapSet,
            skinnedSet,
            shadowSet
        };

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 4, sets,
            0, nullptr);

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
            0, sizeof(VKMeshPC),
            &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // UnlitQuad
    //----------------------------------------------------------
    if (it.type == RenderItemType::UnlitQuad)
    {
        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE) return;

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid()) return;

        VkDescriptorSet sets[2] = { sceneSet, baseMapSet };

        pipe->Bind(cmd);
        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 2, sets,
            0, nullptr);

        Vector3 tint(1.0f, 1.0f, 1.0f);
        float   alpha = 1.0f;

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const UnlitQuadPayload& up = GetUnlitQuadPayload(it.payloadIndex);
            tint  = up.tint;
            alpha = up.alpha;
        }

        VKUnlitQuadPC pc{};
        StoreMat4(pc.world, it.world);
        pc.tintAlpha[0] = tint.x;
        pc.tintAlpha[1] = tint.y;
        pc.tintAlpha[2] = tint.z;
        pc.tintAlpha[3] = alpha;

        vkCmdPushConstants(
            cmd, pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(VKUnlitQuadPC), &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // SkyDome
    //----------------------------------------------------------
    if (it.type == RenderItemType::SkyDome)
    {
        if (mFrameIndex >= mSkySet.size())
        {
            return;
        }

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        VkDescriptorSet skySet = mSkySet[mFrameIndex];
        if (skySet == VK_NULL_HANDLE)
        {
            return;
        }

        SkyDomePayload sky {};
        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            sky = GetSkyDomePayload(it.payloadIndex);
        }

        UpdateSkyUBO(sky);

        pipe->Bind(cmd);

        VkDescriptorSet sets[2] =
        {
            sceneSet,
            skySet
        };

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 2, sets,
            0, nullptr);

        VKSkyPC pc {};
        StoreMat4(pc.world, it.world);

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(VKSkyPC),
            &pc);

        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);
        }

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // OverlayScreen / WeatherOverlay
    //----------------------------------------------------------
    if (it.type == RenderItemType::Overlay)
    {
        if (mFrameIndex >= mOverlaySet.size())
        {
            return;
        }

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        VkDescriptorSet overlaySet = mOverlaySet[mFrameIndex];
        if (overlaySet == VK_NULL_HANDLE)
        {
            return;
        }

        OverlayPayload op {};
        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            op = GetOverlayPayload(it.payloadIndex);
        }

        UpdateOverlayUBO(op);

        pipe->Bind(cmd);

        // Preset 側が set=1 のみ使用する設計
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            1, 1, &overlaySet,
            0, nullptr);

        if (it.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        }
        else if (it.vertexCount > 0)
        {
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);
        }

        AddDrawCall();
        return;
    }

    //----------------------------------------------------------
    // Debug / UnlitWire
    //----------------------------------------------------------
    if (it.type == RenderItemType::Debug)
    {
        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid()) return;

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 1, &sceneSet,
            0, nullptr);

        Vector3 color(1.0f, 1.0f, 1.0f);
        float   alpha    = 1.0f;
        float   useLight = 0.0f;

        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            const DebugPayload& dp = GetDebugPayload(it.payloadIndex);
            color = dp.color;
            alpha = dp.alpha;
        }

        VKDebugPC pc{};
        StoreMat4(pc.world, it.world);

        pc.color[0] = color.x;
        pc.color[1] = color.y;
        pc.color[2] = color.z;
        pc.color[3] = alpha;

        pc.params[0] = useLight;
        pc.params[1] = 0.0f;
        pc.params[2] = 0.0f;
        pc.params[3] = 0.0f;

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(VKDebugPC),
            &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }
    //----------------------------------------------------------
    // Surface
    //----------------------------------------------------------
    if (it.type == RenderItemType::Surface)
    {
        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE) return;

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid()) return;

        VkDescriptorSet sets[2] = { sceneSet, baseMapSet };

        pipe->Bind(cmd);
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 2, sets,
            0, nullptr);

        SurfacePayload sp{};
        if (it.payloadIndex != RenderItem::kInvalidPayload)
        {
            sp = GetSurfacePayload(it.payloadIndex);
        }

        VKSurfacePC pc{};
        StoreMat4(pc.world, it.world);

        // tint / opacity
        pc.tintOpacity[0] = sp.tint.x;
        pc.tintOpacity[1] = sp.tint.y;
        pc.tintOpacity[2] = sp.tint.z;
        pc.tintOpacity[3] = sp.opacity;

        // params0: flipX, flipY, mode, scanlineStrength
        pc.params0[0] = sp.flipX ? 1.0f : 0.0f;
        pc.params0[1] = sp.flipY ? 1.0f : 0.0f;
        pc.params0[2] = static_cast<float>(sp.mode);
        pc.params0[3] = sp.scanlineStrength;

        // params1: time, distortStrength, fresnel, fresnelPow
        pc.params1[0] = sp.time;
        pc.params1[1] = 0.02f; // distortStrength
        pc.params1[2] = 0.35f; // fresnel
        pc.params1[3] = 2.0f;  // fresnelPow

        // params2: waveSpeed, swayStrength, sparkleStrength, reserved
        pc.params2[0] = 1.0f;  // waveSpeed
        pc.params2[1] = 0.0f;  // swayStrength
        pc.params2[2] = 0.0f;  // sparkleStrength

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(VKSurfacePC),
            &pc);

        if (it.indexCount > 0)
            vkCmdDrawIndexed(cmd, it.indexCount, 1, 0, 0, 0);
        else if (it.vertexCount > 0)
            vkCmdDraw(cmd, it.vertexCount, 1, 0, 0);

        AddDrawCall();
        return;
    }
    
    //----------------------------------------------------------
    // Particle
    //----------------------------------------------------------
    if (it.type == RenderItemType::Particle)
    {
        if (sceneSet == VK_NULL_HANDLE)
        {
            return;
        }

        VkDescriptorSet baseMapSet =
            GetOrCreateBaseMapSet(it.texture.ptr, pipelineName);
        if (baseMapSet == VK_NULL_HANDLE)
        {
            return;
        }

        VKPipeline* pipe = mPipelines.Get(pipelineName);
        if (!pipe || !pipe->IsValid())
        {
            return;
        }

        if (it.payloadIndex == RenderItem::kInvalidPayload)
        {
            return;
        }

        if (it.gpuInstanceVB == VK_NULL_HANDLE)
        {
            return;
        }

        // ★ Particle だけは generic bind を使わず、ここで binding0/1 を両方 bind する
        const VertexArray* va = it.geometry.ptr;
        if (!va)
        {
            return;
        }

        auto* backend = (VKVertexArrayBackend*)va->GetBackend();
        if (!backend)
        {
            return;
        }

        VkBuffer quadVB = (VkBuffer)backend->GetVKVertexBuffer();
        VkBuffer quadIB = (VkBuffer)backend->GetVKIndexBuffer();
        if (quadVB == VK_NULL_HANDLE || quadIB == VK_NULL_HANDLE)
        {
            return;
        }

        const ParticlePayload& pp = GetParticlePayload(it.payloadIndex);

        VkDescriptorSet sets[2] =
        {
            sceneSet,
            baseMapSet
        };

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->GetPipelineLayout(),
            0, 2, sets,
            0, nullptr);

        struct VKParticlePC
        {
            float cameraRight[4];
            float cameraUp[4];
            float params[4]; // x=size, y=lifeMax
        };

        VKParticlePC pc{};
        pc.cameraRight[0] = pp.cameraRight.x;
        pc.cameraRight[1] = pp.cameraRight.y;
        pc.cameraRight[2] = pp.cameraRight.z;
        pc.cameraRight[3] = 0.0f;

        pc.cameraUp[0] = pp.cameraUp.x;
        pc.cameraUp[1] = pp.cameraUp.y;
        pc.cameraUp[2] = pp.cameraUp.z;
        pc.cameraUp[3] = 0.0f;

        pc.params[0] = pp.particleSize;
        pc.params[1] = pp.particleLifeMax;
        pc.params[2] = 0.0f;
        pc.params[3] = 0.0f;

        vkCmdPushConstants(
            cmd,
            pipe->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(VKParticlePC),
            &pc);

        VkBuffer bufs[2] =
        {
            quadVB,
            it.gpuInstanceVB
        };
        VkDeviceSize offs[2] = { 0, 0 };

        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
        vkCmdBindIndexBuffer(cmd, quadIB, 0, (VkIndexType)backend->GetVKIndexType());

        vkCmdDrawIndexed(
            cmd,
            it.indexCount,
            static_cast<uint32_t>(it.instanceCount),
            0,
            0,
            0);

        AddDrawCall();
        return;
    }
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    PipelineHandle h{};
    h.ptrVKPipeline = mPipelines.Get(name.c_str());
    return h;
}

//------------------------------------------------------------------------------
// UI bucket draw
//------------------------------------------------------------------------------
void VKRenderer::DrawBucket_UI(const std::vector<uint32_t>& bucket)
{
    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;

        const RenderItem& it = items[idx];

        if (it.pass != RenderPass::UI && it.layer != VisualLayer::UI)
        {
            continue;
        }

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem(it, RenderPass::UI, -1);
    }
}

//------------------------------------------------------------------------------
// Sky bucket draw
//------------------------------------------------------------------------------
void VKRenderer::DrawBucket_Sky(const std::vector<uint32_t>& bucket)
{
    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue;
        }

        const RenderItem& it = items[idx];

        if (it.type != RenderItemType::SkyDome)
        {
            continue;
        }

        DrawItem(it, RenderPass::World, -1);
    }
}

//------------------------------------------------------------------------------
// OverlayScreen bucket draw
//------------------------------------------------------------------------------
void VKRenderer::DrawBucket_OverlayScreen(const std::vector<uint32_t>& bucket)
{
    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue;
        }

        const RenderItem& it = items[idx];

        if (it.type != RenderItemType::Overlay)
        {
            continue;
        }

        DrawItem(it, RenderPass::World, -1);
    }
}

VkDescriptorSet VKRenderer::GetOrCreateEmptySet(const char* pipelineName, uint32_t setIndex)
{
    if (!pipelineName) return VK_NULL_HANDLE;
    if (mDevice == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    if (mDescPool == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    VKPipeline* pipe = mPipelines.Get(pipelineName);
    if (!pipe || !pipe->IsValid()) return VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = pipe->GetSetLayout(setIndex);
    if (layout == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    EmptySetKey key{};
    key.frame = mFrameIndex;
    key.setIndex = setIndex;
    key.pipelineName = pipelineName;

    auto it = mEmptySetCache.find(key);
    if (it != mEmptySetCache.end())
    {
        return it->second;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &layout;

    VkDescriptorSet out = VK_NULL_HANDLE;
    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &out);
    if (vr != VK_SUCCESS || out == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateEmptySet: vkAllocateDescriptorSets failed: "
                  << vr << " pipe=" << pipelineName << " set=" << setIndex << "\n";
        return VK_NULL_HANDLE;
    }

    mEmptySetCache.emplace(std::move(key), out);
    return out;
}

void VKRenderer::ClearEmptySetCache()
{
    mEmptySetCache.clear();
}

} // namespace toy
