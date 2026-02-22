//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO(World+UI) / SceneSet(World+UI)
//  - BaseMap set cache (set=1)
//  - Fallback(1x1 white) texture & set=1
//
// 方針（確定）:
//  - SceneUBO は World と UI を分離（mSceneUBO / mSceneUBO_UI）
//  - SceneSet も World と UI を分離（mSceneSet / mSceneSet_UI）
//  - Update は UpdateSceneUBO_World / UpdateSceneUBO_UI のみを使う
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
    if (!device || !pool || !set)
    {
        return;
    }
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

    // per-frame (World + UI) の SceneSet を想定して余裕を持たせる
    // 例: frames=2 なら SceneSets は 4 個程度
    constexpr uint32_t kMaxSceneSets   = 16;
    constexpr uint32_t kMaxBaseMapSets = 2048;

    VkDescriptorPoolSize sizes[2]{};

    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kMaxSceneSets;

    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kMaxBaseMapSets + 8; // fallback等の余裕

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSceneSets + kMaxBaseMapSets + 8;
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
        //------------------------------------------------------
        // Scene descriptor sets（per-frame, World/UI）
        //------------------------------------------------------
        for (auto& set : mSceneSet)
        {
            if (set != VK_NULL_HANDLE)
            {
                FreeDescriptorSetIfPossible(mDevice, mDescPool, set);
                set = VK_NULL_HANDLE;
            }
        }
        mSceneSet.clear();

        for (auto& set : mSceneSet_UI)
        {
            if (set != VK_NULL_HANDLE)
            {
                FreeDescriptorSetIfPossible(mDevice, mDescPool, set);
                set = VK_NULL_HANDLE;
            }
        }
        mSceneSet_UI.clear();

        //------------------------------------------------------
        // BaseMap / Sprite caches
        //------------------------------------------------------
        DestroyFallbackBaseMapSet();
        ClearBaseMapSetCache();

        //------------------------------------------------------
        // pool destroy
        //------------------------------------------------------
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
    else
    {
        mSceneSet.clear();
        mSceneSet_UI.clear();
        mBaseMapSetCache.clear();
        mSpriteTexSetCache.clear();

        mFallbackBaseMapSetByPipe.clear();
        mFallbackBaseMapSet = VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // fallback image/sampler（pool所有ではない）
    //----------------------------------------------------------
    DestroyFallbackWhiteTexture();
}

//==============================================================
// Scene UBO layout (Shader 側 set=0 binding=0)
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

//==============================================================
// Scene UBO (World + UI)
//==============================================================
bool VKRenderer::CreateSceneUBO()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }

    // 既に作成済みならOK（両方揃っていること）
    if (!mSceneUBO.empty() && !mSceneUBO_UI.empty())
    {
        return true;
    }

    mSceneUBOSize = sizeof(VKSceneUBO);

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    //----------------------------------------------------------
    // resize
    //----------------------------------------------------------
    mSceneUBO.resize(frameCount, VK_NULL_HANDLE);
    mSceneUBOMem.resize(frameCount, VK_NULL_HANDLE);

    mSceneUBO_UI.resize(frameCount, VK_NULL_HANDLE);
    mSceneUBOMem_UI.resize(frameCount, VK_NULL_HANDLE);

    //----------------------------------------------------------
    // create buffers
    //----------------------------------------------------------
    for (size_t i = 0; i < frameCount; ++i)
    {
        // World UBO
        if (!CreateBufferHostVisible(
                (VkDeviceSize)mSceneUBOSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                mSceneUBO[i],
                mSceneUBOMem[i]))
        {
            std::cerr << "[VKRenderer] CreateSceneUBO(World) failed frame " << i << "\n";
            DestroySceneUBO();
            return false;
        }

        // UI UBO
        if (!CreateBufferHostVisible(
                (VkDeviceSize)mSceneUBOSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                mSceneUBO_UI[i],
                mSceneUBOMem_UI[i]))
        {
            std::cerr << "[VKRenderer] CreateSceneUBO(UI) failed frame " << i << "\n";
            DestroySceneUBO();
            return false;
        }
    }

    return true;
}

