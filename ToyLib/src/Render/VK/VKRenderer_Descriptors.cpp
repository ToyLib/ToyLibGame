//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO / SceneSet / BaseMapSet cache
//  - + Fallback(1x1 white) texture & set=1
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKUtil.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Asset/Material/VKTextureGPU.h"
#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/Texture.h"
#include "Render/LightingManager.h"

#include <iostream>
#include <cstring>

namespace toy
{

//--------------------------------------------------------------
// “基準Pipeline” から setLayout を取る
//--------------------------------------------------------------
static VkDescriptorSetLayout GetPipelineSetLayout(VKPipelineLibrary& lib,
                                                  const char* pipelineName,
                                                  uint32_t setIndex)
{
    auto* p = lib.Get(pipelineName);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }
    return p->GetSetLayout(setIndex);
}

static void FreeDescriptorSetIfPossible(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set)
{
    if (!device || !pool || !set) return;
    vkFreeDescriptorSets(device, pool, 1, &set);
}

//==============================================================
// DescriptorPool
//==============================================================
bool VKRenderer::CreateDescriptorPool()
{
    if (!mDevice)
    {
        return false;
    }
    if (mDescPool)
    {
        return true;
    }

    constexpr uint32_t kMaxSceneSets     = 8;
    constexpr uint32_t kMaxBaseMapSets   = 1024;

    VkDescriptorPoolSize sizes[2]{};

    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kMaxSceneSets;

    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kMaxBaseMapSets + 4; // +fallback分の余裕

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSceneSets + kMaxBaseMapSets + 4;
    ci.poolSizeCount = 2;
    ci.pPoolSizes    = sizes;

    const VkResult vr = vkCreateDescriptorPool(mDevice, &ci, nullptr, &mDescPool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateDescriptorPool failed: " << vr << "\n";
        mDescPool = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VKRenderer::DestroyDescriptorPool()
{
    if (!mDevice)
    {
        return;
    }

    if (mDescPool != VK_NULL_HANDLE)
    {
        // sets
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mSceneSet);
        mSceneSet = VK_NULL_HANDLE;

        DestroyFallbackBaseMapSet();
        ClearBaseMapSetCache();

        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
    else
    {
        mSceneSet = VK_NULL_HANDLE;
        mBaseMapSetCache.clear();
        mSpriteTexSetCache.clear();
        mFallbackBaseMapSet = VK_NULL_HANDLE;
    }

    // fallback image/sampler are not owned by pool
    DestroyFallbackWhiteTexture();
}

//==============================================================
// Scene UBO
//==============================================================
struct VKSceneUBO
{
    float viewProj[16];     // mat4
    float cameraPos[4];     // vec4
    float ambient[4];       // vec4
    float dirDir[4];        // vec4
    float dirDiffuse[4];    // vec4
    float dirSpecular[4];   // vec4
};

bool VKRenderer::CreateSceneUBO()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }
    if (mSceneUBO != VK_NULL_HANDLE)
    {
        return true;
    }

    mSceneUBOSize = sizeof(VKSceneUBO);

    if (!CreateBufferHostVisible((VkDeviceSize)mSceneUBOSize,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 mSceneUBO,
                                 mSceneUBOMem))
    {
        std::cerr << "[VKRenderer] CreateSceneUBO: CreateBufferHostVisible failed\n";
        DestroySceneUBO();
        return false;
    }

    UpdateSceneUBO();
    return true;
}

void VKRenderer::DestroySceneUBO()
{
    if (!mDevice)
    {
        return;
    }

    if (mSceneUBO != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, mSceneUBO, nullptr);
        mSceneUBO = VK_NULL_HANDLE;
    }
    if (mSceneUBOMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mSceneUBOMem, nullptr);
        mSceneUBOMem = VK_NULL_HANDLE;
    }

    mSceneUBOSize = 0;
}

