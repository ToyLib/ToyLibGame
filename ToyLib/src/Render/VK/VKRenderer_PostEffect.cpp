//======================================================================
// Render/VK/VKRenderer_PostEffect.cpp
//
// PostEffect
//  - SceneRT の color texture を fullscreen quad に描く
//  - 最大2段のチェーン: stage0を中間RT(mPostMidRT)に焼き、
//    stage1がそれをサンプルしてswapchainに描く
//  - descriptor set は per-frame × per-stage 固定
//======================================================================

#include "Render/VK/VKPushConstants.h"
#include "Render/VK/VKRenderer.h"

#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/Pipeline/VKPipelinePresets.h"
#include "Render/VK/VKPostMidTarget.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/VKTextureGPU.h"

#include <SDL3/SDL.h>
#include <iostream>

namespace toy
{

//--------------------------------------------------------------
// VertexArray bind helper
//--------------------------------------------------------------
static bool BindVertexArrayVK_Post(VkCommandBuffer cmd, const GeometryHandle& gh)
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

    auto* backend = static_cast<const VKVertexArrayBackend*>(va->GetBackend());
    if (!backend)
    {
        return false;
    }

    VkBuffer vb = static_cast<VkBuffer>(backend->GetVKVertexBuffer());
    if (vb == VK_NULL_HANDLE)
    {
        return false;
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);

    VkBuffer ib = static_cast<VkBuffer>(backend->GetVKIndexBuffer());
    if (ib != VK_NULL_HANDLE)
    {
        vkCmdBindIndexBuffer(cmd, ib, 0, static_cast<VkIndexType>(backend->GetVKIndexType()));
    }

    return true;
}

//--------------------------------------------------------------
// CreatePostEffectDescriptorSets
//  - frameCount * kPostStageSlots 分をまとめて確保する
//--------------------------------------------------------------
bool VKRenderer::CreatePostEffectDescriptorSets()
{
    mPostEffectSets.clear();

    if (mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreatePostEffectDescriptorSets: device null\n";
        return false;
    }

    if (mDescPool == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreatePostEffectDescriptorSets: desc pool null\n";
        return false;
    }

    if (mPostEffectSetLayout == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreatePostEffectDescriptorSets: set layout null\n";
        return false;
    }

    if (mFrames.empty())
    {
        std::cerr << "[VKRenderer] CreatePostEffectDescriptorSets: no frames\n";
        return false;
    }

    const uint32_t frameCount = static_cast<uint32_t>(mFrames.size());
    const uint32_t setCount   = frameCount * kPostStageSlots;
    mPostEffectSets.resize(setCount, VK_NULL_HANDLE);

    std::vector<VkDescriptorSetLayout> layouts(setCount, mPostEffectSetLayout);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = mDescPool;
    ai.descriptorSetCount = setCount;
    ai.pSetLayouts = layouts.data();

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, mPostEffectSets.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] CreatePostEffectDescriptorSets failed: " << vr << "\n";
        mPostEffectSets.clear();
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// UpdatePostEffectDescriptorSet
//  - setIndex = frameIndex * kPostStageSlots + stageSlot
//--------------------------------------------------------------
void VKRenderer::UpdatePostEffectDescriptorSet(uint32_t setIndex, const Texture* inputTex)
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (setIndex >= mPostEffectSets.size())
    {
        return;
    }

    if (!inputTex)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: inputTex null\n";
        return;
    }

    const auto* inputGPU = dynamic_cast<const VKTextureGPU*>(inputTex->GetGPU());

    if (!inputGPU)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: inputGPU null\n";
        return;
    }

    const VkSampler inputSampler = inputGPU->GetSampler();
    const VkImageView inputView = inputGPU->GetImageView();

    if (inputSampler == VK_NULL_HANDLE || inputView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: sampler/view null\n";
        return;
    }

    const VkDescriptorSet ds = mPostEffectSets[setIndex];
    if (ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: descriptor set null\n";
        return;
    }

    // sampler2D 用なので SHADER_READ_ONLY_OPTIMAL
    VkDescriptorImageInfo inputII{};
    inputII.sampler = inputSampler;
    inputII.imageView = inputView;
    inputII.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[1]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = ds;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputII;

    vkUpdateDescriptorSets(mDevice, 1, writes, 0, nullptr);
}