void VKRenderer::DestroySceneUBO()
{
    if (!mDevice)
    {
        return;
    }

    auto destroyVec = [&](std::vector<VkBuffer>& bufs,
                          std::vector<VkDeviceMemory>& mems)
    {
        for (size_t i = 0; i < bufs.size(); ++i)
        {
            if (bufs[i] != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(mDevice, bufs[i], nullptr);
                bufs[i] = VK_NULL_HANDLE;
            }
            if (mems[i] != VK_NULL_HANDLE)
            {
                vkFreeMemory(mDevice, mems[i], nullptr);
                mems[i] = VK_NULL_HANDLE;
            }
        }
        bufs.clear();
        mems.clear();
    };

    destroyVec(mSceneUBO, mSceneUBOMem);
    destroyVec(mSceneUBO_UI, mSceneUBOMem_UI);

    mSceneUBOSize = 0;
}

//==============================================================
// Scene UBO update (World)
//==============================================================
void VKRenderer::UpdateSceneUBO_World()
{
    if (mSceneUBOMem.empty())
    {
        return;
    }
    if (mFrameIndex >= mSceneUBOMem.size())
    {
        return;
    }

    VKSceneUBO ubo{};

    // ToyLib 既存規約に合わせる: viewProj = View * Proj
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
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

    UploadToBuffer(
        mSceneUBOMem[mFrameIndex],
        &ubo,
        (VkDeviceSize)mSceneUBOSize);
}

//==============================================================
// Scene UBO update (UI)
//==============================================================
void VKRenderer::UpdateSceneUBO_UI(const Matrix4& uiViewProj)
{
    if (mSceneUBOMem_UI.empty())
    {
        return;
    }
    if (mFrameIndex >= mSceneUBOMem_UI.size())
    {
        return;
    }

    VKSceneUBO ubo{};
    std::memcpy(ubo.viewProj, &uiViewProj, sizeof(float) * 16);

    // UI はライティング等を使わない想定だが、未定義値防止で最低限初期化
    ubo.cameraPos[0] = 0.0f;
    ubo.cameraPos[1] = 0.0f;
    ubo.cameraPos[2] = 0.0f;
    ubo.cameraPos[3] = 1.0f;

    ubo.ambient[0] = 1.0f;
    ubo.ambient[1] = 1.0f;
    ubo.ambient[2] = 1.0f;
    ubo.ambient[3] = 1.0f;

    UploadToBuffer(
        mSceneUBOMem_UI[mFrameIndex],
        &ubo,
        (VkDeviceSize)mSceneUBOSize);
}