void VKRenderer::UpdateSceneUBO()
{
    if (!mDevice || mSceneUBOMem == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        return;
    }

    VKSceneUBO ubo{};

    // ToyLib: row-vector v*M 前提 → ViewProj は View*Proj
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
    std::memcpy(ubo.viewProj, &viewProj, sizeof(float) * 16);

    Vector3 cameraPos = GetCameraPosition();
    ubo.cameraPos[0] = cameraPos.x;
    ubo.cameraPos[1] = cameraPos.y;
    ubo.cameraPos[2] = cameraPos.z;
    ubo.cameraPos[3] = 1.0f;

    Vector3 ambient(0.2f, 0.2f, 0.2f);
    DirectionalLight dirLight{};
    dirLight.DiffuseColor = Vector3(1.0f, 1.0f, 1.0f);
    dirLight.SpecColor    = Vector3(1.0f, 1.0f, 1.0f);

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

    ubo.dirDiffuse[0] = dirLight.DiffuseColor.x;
    ubo.dirDiffuse[1] = dirLight.DiffuseColor.y;
    ubo.dirDiffuse[2] = dirLight.DiffuseColor.z;
    ubo.dirDiffuse[3] = 1.0f;

    ubo.dirSpecular[0] = dirLight.SpecColor.x;
    ubo.dirSpecular[1] = dirLight.SpecColor.y;
    ubo.dirSpecular[2] = dirLight.SpecColor.z;
    ubo.dirSpecular[3] = 1.0f;

    (void)UploadToBuffer(mSceneUBOMem, &ubo, (VkDeviceSize)mSceneUBOSize);
}

void VKRenderer::UpdateSceneUBO(const Matrix4& viewProjOverride)
{
    if (!mDevice || mSceneUBOMem == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        return;
    }

    VKSceneUBO ubo{};
    std::memcpy(ubo.viewProj, &viewProjOverride, sizeof(float) * 16);
    (void)UploadToBuffer(mSceneUBOMem, &ubo, (VkDeviceSize)mSceneUBOSize);
}

void VKRenderer::UpdateSceneUBOFromMatrix(const Matrix4& viewProj)
{
    UpdateSceneUBO(viewProj);
}

//==============================================================
// Scene Descriptor Set (set=0 binding=0 UBO)
//  - set0 layout は Sprite/Mesh/Skinned で同一運用
//==============================================================
bool VKRenderer::CreateSceneDescriptorSet()
{
    if (!mDevice || !mDescPool)
    {
        return false;
    }

    if (mSceneUBO == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: SceneUBO not created\n";
        return false;
    }

    // 既存を解放
    if (mSceneSet != VK_NULL_HANDLE)
    {
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mSceneSet);
        mSceneSet = VK_NULL_HANDLE;
    }

    // 基準は Sprite の set=0（Mesh/Skinned と同じ）
    VkDescriptorSetLayout set0 = GetPipelineSetLayout(mPipelines, "Sprite", 0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: Sprite set0 layout is null\n";
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &set0;

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet);
    if (vr != VK_SUCCESS || mSceneSet == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(SceneSet) failed: " << vr << "\n";
        return false;
    }

    VkDescriptorBufferInfo bi{};
    bi.buffer = mSceneUBO;
    bi.offset = 0;
    bi.range  = (VkDeviceSize)mSceneUBOSize;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = mSceneSet;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &bi;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    // fallback set=1 は pipeline layout 依存なのでここで作り直す
    if (!CreateFallbackWhiteTexture())
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: CreateFallbackWhiteTexture failed\n";
        return false;
    }
    if (!CreateFallbackBaseMapSet())
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: CreateFallbackBaseMapSet failed\n";
        return false;
    }

    std::cerr << "[VKRenderer] SceneSet created: " << (void*)mSceneSet
              << " FallbackBaseMapSet=" << (void*)mFallbackBaseMapSet << "\n";

    return true;
}

//==============================================================
// BaseMap set cache (set=1 binding=0 CombinedImageSampler)
//==============================================================
void VKRenderer::ClearBaseMapSetCache()
{
    if (mDevice && mDescPool)
    {
        for (auto& kv : mBaseMapSetCache)
        {
            if (kv.second != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &kv.second);
            }
        }
    }
    mBaseMapSetCache.clear();

    // 旧互換mapも空に
    mSpriteTexSetCache.clear();

    // pipeline recreate に備え、fallback DS は作り直す必要がある
    DestroyFallbackBaseMapSet();
}

void VKRenderer::ClearSpriteTextureSetCache()
{
    mSpriteTexSetCache.clear();
    ClearBaseMapSetCache();
}