//--------------------------------------------------------------
// EnsurePostMidTarget
//  - 2段目の入力となる中間オフスクリーンターゲット(color only)を
//    現在のswapchainサイズに合わせて用意する
//--------------------------------------------------------------
bool VKRenderer::EnsurePostMidTarget()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return false;
    }

    if (mSwapchainExtent.width == 0 || mSwapchainExtent.height == 0)
    {
        return false;
    }

    if (!mPostMidRT)
    {
        mPostMidRT = std::make_shared<VKPostMidTarget>();
    }

    if (!mPostMidRT->Resize(static_cast<int>(mSwapchainExtent.width), static_cast<int>(mSwapchainExtent.height)))
    {
        std::cerr << "[VKRenderer] EnsurePostMidTarget: resize failed\n";
        mPostMidRT.reset();
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// EnsurePostMidPipeline
//  - 中間ターゲット向けのcolor-only render passを使う専用パイプライン
//    ("PostEffect"とシェーダー/descriptor layout/push constantは同一)
//--------------------------------------------------------------
bool VKRenderer::EnsurePostMidPipeline()
{
    if (mPostMidPipelineReady)
    {
        return true;
    }

    if (!EnsurePostMidTarget())
    {
        return false;
    }

    auto* midRT = dynamic_cast<VKPostMidTarget*>(mPostMidRT.get());
    if (!midRT)
    {
        return false;
    }

    const std::string base = mShaderPath + "VK/spv/";
    VKPipelineDesc post = toy::VKPipelinePresets::MakePostEffect(base);

    if (!mPipelines.CreatePipeline("PostEffectMid", mDevice, midRT->GetRenderPass(), midRT->GetExtent(), post))
    {
        std::cerr << "[VKRenderer] EnsurePostMidPipeline: CreatePipeline failed\n";
        return false;
    }

    VKPipeline* pipe = mPipelines.Get("PostEffectMid");
    if (!pipe || !pipe->IsValid())
    {
        std::cerr << "[VKRenderer] EnsurePostMidPipeline: pipeline invalid\n";
        return false;
    }

    mPostMidPipelineReady = true;
    return true;
}

//--------------------------------------------------------------
// DrawPostEffectPass
//--------------------------------------------------------------
void VKRenderer::DrawPostEffectPass()
{
    if (mDevice == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    if (!mSceneRT)
    {
        return;
    }

    auto sceneTex = mSceneRT->GetColorTexture();
    if (!sceneTex)
    {
        return;
    }

    if (!mFullScreenQuad)
    {
        return;
    }

    // 有効なステージだけを順番に並べる(最大2段)
    struct StageRun { PostEffectType type; float intensity; };
    StageRun stages[2];
    int stageCount = 0;
    if (mPost.stage0.type != PostEffectType::None)
    {
        stages[stageCount++] = { mPost.stage0.type, mPost.stage0.intensity };
    }
    if (mPost.stage1.type != PostEffectType::None)
    {
        stages[stageCount++] = { mPost.stage1.type, mPost.stage1.intensity };
    }
    if (stageCount == 0)
    {
        return;
    }

    if (mFrameIndex >= mFrames.size())
    {
        std::cerr << "[PostEffect] invalid frame index\n";
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    // 2段目が要るなら中間RTと専用パイプラインを用意しておく
    if (stageCount > 1 && !EnsurePostMidPipeline())
    {
        std::cerr << "[VKRenderer] DrawPostEffectPass: mid pipeline unavailable\n";
        return;
    }

    // World/Overlay は SceneRT に描いているので閉じる
    if (mIsInRenderPass)
    {
        vkCmdEndRenderPass(cmd);
        mIsInRenderPass = false;
    }

    // 以降は swapchain に描く
    mRenderToSceneRTThisFrame = false;

    VKPipeline* postPipe = mPipelines.Get("PostEffect");
    if (!postPipe || !postPipe->IsValid())
    {
        std::cerr << "[VKRenderer] DrawPostEffectPass: PostEffect pipeline missing\n";
        return;
    }

    GeometryHandle gh{};
    gh.ptr = mFullScreenQuad.get();

    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;

    std::shared_ptr<Texture> input = sceneTex;

    for (int i = 0; i < stageCount; ++i)
    {
        const bool isLast = (i == stageCount - 1);
        const uint32_t setIndex = mFrameIndex * kPostStageSlots + static_cast<uint32_t>(i);

        if (setIndex >= mPostEffectSets.size())
        {
            std::cerr << "[PostEffect] invalid descriptor set index\n";
            return;
        }

        UpdatePostEffectDescriptorSet(setIndex, input.get());

        VkDescriptorSet postSet = mPostEffectSets[setIndex];
        if (postSet == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] DrawPostEffectPass: postSet null\n";
            return;
        }

        VKPipeline* pipe = nullptr;

        if (isLast)
        {
            BeginSwapchainRenderPassIfNeeded();
            if (!mIsInRenderPass)
            {
                return;
            }
            pipe = postPipe;
        }
        else
        {
            auto* midRT = dynamic_cast<VKPostMidTarget*>(mPostMidRT.get());
            if (!midRT)
            {
                return;
            }

            VkClearValue clear{};
            clear.color.float32[0] = 0.0f;
            clear.color.float32[1] = 0.0f;
            clear.color.float32[2] = 0.0f;
            clear.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo rpbi{};
            rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpbi.renderPass = midRT->GetRenderPass();
            rpbi.framebuffer = midRT->GetFramebuffer();
            rpbi.renderArea.offset = { 0, 0 };
            rpbi.renderArea.extent = midRT->GetExtent();
            rpbi.clearValueCount = 1;
            rpbi.pClearValues = &clear;

            vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            mIsInRenderPass = true;

            // このパスは通常の(非反転)ビューポートで焼く。
            // 次段でサンプルするときflip不要にするため。
            VkViewport vp{};
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width  = static_cast<float>(midRT->GetExtent().width);
            vp.height = static_cast<float>(midRT->GetExtent().height);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &vp);

            VkRect2D sc{};
            sc.offset = { 0, 0 };
            sc.extent = midRT->GetExtent();
            vkCmdSetScissor(cmd, 0, 1, &sc);

            pipe = mPipelines.Get("PostEffectMid");
        }

        if (!pipe || !pipe->IsValid())
        {
            std::cerr << "[VKRenderer] DrawPostEffectPass: stage pipeline missing\n";
            return;
        }

        if (!BindVertexArrayVK_Post(cmd, gh))
        {
            return;
        }

        pipe->Bind(cmd);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->GetPipelineLayout(), 0, 1, &postSet, 0,
                                nullptr);

        VKPostEffectPC pc{};
        pc.params0[0] = static_cast<float>(stages[i].type);
        pc.params0[1] = stages[i].intensity;
        pc.params0[2] = timeSec;
        // flipY: mSceneRTを直接サンプルする最初のステージだけ補正が要る。
        // 中間RTは非反転ビューポートで焼いているので、それを読む後段はflip不要。
        pc.params0[3] = (i == 0) ? 1.0f : 0.0f;

        vkCmdPushConstants(cmd, pipe->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VKPostEffectPC), &pc);

        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

        AddDrawCall();

        if (!isLast)
        {
            vkCmdEndRenderPass(cmd);
            mIsInRenderPass = false;
            input = mPostMidRT->GetColorTexture();
        }
    }
}

} // namespace toy
