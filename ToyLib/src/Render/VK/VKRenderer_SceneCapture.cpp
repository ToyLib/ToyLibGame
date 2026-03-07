//======================================================================
// Render/VK/VKRenderer_SceneCapture.cpp
//
// SceneCapture 用描画
//  - capture ごとの SceneUBO / SceneSet slot を使う
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/VKShaderTypes.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Graphics/Light/PointLightComponent.h"
#include "Render/LightingManager.h"

#include <iostream>
#include <cstring>
#include <algorithm>

namespace toy
{

static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}


//==============================================================
// CreateSceneUBO_Capture
//==============================================================
bool VKRenderer::CreateSceneUBO_Capture()
{
    DestroySceneUBO_Capture();

    if (mDevice == VK_NULL_HANDLE) return false;
    if (mDescPool == VK_NULL_HANDLE) return false;
    if (mFrames.empty()) return false;

    VKPipeline* meshPipe = mPipelines.Get("Mesh");
    if (!meshPipe || !meshPipe->IsValid())
    {
        std::cerr << "[VKRenderer] CreateSceneUBO_Capture: Mesh pipeline not found.\n";
        return false;
    }

    VkDescriptorSetLayout set0 = meshPipe->GetSetLayout(0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSceneUBO_Capture: set0 layout is null.\n";
        return false;
    }

    mSceneUBO_Capture.resize(mFrames.size());
    mSceneUBOMem_Capture.resize(mFrames.size());
    mSceneSet_Capture.resize(mFrames.size());

    for (size_t fi = 0; fi < mFrames.size(); ++fi)
    {
        mSceneUBO_Capture[fi].resize(kMaxSceneCaptureSlots, VK_NULL_HANDLE);
        mSceneUBOMem_Capture[fi].resize(kMaxSceneCaptureSlots, VK_NULL_HANDLE);
        mSceneSet_Capture[fi].resize(kMaxSceneCaptureSlots, VK_NULL_HANDLE);

        for (uint32_t si = 0; si < kMaxSceneCaptureSlots; ++si)
        {
            if (!CreateBufferHostVisible(
                    (VkDeviceSize)mSceneUBOSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    mSceneUBO_Capture[fi][si],
                    mSceneUBOMem_Capture[fi][si]))
            {
                std::cerr << "[VKRenderer] CreateSceneUBO_Capture: buffer failed. frame="
                          << fi << " slot=" << si << "\n";
                DestroySceneUBO_Capture();
                return false;
            }

            VkDescriptorSetAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool = mDescPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &set0;

            VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet_Capture[fi][si]);
            if (vr != VK_SUCCESS || mSceneSet_Capture[fi][si] == VK_NULL_HANDLE)
            {
                std::cerr << "[VKRenderer] CreateSceneUBO_Capture: alloc failed. frame="
                          << fi << " slot=" << si << " vr=" << vr << "\n";
                DestroySceneUBO_Capture();
                return false;
            }

            VkDescriptorBufferInfo bi{};
            bi.buffer = mSceneUBO_Capture[fi][si];
            bi.offset = 0;
            bi.range  = (VkDeviceSize)mSceneUBOSize;

            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = mSceneSet_Capture[fi][si];
            w.dstBinding = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo = &bi;

            vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
        }
    }

    return true;
}

//==============================================================
// DestroySceneUBO_Capture
//==============================================================
void VKRenderer::DestroySceneUBO_Capture()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        for (size_t fi = 0; fi < mSceneUBO_Capture.size(); ++fi)
        {
            for (size_t si = 0; si < mSceneUBO_Capture[fi].size(); ++si)
            {
                if (mSceneUBO_Capture[fi][si] != VK_NULL_HANDLE)
                {
                    vkDestroyBuffer(mDevice, mSceneUBO_Capture[fi][si], nullptr);
                    mSceneUBO_Capture[fi][si] = VK_NULL_HANDLE;
                }

                if (mSceneUBOMem_Capture[fi][si] != VK_NULL_HANDLE)
                {
                    vkFreeMemory(mDevice, mSceneUBOMem_Capture[fi][si], nullptr);
                    mSceneUBOMem_Capture[fi][si] = VK_NULL_HANDLE;
                }
            }
        }
    }

    mSceneUBO_Capture.clear();
    mSceneUBOMem_Capture.clear();
    mSceneSet_Capture.clear();

    mCaptureSlotCursor = 0;
    mActiveCaptureSlot = -1;
}