VkDescriptorSet VKRenderer::GetOrCreateSpriteTextureSet(const Texture* tex)
{
    return GetOrCreateBaseMapSet(tex, "Sprite");
}

VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    if (!mDevice || !mDescPool || !pipelineName)
    {
        std::cerr << "[VK] BaseMapSet: invalid state dev/pool/name\n";
        return VK_NULL_HANDLE;
    }

    // tex が無いなら fallback を返す（既存メンバー名に合わせる）
    if (!tex)
    {
        std::cerr << "[VK] BaseMapSet: tex is NULL (" << pipelineName << ")\n";
        return mFallbackBaseMapSet; // ★既存
    }

    // 単一キャッシュ（既存メンバー名に合わせる）
    {
        auto it = mBaseMapSetCache.find(tex); // ★既存
        if (it != mBaseMapSetCache.end())
        {
            return it->second;
        }
    }

    // set=1 layout は pipeline から取る（ただし presets 的には互換のはず）
    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: set1 layout NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorSet ds = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &set1;

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &ds);
    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: alloc failed vr=" << vr
                  << " (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto fail = [&](const char* msg) -> VkDescriptorSet
    {
        std::cerr << msg << " (" << pipelineName << ")\n";
        if (ds != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            ds = VK_NULL_HANDLE;
        }
        return VK_NULL_HANDLE;
    };

    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        return fail("[VK] BaseMapSet: tex GPU is NULL");
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        return fail("[VK] BaseMapSet: GPU is not VKTextureGPU");
    }

    if (vkgpu->GetSampler() == VK_NULL_HANDLE || vkgpu->GetImageView() == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: sampler/view NULL sampler=" << (void*)vkgpu->GetSampler()
                  << " view=" << (void*)vkgpu->GetImageView()
                  << " (" << pipelineName << ")\n";
        return fail("[VK] BaseMapSet: sampler/view NULL");
    }

    VkDescriptorImageInfo ii{};
    ii.sampler     = vkgpu->GetSampler();
    ii.imageView   = vkgpu->GetImageView();
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = ds;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    // 単一キャッシュに保存（既存メンバー）
    mBaseMapSetCache[tex] = ds;

    std::cerr << "[VK] BaseMapSet: created ds=" << (void*)ds
              << " tex=" << tex
              << " view=" << (void*)ii.imageView
              << " sampler=" << (void*)ii.sampler
              << " (" << pipelineName << ")\n";

    return ds;
}

//==============================================================
// Fallback White Texture (1x1 RGBA8) : Image/View/Sampler
//==============================================================
bool VKRenderer::CreateFallbackWhiteTexture()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }
    if (mFallbackWhiteImg != VK_NULL_HANDLE &&
        mFallbackWhiteView != VK_NULL_HANDLE &&
        mFallbackWhiteSampler != VK_NULL_HANDLE)
    {
        return true;
    }

    DestroyFallbackWhiteTexture();

    // 1x1 RGBA8 white
    const uint32_t w = 1;
    const uint32_t h = 1;
    const uint32_t pixel = 0xFFFFFFFFu;

    // staging buffer
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (!toy::vkutil::CreateBuffer_HostVisible(
            mPhysicalDevice,
            mDevice,
            sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            staging,
            stagingMem))
    {
        std::cerr << "[VKRenderer] CreateFallbackWhiteTexture: staging buffer create failed\n";
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, stagingMem, 0, sizeof(uint32_t), 0, &mapped) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }
    std::memcpy(mapped, &pixel, sizeof(uint32_t));
    vkUnmapMemory(mDevice, stagingMem);

    // image
    if (!toy::vkutil::CreateImage2D(
            mPhysicalDevice,
            mDevice,
            w,
            h,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mFallbackWhiteImg,
            mFallbackWhiteMem,
            VK_IMAGE_LAYOUT_UNDEFINED))
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        std::cerr << "[VKRenderer] CreateFallbackWhiteTexture: CreateImage2D failed\n";
        return false;
    }

    // record copy & transitions
    VkCommandBuffer cmd = BeginOneTimeCommands();
    if (cmd == VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        DestroyFallbackWhiteTexture();
        return false;
    }

    toy::vkutil::CmdTransitionImageLayout(
        cmd,
        mFallbackWhiteImg,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };

    vkCmdCopyBufferToImage(
        cmd,
        staging,
        mFallbackWhiteImg,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);

    toy::vkutil::CmdTransitionImageLayout(
        cmd,
        mFallbackWhiteImg,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);

    EndOneTimeCommands(cmd);

    // staging cleanup
    vkDestroyBuffer(mDevice, staging, nullptr);
    vkFreeMemory(mDevice, stagingMem, nullptr);

    // view
    mFallbackWhiteView = toy::vkutil::CreateImageView2D(
        mDevice,
        mFallbackWhiteImg,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT);

    if (mFallbackWhiteView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateFallbackWhiteTexture: CreateImageView2D failed\n";
        DestroyFallbackWhiteTexture();
        return false;
    }

    // sampler
    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    sci.maxAnisotropy = 1.0f;

    if (vkCreateSampler(mDevice, &sci, nullptr, &mFallbackWhiteSampler) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] CreateFallbackWhiteTexture: vkCreateSampler failed\n";
        DestroyFallbackWhiteTexture();
        return false;
    }

    return true;
}

