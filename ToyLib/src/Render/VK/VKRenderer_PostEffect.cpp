//======================================================================
// Render/VK/VKRenderer_PostEffect.cpp
//
// PostEffect
//  - fullscreen quad に scene texture をかける
//  - optional で paper texture を使う
//  - descriptor set は PostEffect 専用に扱う
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Asset/Material/Texture.h"
#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/VKTextureGPU.h"

#include <SDL3/SDL.h>
#include <iostream>
#include <cstring>

namespace toy
{

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
// PostEffect PushConstants
//--------------------------------------------------------------
struct VKPostEffectPC
{
    float params0[4]; // x=postType, y=intensity, z=time, w=flipY
    float params1[4]; // x=usePaperTex, y/z/w=reserved
};

//--------------------------------------------------------------
// GetOrCreatePostEffectSet
//--------------------------------------------------------------
VkDescriptorSet VKRenderer::GetOrCreatePostEffectSet(const Texture* sceneTex,
                                                     const Texture* paperTex)
{
    if (!mDevice || !sceneTex)
    {
        return VK_NULL_HANDLE;
    }

    VKPipeline* pipe = mPipelines.Get("PostEffect");
    if (!pipe || !pipe->IsValid())
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: PostEffect pipeline missing\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout layout = pipe->GetSetLayout(0);
    if (layout == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: set0 layout null\n";
        return VK_NULL_HANDLE;
    }

    PostEffectSetKey key{};
    key.frame    = mFrameIndex;
    key.sceneTex = sceneTex;
    key.paperTex = paperTex;

    auto it = mPostEffectSetCache.find(key);
    if (it != mPostEffectSetCache.end())
    {
        return it->second;
    }

    auto* sceneGPU = dynamic_cast<const VKTextureGPU*>(sceneTex->GetGPU());
    if (!sceneGPU)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: sceneTex is not VKTextureGPU\n";
        return VK_NULL_HANDLE;
    }
    
    const Texture* paperTexResolved = paperTex ? paperTex : sceneTex;
    auto* paperGPU = dynamic_cast<const VKTextureGPU*>(paperTexResolved->GetGPU());
    if (!paperGPU)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: paperTex is not VKTextureGPU\n";
        return VK_NULL_HANDLE;
    }

    if (sceneGPU->GetSampler() == VK_NULL_HANDLE || sceneGPU->GetImageView() == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: sceneTex sampler/view null\n";
        return VK_NULL_HANDLE;
    }

    if (paperGPU->GetSampler() == VK_NULL_HANDLE || paperGPU->GetImageView() == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: paperTex sampler/view null\n";
        return VK_NULL_HANDLE;
    }

    if (mDescPool == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: mDescPool null\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &layout;

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &ds);
    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreatePostEffectSet: alloc failed vr=" << vr << "\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorImageInfo sceneII{};
    sceneII.sampler     = sceneGPU->GetSampler();
    sceneII.imageView   = sceneGPU->GetImageView();
    sceneII.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo paperII{};
    paperII.sampler     = paperGPU->GetSampler();
    paperII.imageView   = paperGPU->GetImageView();
    paperII.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = ds;
    writes[0].dstBinding      = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo      = &sceneII;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = ds;
    writes[1].dstBinding      = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &paperII;

    vkUpdateDescriptorSets(mDevice, 2, writes, 0, nullptr);

    mPostEffectSetCache.emplace(key, ds);
    return ds;
}

//--------------------------------------------------------------
// ClearPostEffectSetCache
//--------------------------------------------------------------
void VKRenderer::ClearPostEffectSetCache()
{
    mPostEffectSetCache.clear();
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

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    BeginSwapchainRenderPassIfNeeded();
    if (!mIsInRenderPass)
    {
        return;
    }

    GeometryHandle gh{};
    gh.ptr = mFullScreenQuad.get();

    if (!BindVertexArrayVK(cmd, gh))
    {
        return;
    }

    VKPipeline* pipe = mPipelines.Get("PostEffect");
    if (!pipe || !pipe->IsValid())
    {
        std::cerr << "[VKRenderer] DrawPostEffectPass: PostEffect pipeline missing\n";
        return;
    }

    VkDescriptorSet postSet =
        GetOrCreatePostEffectSet(sceneTex.get(), mPost.paperTex.get());
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
    pc.params0[3] = 0.0f; // flipY

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