//==============================================================
// UpdateSceneUBO_Capture
//==============================================================
void VKRenderer::UpdateSceneUBO_Capture(const Matrix4& viewProj)
{
    if (mActiveCaptureSlot < 0) return;
    if (mFrameIndex >= mSceneUBOMem_Capture.size()) return;
    if ((size_t)mActiveCaptureSlot >= mSceneUBOMem_Capture[mFrameIndex].size()) return;

    VKSceneUBO ubo{};

    std::memcpy(ubo.viewProj, &viewProj, sizeof(float) * 16);

    Vector3 cameraPos = GetCameraPosition();
    ubo.cameraPos[0] = cameraPos.x;
    ubo.cameraPos[1] = cameraPos.y;
    ubo.cameraPos[2] = cameraPos.z;
    ubo.cameraPos[3] = 1.0f;

    Vector3 ambient(0.2f, 0.2f, 0.2f);
    DirectionalLight dirLight{};

    if (auto lm = GetLightingManager())
    {
        ambient  = lm->GetAmbientColor();
        dirLight = lm->GetDirectionalLight();
    }

    ubo.ambient[0] = ambient.x;
    ubo.ambient[1] = ambient.y;
    ubo.ambient[2] = ambient.z;
    ubo.ambient[3] = 1.0f;

    ubo.dirDir[0] = dirLight.GetDirection().x;
    ubo.dirDir[1] = dirLight.GetDirection().y;
    ubo.dirDir[2] = dirLight.GetDirection().z;
    ubo.dirDir[3] = 0.0f;

    const Vector3 dd = dirLight.GetDiffuseColor();
    const Vector3 ds = dirLight.GetSpecularColor();

    ubo.dirDiffuse[0] = dd.x;
    ubo.dirDiffuse[1] = dd.y;
    ubo.dirDiffuse[2] = dd.z;
    ubo.dirDiffuse[3] = 1.0f;

    ubo.dirSpecular[0] = ds.x;
    ubo.dirSpecular[1] = ds.y;
    ubo.dirSpecular[2] = ds.z;
    ubo.dirSpecular[3] = 1.0f;

    ubo.numPointLights = 0;

    if (auto lm = GetLightingManager())
    {
        const auto& pls = lm->GetPointLights();

        const int count = (int)std::min<size_t>(pls.size(), 8);
        ubo.numPointLights = count;

        for (int i = 0; i < count; ++i)
        {
            const auto* pl = pls[i];
            if (!pl) continue;

            const Vector3 pos   = pl->GetPosition();
            const Vector3 color = pl->GetColor();
            const float   inten = pl->GetIntensity();
            const float   c     = pl->GetConstant();
            const float   l     = pl->GetLinear();
            const float   q     = pl->GetQuadratic();
            const float   r     = pl->GetRadius();

            ubo.pointLights[i].position_radius[0] = pos.x;
            ubo.pointLights[i].position_radius[1] = pos.y;
            ubo.pointLights[i].position_radius[2] = pos.z;
            ubo.pointLights[i].position_radius[3] = r;

            ubo.pointLights[i].color_intensity[0] = color.x;
            ubo.pointLights[i].color_intensity[1] = color.y;
            ubo.pointLights[i].color_intensity[2] = color.z;
            ubo.pointLights[i].color_intensity[3] = inten;

            ubo.pointLights[i].atten[0] = c;
            ubo.pointLights[i].atten[1] = l;
            ubo.pointLights[i].atten[2] = q;
            ubo.pointLights[i].atten[3] = 0.0f;
        }
    }

    Vector3 fogColor(0.5f, 0.6f, 0.7f);
    float fogMin = 50.0f;
    float fogMax = 200.0f;

    if (auto lm = GetLightingManager())
    {
        fogColor = lm->GetFogColor();
        fogMin   = lm->GetFogMinDist();
        fogMax   = lm->GetFogMaxDist();
    }

    ubo.fogColor[0] = fogColor.x;
    ubo.fogColor[1] = fogColor.y;
    ubo.fogColor[2] = fogColor.z;
    ubo.fogColor[3] = 1.0f;

    ubo.fogParams[0] = fogMin;
    ubo.fogParams[1] = fogMax;
    ubo.fogParams[2] = 0.0f;
    ubo.fogParams[3] = 0.0f;

    if ((int)mShadowCascades.size() == 2)
    {
        StoreMat4(ubo.shadowVP0, mShadowCascades[0].lightVP);
        StoreMat4(ubo.shadowVP1, mShadowCascades[1].lightVP);

        ubo.shadowParams[0] = GetCascadeSplit0();
        ubo.shadowParams[1] = GetCascadeBlend();
        ubo.shadowParams[2] = 1.0f;
        ubo.shadowParams[3] = GetShadowBias();
    }
    else
    {
        StoreMat4(ubo.shadowVP0, Matrix4::Identity);
        StoreMat4(ubo.shadowVP1, Matrix4::Identity);
        ubo.shadowParams[0] = 0.0f;
        ubo.shadowParams[1] = 0.0f;
        ubo.shadowParams[2] = 0.0f;
        ubo.shadowParams[3] = 0.0f;
    }

    UploadToBuffer(
        mSceneUBOMem_Capture[mFrameIndex][mActiveCaptureSlot],
        &ubo,
        (VkDeviceSize)mSceneUBOSize);
}