void VKRenderer::DestroyFallbackWhiteTexture()
{
    if (!mDevice) return;

    if (mFallbackWhiteSampler)
    {
        vkDestroySampler(mDevice, mFallbackWhiteSampler, nullptr);
        mFallbackWhiteSampler = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteView)
    {
        vkDestroyImageView(mDevice, mFallbackWhiteView, nullptr);
        mFallbackWhiteView = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteImg)
    {
        vkDestroyImage(mDevice, mFallbackWhiteImg, nullptr);
        mFallbackWhiteImg = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteMem)
    {
        vkFreeMemory(mDevice, mFallbackWhiteMem, nullptr);
        mFallbackWhiteMem = VK_NULL_HANDLE;
    }
}

bool VKRenderer::CreateFallbackBaseMapSet()
{
    if (!mDevice || !mDescPool)
    {
        return false;
    }
    if (mFallbackBaseMapSet != VK_NULL_HANDLE)
    {
        return true;
    }
    if (mFallbackWhiteView == VK_NULL_HANDLE || mFallbackWhiteSampler == VK_NULL_HANDLE)
    {
        return false;
    }

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, "Sprite", 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateFallbackBaseMapSet: Sprite set1 layout is null\n";
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &set1;

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &mFallbackBaseMapSet);
    if (vr != VK_SUCCESS || mFallbackBaseMapSet == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(FallbackBaseMapSet) failed: " << vr << "\n";
        mFallbackBaseMapSet = VK_NULL_HANDLE;
        return false;
    }

    VkDescriptorImageInfo ii{};
    ii.sampler     = mFallbackWhiteSampler;
    ii.imageView   = mFallbackWhiteView;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = mFallbackBaseMapSet;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    return true;
}

void VKRenderer::DestroyFallbackBaseMapSet()
{
    if (!mDevice || !mDescPool) { mFallbackBaseMapSet = VK_NULL_HANDLE; return; }

    if (mFallbackBaseMapSet != VK_NULL_HANDLE)
    {
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mFallbackBaseMapSet);
        mFallbackBaseMapSet = VK_NULL_HANDLE;
    }
}

//==============================================================
// Host-visible buffer helpers（既存）
//==============================================================
bool VKRenderer::CreateBufferHostVisible(VkDeviceSize size,
                                         VkBufferUsageFlags usage,
                                         VkBuffer& outBuf,
                                         VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!mDevice || !mPhysicalDevice || size == 0)
    {
        return false;
    }

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = size;
    bci.usage       = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    const uint32_t typeIndex =
        toy::vkutil::FindMemoryType(mPhysicalDevice,
                                    req.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (typeIndex == UINT32_MAX)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(mDevice, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        outMem = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(mDevice, outBuf, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(mDevice, outMem, nullptr);
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        outMem = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool VKRenderer::UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size)
{
    if (!mDevice || mem == VK_NULL_HANDLE || !data || size == 0)
    {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mem, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        return false;
    }

    std::memcpy(mapped, data, (size_t)size);
    vkUnmapMemory(mDevice, mem);
    return true;
}

} // namespace toy