//==============================================================
// Scene Descriptor Set (set=0 binding=0 UBO)
//  - set0 layout は Sprite/Mesh/Skinned で同一運用（前提）
//==============================================================
bool VKRenderer::CreateSceneDescriptorSet()
{
    if (!mDevice || !mDescPool)
    {
        return false;
    }

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }
    if (mSceneUBO.size() != frameCount || mSceneUBO_UI.size() != frameCount)
    {
        std::cerr << "[VK] CreateSceneDescriptorSet: SceneUBO not ready.\n";
        return false;
    }

    //----------------------------------------------------------
    // free old
    //----------------------------------------------------------
    auto freeAll = [&](std::vector<VkDescriptorSet>& arr)
    {
        for (auto& ds : arr)
        {
            if (ds != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
                ds = VK_NULL_HANDLE;
            }
        }
        arr.clear();
    };

    freeAll(mSceneSet);
    freeAll(mSceneSet_UI);

    mSceneSet.resize(frameCount, VK_NULL_HANDLE);
    mSceneSet_UI.resize(frameCount, VK_NULL_HANDLE);

    //----------------------------------------------------------
    // layout
    //----------------------------------------------------------
    VkDescriptorSetLayout set0 =
        GetPipelineSetLayout(mPipelines, "Sprite", 0);

    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateSceneDescriptorSet: set0 null\n";
        return false;
    }

    //----------------------------------------------------------
    // allocate BOTH world + UI
    //----------------------------------------------------------
    for (size_t i = 0; i < frameCount; ++i)
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set0;

        // world
        if (vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet[i]) != VK_SUCCESS)
        {
            std::cerr << "[VK] SceneSet(world) alloc failed frame=" << i << "\n";
            return false;
        }

        // ui
        if (vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet_UI[i]) != VK_SUCCESS)
        {
            std::cerr << "[VK] SceneSet(ui) alloc failed frame=" << i << "\n";
            return false;
        }

        //------------------------------------------------------
        // bind UBO world
        //------------------------------------------------------
        VkDescriptorBufferInfo biW{};
        biW.buffer = mSceneUBO[i];
        biW.offset = 0;
        biW.range  = mSceneUBOSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = mSceneSet[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &biW;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

        //------------------------------------------------------
        // bind UBO ui
        //------------------------------------------------------
        VkDescriptorBufferInfo biUI{};
        biUI.buffer = mSceneUBO_UI[i];
        biUI.offset = 0;
        biUI.range  = mSceneUBOSize;

        w.dstSet      = mSceneSet_UI[i];
        w.pBufferInfo = &biUI;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    //----------------------------------------------------------
    // fallback texture（set=1）
    //----------------------------------------------------------
    if (!CreateFallbackWhiteTexture())
    {
        return false;
    }

    // ★重要：pipelineごとにfallback DSを作る
    if (!CreateFallbackBaseMapSet("Sprite"))
    {
        return false;
    }
    if (!CreateFallbackBaseMapSet("Mesh"))
    {
        return false;
    }
    if (!CreateFallbackBaseMapSet("SkinnedMesh"))
    {
        return false;
    }

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
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            }
        }
    }
    mBaseMapSetCache.clear();

    // 旧互換 map も空に
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

// Helper to hash pipeline name (FNV-1a 32-bit)
static uint32_t HashPipelineName(const char* name)
{
    uint32_t h = 2166136261u;
    if (!name)
    {
        return h;
    }
    for (const unsigned char* p = (const unsigned char*)name; *p; ++p)
    {
        h ^= (uint32_t)(*p);
        h *= 16777619u;
    }
    return h;
}

VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    if (!mDevice || !mDescPool || !pipelineName)
    {
        std::cerr << "[VK] BaseMapSet: invalid state dev/pool/name\n";
        return VK_NULL_HANDLE;
    }

    // tex が無いなら fallback
    // tex が無いなら pipeline ごとの fallback
    if (!tex)
    {
        const uint32_t ph = HashPipelineName(pipelineName);

        auto it = mFallbackBaseMapSetByPipe.find(ph);
        if (it != mFallbackBaseMapSetByPipe.end() && it->second != VK_NULL_HANDLE)
        {
            return it->second;
        }

        // まだ無いなら作る（安全弁）
        if (CreateFallbackBaseMapSet(pipelineName))
        {
            it = mFallbackBaseMapSetByPipe.find(ph);
            if (it != mFallbackBaseMapSetByPipe.end())
            {
                return it->second;
            }
        }

        std::cerr << "[VK] BaseMapSet: no fallback DS (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    // cache (pipeline + texture)
    BaseMapKey key{};
    key.tex = tex;
    key.pipelineHash = HashPipelineName(pipelineName);

    if (auto it = mBaseMapSetCache.find(key); it != mBaseMapSetCache.end())
    {
        return it->second;
    }

    // set=1 layout を pipeline から取る
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

    // GPU backend
    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        std::cerr << "[VK] BaseMapSet: tex GPU is NULL tex=" << tex
                  << " (" << pipelineName << ")\n";
        return fail("[VK] BaseMapSet: tex GPU is NULL");
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        std::cerr << "[VK] BaseMapSet: GPU is not VKTextureGPU tex=" << tex
                  << " gpu=" << gpu
                  << " (" << pipelineName << ")\n";
        return fail("[VK] BaseMapSet: GPU is not VKTextureGPU");
    }

    const VkSampler sampler = vkgpu->GetSampler();
    const VkImageView view  = vkgpu->GetImageView();

    if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: sampler/view NULL tex=" << tex
                  << " sampler=" << (void*)sampler
                  << " view=" << (void*)view
                  << " (" << pipelineName << ")\n";
        return fail("[VK] BaseMapSet: sampler/view NULL");
    }

    VkDescriptorImageInfo ii{};
    ii.sampler     = sampler;
    ii.imageView   = view;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = ds;
    w.dstBinding      = 0; // ★ shader の binding と一致してる必要あり
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    mBaseMapSetCache[key] = ds;

    std::cerr << "[VK] BaseMapSet: created ds=" << (void*)ds
              << " tex=" << tex
              << " view=" << (void*)view
              << " sampler=" << (void*)sampler
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

    const uint32_t w = 1;
    const uint32_t h = 1;
    const uint32_t pixel = 0xFFFFFFFFu;

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

    vkDestroyBuffer(mDevice, staging, nullptr);
    vkFreeMemory(mDevice, stagingMem, nullptr);

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
    if (!mDevice)
    {
        return;
    }

    if (mFallbackWhiteSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mDevice, mFallbackWhiteSampler, nullptr);
        mFallbackWhiteSampler = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mFallbackWhiteView, nullptr);
        mFallbackWhiteView = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteImg != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mFallbackWhiteImg, nullptr);
        mFallbackWhiteImg = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mFallbackWhiteMem, nullptr);
        mFallbackWhiteMem = VK_NULL_HANDLE;
    }
}

bool VKRenderer::CreateFallbackBaseMapSet(const char* pipelineName)
{
    if (!mDevice || !mDescPool || !pipelineName)
    {
        return false;
    }
    if (mFallbackWhiteView == VK_NULL_HANDLE || mFallbackWhiteSampler == VK_NULL_HANDLE)
    {
        return false;
    }

    const uint32_t ph = HashPipelineName(pipelineName);

    {
        auto it = mFallbackBaseMapSetByPipe.find(ph);
        if (it != mFallbackBaseMapSetByPipe.end() && it->second != VK_NULL_HANDLE)
        {
            return true;
        }
    }

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] FallbackBaseMapSet: set1 layout null (" << pipelineName << ")\n";
        return false;
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
        std::cerr << "[VK] FallbackBaseMapSet: alloc failed vr=" << vr
                  << " (" << pipelineName << ")\n";
        return false;
    }

    VkDescriptorImageInfo ii{};
    ii.sampler     = mFallbackWhiteSampler;
    ii.imageView   = mFallbackWhiteView;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = ds;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    mFallbackBaseMapSetByPipe[ph] = ds;

    // 互換: 旧メンバー（Sprite用）も維持しておく
    if (std::strcmp(pipelineName, "Sprite") == 0)
    {
        mFallbackBaseMapSet = ds;
    }

    return true;
}

void VKRenderer::DestroyFallbackBaseMapSet()
{
    if (!mDevice || !mDescPool)
    {
        mFallbackBaseMapSetByPipe.clear();
        mFallbackBaseMapSet = VK_NULL_HANDLE;
        return;
    }

    for (auto& kv : mFallbackBaseMapSetByPipe)
    {
        VkDescriptorSet ds = kv.second;
        if (ds != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
        }
    }
    mFallbackBaseMapSetByPipe.clear();

    mFallbackBaseMapSet = VK_NULL_HANDLE;
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