//==============================================================
// DrawToRenderTarget
//==============================================================
void VKRenderer::DrawToRenderTarget(const SceneCaptureRequest& req)
{
    if (!req.rt) return;

    ChangeDebugRTT();

    auto* vkrt = dynamic_cast<VKSceneRenderTarget*>(req.rt.get());
    if (!vkrt) return;

    if (mDevice == VK_NULL_HANDLE || mFrames.empty()) return;

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE) return;

    if (mCaptureSlotCursor >= kMaxSceneCaptureSlots)
    {
        std::cerr << "[VKRenderer] SceneCapture slot exhausted. max="
                  << kMaxSceneCaptureSlots << "\n";
        return;
    }

    const int slot = (int)mCaptureSlotCursor++;
    mActiveCaptureSlot = slot;

    const Matrix4 prevView = mViewMatrix;
    const Matrix4 prevProj = mProjectionMatrix;
    const Matrix4 prevInvV = mInvView;

    auto savedQueue   = mRenderQueue;
    auto savedBuckets = mBuckets;

    mViewMatrix       = req.view;
    mProjectionMatrix = req.proj;
    mInvView          = req.view;
    mInvView.Invert();

    BuildFrameQueues();

    mIsDrawingCapture = true;

    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
    UpdateSceneUBO_Capture(viewProj);

    VkClearValue clears[2]{};
    clears[0].color.float32[0] = 0.0f;
    clears[0].color.float32[1] = 0.0f;
    clears[0].color.float32[2] = 0.0f;
    clears[0].color.float32[3] = 1.0f;
    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = vkrt->GetRenderPass();
    rp.framebuffer = vkrt->GetFramebuffer();
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = vkrt->GetExtent();
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vpstate{};
    vpstate.x = 0.0f;
    vpstate.y = (float)rp.renderArea.extent.height;
    vpstate.width  = (float)rp.renderArea.extent.width;
    vpstate.height = -(float)rp.renderArea.extent.height;
    vpstate.minDepth = 0.0f;
    vpstate.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vpstate);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = rp.renderArea.extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    const auto& items = mRenderQueue.Items();

    auto drawBucket = [&](const std::vector<uint32_t>& bucket)
    {
        for (uint32_t idx : bucket)
        {
            if (idx >= items.size()) continue;

            const RenderItem& it = items[idx];

            // UI 混入 safety
            if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
            {
                continue;
            }

            // SceneCapture 内で危ないものを除外
            switch (it.type)
            {
                case RenderItemType::SkyDome:
                case RenderItemType::Mesh:
                case RenderItemType::SkinnedMesh:
                case RenderItemType::UnlitQuad:
                case RenderItemType::Particle:
                case RenderItemType::Surface:
                    DrawItem(it, RenderPass::World, -1);
                    break;

                // Debug は SceneCapture には不要。
                // RT 用 pipeline/renderpass 互換の問題も避けるため除外する。
                case RenderItemType::Overlay:
                case RenderItemType::Sprite:
                case RenderItemType::Debug:
                default:
                    break;
            }
        }
    };

    if (req.drawSky)
    {
        drawBucket(mBuckets.sky);
    }

    if (req.drawWorld)
    {
        drawBucket(mBuckets.worldOpaque);
        drawBucket(mBuckets.effectPre);
        drawBucket(mBuckets.worldTransparent);
        drawBucket(mBuckets.effectOverlay);
    }

    vkCmdEndRenderPass(cmd);

    mIsDrawingCapture = false;
    mActiveCaptureSlot = -1;

    mViewMatrix       = prevView;
    mProjectionMatrix = prevProj;
    mInvView          = prevInvV;

    mRenderQueue = std::move(savedQueue);
    mBuckets     = std::move(savedBuckets);

    UpdateSceneUBO_World();

    ChangeDebugOnScreen();
}
} // namespace toy
