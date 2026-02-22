//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO / SceneSet / BaseMapSet cache / SkinnedUBO
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/Pipeline/VKPipeline.h"
#include "Asset/Material/VKTextureGPU.h"
#include "Asset/Material/ITextureGPU.h"
#include "Render/LightingManager.h"

#include <iostream>
#include <cstring>

namespace toy
{

static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        const bool typeOk = (typeBits & (1u << i)) != 0;
        const bool propOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

//--------------------------------------------------------------
// “基準Pipeline” から setLayout を取る（set互換を前提に運用）
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
    constexpr uint32_t kMaxSkinnedSets   = 256;

    VkDescriptorPoolSize sizes[3]{};

    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kMaxSceneSets + kMaxSkinnedSets;

    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kMaxBaseMapSets;

    sizes[2].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[2].descriptorCount = kMaxSkinnedSets;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSceneSets + kMaxBaseMapSets + kMaxSkinnedSets;
    ci.poolSizeCount = 3;
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

static void FreeDescriptorSetIfPossible(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set)
{
    if (!device || !pool || !set) return;
    vkFreeDescriptorSets(device, pool, 1, &set);
}

void VKRenderer::DestroyDescriptorPool()
{
    if (!mDevice)
    {
        return;
    }

    if (mSceneSet != VK_NULL_HANDLE)
    {
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mSceneSet);
        mSceneSet = VK_NULL_HANDLE;
    }

    ClearBaseMapSetCache();

    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
}

//==============================================================
// Scene UBO (Mesh/Skinned 用に拡張)
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
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;

    std::memcpy(ubo.viewProj, &viewProj, sizeof(float) * 16);

    // ここは “まず表示” のために最低限埋める
    // あなたの Renderer が既に持ってる値に置き換えてOK
    Vector3 cameraPos = GetCameraPosition();
    ubo.cameraPos[0] = cameraPos.x;
    ubo.cameraPos[1] = cameraPos.y;
    ubo.cameraPos[2] = cameraPos.z;
    ubo.cameraPos[3] = 1.0f;

    Vector3 ambient = GetLightingManager()->GetAmbientColor();
    ubo.ambient[0] = ambient.x;
    ubo.ambient[1] = ambient.y;
    ubo.ambient[2] = ambient.z;
    ubo.ambient[3] = 1.0f;

    DirectionalLight dirLight = GetLightingManager()->GetDirectionalLight();
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

//==============================================================
// Scene UBO update (override)
//  - viewProjOverride は「Row-vector v*M」前提で、
//    シェーダ側の掛け算順（worldPos * viewProj）に合わせて
//    そのまま渡す。
//==============================================================
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
//==============================================================
bool VKRenderer::CreateSceneDescriptorSet()
{
    if (!mDevice || !mDescPool)
    {
        return false;
    }
    if (mSceneSet != VK_NULL_HANDLE)
    {
        return true;
    }
    if (mSceneUBO == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: SceneUBO not created\n";
        return false;
    }

    // set互換の基準は Mesh
    VkDescriptorSetLayout set0 = GetPipelineSetLayout(mPipelines, "Mesh", 0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: Mesh set0 layout is null\n";
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
        mSceneSet = VK_NULL_HANDLE;
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
            VkDescriptorSet ds = kv.second;
            if (ds != VK_NULL_HANDLE)
            {
                FreeDescriptorSetIfPossible(mDevice, mDescPool, ds);
            }
        }
    }
    mBaseMapSetCache.clear();
}

//==============================================================
// Backward compatible: Sprite cache API
//  - 旧コードが残っていてもビルドできるように維持
//  - 実体は BaseMap cache に寄せる
//==============================================================
void VKRenderer::ClearSpriteTextureSetCache()
{
    // 互換：古い map も一応クリア（ただし実際の DS は BaseMap 側で管理）
    mSpriteTexSetCache.clear();
    ClearBaseMapSetCache();
}

VkDescriptorSet VKRenderer::GetOrCreateSpriteTextureSet(const Texture* tex)
{
    // Sprite pipeline の set=1 を使う
    return GetOrCreateBaseMapSet(tex, "Sprite");
}

VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    if (!tex || !mDevice || !mDescPool)
    {
        return VK_NULL_HANDLE;
    }

    auto it = mBaseMapSetCache.find(tex);
    if (it != mBaseMapSetCache.end())
    {
        return it->second;
    }

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] BaseMap set1 layout is null\n";
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
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(BaseMapSet) failed: " << vr << "\n";
        return VK_NULL_HANDLE;
    }

    auto fail = [&](const char* msg) -> VkDescriptorSet
    {
        std::cerr << msg << "\n";
        FreeDescriptorSetIfPossible(mDevice, mDescPool, ds);
        ds = VK_NULL_HANDLE;
        return VK_NULL_HANDLE;
    };

    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        return fail("[VKRenderer] texture GPU is null");
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        return fail("[VKRenderer] texture GPU is not VKTextureGPU");
    }

    if (vkgpu->GetSampler() == VK_NULL_HANDLE || vkgpu->GetImageView() == VK_NULL_HANDLE)
    {
        return fail("[VKRenderer] VKTextureGPU sampler/view is null");
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

    mBaseMapSetCache[tex] = ds;
    return ds;
}

//==============================================================
// Host-visible buffer helpers（あなたの既存のまま）
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
        FindMemoryType(mPhysicalDevice,
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
