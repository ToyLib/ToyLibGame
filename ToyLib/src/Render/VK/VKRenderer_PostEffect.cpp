//======================================================================
// Render/VK/VKRenderer_PostEffect.cpp
//
// PostEffect
//  - SceneRT の color texture を fullscreen quad に描く
//  - descriptor set は per-frame 固定
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPushConstants.h"

#include "Render/VK/Pipeline/VKPipeline.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Asset/Material/Texture.h"
#include "Asset/Material/VKTextureGPU.h"
#include "Render/VK/VKSceneRenderTarget.h"

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
    mPostEffectSets.resize(frameCount, VK_NULL_HANDLE);

    std::vector<VkDescriptorSetLayout> layouts(frameCount, mPostEffectSetLayout);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = frameCount;
    ai.pSetLayouts        = layouts.data();

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
//--------------------------------------------------------------
void VKRenderer::UpdatePostEffectDescriptorSet(uint32_t frameIndex,
                                               const Texture* sceneTex,
                                               const Texture* paperTex)
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (frameIndex >= mPostEffectSets.size())
    {
        return;
    }

    if (!sceneTex)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: sceneTex null\n";
        return;
    }

    
    const Texture* paperTexResolved = paperTex ? paperTex : sceneTex;

    const auto* sceneGPU = dynamic_cast<const VKTextureGPU*>(sceneTex->GetGPU());
    const auto* paperGPU = dynamic_cast<const VKTextureGPU*>(paperTexResolved->GetGPU());

    if (!sceneGPU)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: sceneGPU null\n";
        return;
    }

    if (!paperGPU)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: paperGPU null\n";
        return;
    }

    const VkSampler   sceneSampler = sceneGPU->GetSampler();
    const VkImageView sceneView    = sceneGPU->GetImageView();

    const VkSampler   paperSampler = paperGPU->GetSampler();
    const VkImageView paperView    = paperGPU->GetImageView();

    if (sceneSampler == VK_NULL_HANDLE || sceneView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: scene sampler/view null\n";
        return;
    }

    if (paperSampler == VK_NULL_HANDLE || paperView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: paper sampler/view null\n";
        return;
    }

    const VkDescriptorSet ds = mPostEffectSets[frameIndex];
    if (ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] UpdatePostEffectDescriptorSet: descriptor set null\n";
        return;
    }

    // sampler2D 用なので SHADER_READ_ONLY_OPTIMAL
    VkDescriptorImageInfo sceneII{};
    sceneII.sampler     = sceneSampler;
    sceneII.imageView   = sceneView;
    sceneII.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo paperII{};
    paperII.sampler     = paperSampler;
    paperII.imageView   = paperView;
    paperII.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = ds;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo      = &sceneII;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = ds;
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo      = &paperII;

    vkUpdateDescriptorSets(mDevice, 2, writes, 0, nullptr);
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

    if (mFrameIndex >= mPostEffectSets.size())
    {
        return;
    }

    if (mFrameIndex >= mPostEffectSets.size())
    {
        std::cerr << "[PostEffect] invalid frame index\n";
        return;
    }
    
    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE)
    {
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

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    GeometryHandle gh{};
    gh.ptr = mFullScreenQuad.get();

    if (!BindVertexArrayVK_Post(cmd, gh))
    {
        return;
    }

    VKPipeline* pipe = mPipelines.Get("PostEffect");
    if (!pipe || !pipe->IsValid())
    {
        std::cerr << "[VKRenderer] DrawPostEffectPass: PostEffect pipeline missing\n";
        return;
    }

    UpdatePostEffectDescriptorSet(mFrameIndex, sceneTex.get(), mPost.paperTex.get());

    VkDescriptorSet postSet = mPostEffectSets[mFrameIndex];
    if (postSet == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] DrawPostEffectPass: postSet null\n";
        return;
    }

    pipe->Bind(cmd);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipe->GetPipelineLayout(),
        0, 1, &postSet,
        0, nullptr);

    VKPostEffectPC pc{};
    pc.params0[0] = static_cast<float>(mPost.type);
    pc.params0[1] = mPost.intensity;
    pc.params0[2] = static_cast<float>(SDL_GetTicks()) * 0.001f;
    pc.params0[3] = 1.0f; // flipY (VK SceneRT -> fullscreen sample)

    pc.params1[0] = (mPost.paperTex != nullptr) ? 1.0f : 0.0f;
    pc.params1[1] = 0.0f;
    pc.params1[2] = 0.0f;
    pc.params1[3] = 0.0f;

    vkCmdPushConstants(
        cmd,
        pipe->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(VKPostEffectPC),
        &pc);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    AddDrawCall();
}

} // namespace toy
