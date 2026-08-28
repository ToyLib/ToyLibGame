//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO(World+UI) / SceneSet(World+UI)
//  - BaseMap set cache (set=1) / Fallback(1x1 white) texture の管理は
//    VKBaseMapDescriptorCache に委譲（本ファイルからは呼び出すのみ）
//  - Skinned palette slots (set=2) : draw ごとに acquire して上書き事故を回避
//
// 方針（確定）:
//  - SceneUBO/SceneSet(World/UI) は VKUniformSet で管理する
//  - Update は UpdateSceneUBO_World / UpdateSceneUBO_UI のみを使う
//  - Skinned palette は AcquireSkinnedSet() で set=2 を draw ごとに確保/更新
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/LightingManager.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKShaderTypes.h"
#include "Render/VK/VKUtil.h"

#include <cstring>
#include <iostream>

namespace toy
{

static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}

//--------------------------------------------------------------
// “基準Pipeline” から setLayout を取る
//--------------------------------------------------------------
static VkDescriptorSetLayout GetPipelineSetLayout(VKPipelineLibrary& lib, const char* pipelineName, uint32_t setIndex)
{
    auto* p = lib.Get(pipelineName);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }
    return p->GetSetLayout(setIndex);
}

//==============================================================
// DescriptorPool (UBO用: Scene + Skinned)
//==============================================================
bool VKRenderer::CreateDescriptorPool()
{
    if (!mDevice)
    {
        return false;
    }

    constexpr uint32_t kMaxSetsTotal = 8192;
    constexpr uint32_t kUBOCount = 8192;

    // PostEffect等のtexture descriptor用
    constexpr uint32_t kSamplerCount = 1024;

    // Skinned palette(set=2)用。UBOのmaxUniformBufferRange保証最小値(16KB)を
    // 320ボーン分(20KB)が超えるためSTORAGE_BUFFERを使う（AddSet2_SkinnedUBO参照）
    constexpr uint32_t kSSBOCount = 1024;

    VkDescriptorPoolSize sizes[3]{};

    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kUBOCount;

    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kSamplerCount;

    sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[2].descriptorCount = kSSBOCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets = kMaxSetsTotal;
    ci.poolSizeCount = 3;
    ci.pPoolSizes = sizes;

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

    //----------------------------------------------------------
    // BaseMap pools は mDescPool と独立
    //----------------------------------------------------------
    mBaseMapCache.Clear();

    //----------------------------------------------------------
    // Skinned slot pool (UBO + DS)
    //----------------------------------------------------------
    DestroySkinnedSlots();

    //----------------------------------------------------------
    // Scene / Sky / Overlay の UBO+Set は VKUniformSet::Destroy() 側で
    // 破棄済み（DestroySceneUBO/DestroySkyUBO/DestroyOverlayUBOが先に呼ばれる）。
    // ここでは pool 自体を破棄するのみ。
    //----------------------------------------------------------
    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // fallback image/sampler（pool所有ではない）
    //----------------------------------------------------------
    mBaseMapCache.DestroyFallbackWhiteTexture();
}

//==============================================================
// Scene UBO (World + UI)
//==============================================================
bool VKRenderer::CreateSceneUBO()
{
    if (!mDevice || !mPhysicalDevice || !mDescPool)
    {
        return false;
    }

    if (mSceneUniformWorld.IsCreated() && mSceneUniformUI.IsCreated())
    {
        return true;
    }

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    // set0 layout は “Sprite” を基準に取得（set0は共通運用の前提）
    VkDescriptorSetLayout set0 = GetPipelineSetLayout(mPipelines, "Sprite", 0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateSceneUBO: set0 null\n";
        return false;
    }

    if (!mSceneUniformWorld.Create(mDevice, mPhysicalDevice, mDescPool, set0, sizeof(VKSceneUBO), frameCount))
    {
        std::cerr << "[VKRenderer] CreateSceneUBO(World) failed\n";
        DestroySceneUBO();
        return false;
    }

    if (!mSceneUniformUI.Create(mDevice, mPhysicalDevice, mDescPool, set0, sizeof(VKSceneUBO), frameCount))
    {
        std::cerr << "[VKRenderer] CreateSceneUBO(UI) failed\n";
        DestroySceneUBO();
        return false;
    }

    return true;
}

void VKRenderer::DestroySceneUBO()
{
    mSceneUniformWorld.Destroy(mDevice, mDescPool);
    mSceneUniformUI.Destroy(mDevice, mDescPool);
}

//==============================================================
// Scene UBO update (World)
//==============================================================
void VKRenderer::UpdateSceneUBO_World()
{
    if (!mSceneUniformWorld.IsCreated())
    {
        return;
    }

    VKSceneUBO ubo{};

    // ToyLib 規約: viewProj = View * Proj（row-vector想定でも “CPU側” と一致してればOK）
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
    std::memcpy(ubo.viewProj, &viewProj, sizeof(float) * 16);

    // ライトデータ収集（LightingManager に一本化）
    SceneLightData ld{};
    if (auto lm = GetLightingManager())
    {
        ld = lm->BuildLightData(mViewMatrix);
    }

    // Camera pos
    ubo.cameraPos[0] = ld.cameraPos.x;
    ubo.cameraPos[1] = ld.cameraPos.y;
    ubo.cameraPos[2] = ld.cameraPos.z;
    ubo.cameraPos[3] = 1.0f;

    // Ambient
    ubo.ambient[0] = ld.ambientColor.x;
    ubo.ambient[1] = ld.ambientColor.y;
    ubo.ambient[2] = ld.ambientColor.z;
    ubo.ambient[3] = 1.0f;

    // Directional light
    ubo.dirDir[0] = ld.dirDirection.x;
    ubo.dirDir[1] = ld.dirDirection.y;
    ubo.dirDir[2] = ld.dirDirection.z;
    ubo.dirDir[3] = 0.0f;

    ubo.dirDiffuse[0] = ld.dirDiffuse.x;
    ubo.dirDiffuse[1] = ld.dirDiffuse.y;
    ubo.dirDiffuse[2] = ld.dirDiffuse.z;
    ubo.dirDiffuse[3] = 1.0f;

    ubo.dirSpecular[0] = ld.dirSpecular.x;
    ubo.dirSpecular[1] = ld.dirSpecular.y;
    ubo.dirSpecular[2] = ld.dirSpecular.z;
    ubo.dirSpecular[3] = 1.0f;

    // Point lights
    ubo.numPointLights = ld.numPointLights;
    for (int i = 0; i < ld.numPointLights; ++i)
    {
        const PointLightData& pl = ld.pointLights[i];
        ubo.pointLights[i].position_radius[0] = pl.position.x;
        ubo.pointLights[i].position_radius[1] = pl.position.y;
        ubo.pointLights[i].position_radius[2] = pl.position.z;
        ubo.pointLights[i].position_radius[3] = pl.radius;

        ubo.pointLights[i].color_intensity[0] = pl.color.x;
        ubo.pointLights[i].color_intensity[1] = pl.color.y;
        ubo.pointLights[i].color_intensity[2] = pl.color.z;
        ubo.pointLights[i].color_intensity[3] = pl.intensity;

        ubo.pointLights[i].atten[0] = pl.constant;
        ubo.pointLights[i].atten[1] = pl.linear;
        ubo.pointLights[i].atten[2] = pl.quadratic;
        ubo.pointLights[i].atten[3] = 0.0f;
    }

    // Fog
    ubo.fogColor[0] = ld.fogColor.x;
    ubo.fogColor[1] = ld.fogColor.y;
    ubo.fogColor[2] = ld.fogColor.z;
    ubo.fogColor[3] = 1.0f;

    ubo.fogParams[0] = ld.fogMinDist;
    ubo.fogParams[1] = ld.fogMaxDist;
    ubo.fogParams[2] = 0.0f;
    ubo.fogParams[3] = 0.0f;

    //========================
    // Shadow (Step3)
    //========================
    if ((int)mShadowCascades.size() == 2)
    {
        // non-biased LightVP（GL互換）
        StoreMat4(ubo.shadowVP0, mShadowCascades[0].lightVP);
        StoreMat4(ubo.shadowVP1, mShadowCascades[1].lightVP);

        ubo.shadowParams[0] = GetCascadeSplit0(); // split0
        ubo.shadowParams[1] = GetCascadeBlend();  // blend
        ubo.shadowParams[2] = 1.0f;               // strength: 影を普通に効かせるならまず 1
        ubo.shadowParams[3] = GetShadowBias();    // bias: まずは 0.001〜0.01 で調整（PCF 3x3前提）
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

    ubo.shadowFlags[0] = static_cast<int>(mEnableShadow);
    ubo.shadowFlags[1] = 0;
    ubo.shadowFlags[2] = 0;
    ubo.shadowFlags[3] = 0;

    mSceneUniformWorld.Upload(mDevice, mFrameIndex, &ubo, sizeof(ubo));
}

//==============================================================
// Scene UBO update (UI)
//==============================================================
void VKRenderer::UpdateSceneUBO_UI(const Matrix4& uiViewProj)
{
    if (!mSceneUniformUI.IsCreated())
    {
        return;
    }

    VKSceneUBO ubo{};
    std::memcpy(ubo.viewProj, &uiViewProj, sizeof(float) * 16);

    // UI は最低限初期化
    ubo.cameraPos[3] = 1.0f;
    ubo.ambient[0] = 1.0f;
    ubo.ambient[1] = 1.0f;
    ubo.ambient[2] = 1.0f;
    ubo.ambient[3] = 1.0f;

    // Fog: “影響なし” の値
    ubo.fogColor[0] = 0.0f;
    ubo.fogColor[1] = 0.0f;
    ubo.fogColor[2] = 0.0f;
    ubo.fogColor[3] = 1.0f;

    ubo.fogParams[0] = 0.0f;   // min
    ubo.fogParams[1] = 1.0e9f; // max を巨大に（fogFactor≈1）
    ubo.fogParams[2] = 0.0f;
    ubo.fogParams[3] = 0.0f;

    mSceneUniformUI.Upload(mDevice, mFrameIndex, &ubo, sizeof(ubo));
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

    //----------------------------------------------------------
    // fallback texture（set=1）
    //----------------------------------------------------------
    mBaseMapCache.Init(mDevice, mPhysicalDevice);

    if (!mBaseMapCache.CreateFallbackWhiteTexture([this]() { return BeginOneTimeCommands(); },
                                                   [this](VkCommandBuffer cmd) { EndOneTimeCommands(cmd); }))
    {
        return false;
    }

    // BaseMap 側は pool chain を使う（枯れ対策）
    static const char* kFallbackPipelines[] = {"Sprite",      "Mesh",           "Mesh_CW",
                                               "SkinnedMesh", "SkinnedMesh_CW", "UnlitQuad"};
    for (const char* pipelineName : kFallbackPipelines)
    {
        if (!mBaseMapCache.CreateFallbackBaseMapSet(mPipelines, pipelineName))
        {
            return false;
        }
    }

    return true;
}

//==============================================================
// BaseMap(set=1) の管理は VKBaseMapDescriptorCache に委譲
//==============================================================
VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    return mBaseMapCache.GetOrCreateBaseMapSet(mPipelines, tex, pipelineName);
}

//==============================================================
// Host-visible buffer helpers（既存）
//==============================================================
bool VKRenderer::CreateBufferHostVisible(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuf,
                                         VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!mDevice || !mPhysicalDevice || size == 0)
    {
        return false;
    }

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    const uint32_t typeIndex =
        toy::vkutil::FindMemoryType(mPhysicalDevice, req.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (typeIndex == UINT32_MAX)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
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

//==============================================================
// Skinned slot pool (set=2) : mDescPool から確保
//==============================================================
void VKRenderer::DestroySkinnedSlots()
{
    if (!mDevice)
    {
        mSkinnedSlots.clear();
        mSkinnedSlotCursor.clear();
        return;
    }

    for (auto& perFrame : mSkinnedSlots)
    {
        for (auto& s : perFrame)
        {
            if (mDescPool && s.set != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &s.set);
                s.set = VK_NULL_HANDLE;
            }

            if (s.ubo != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(mDevice, s.ubo, nullptr);
                s.ubo = VK_NULL_HANDLE;
            }
            if (s.mem != VK_NULL_HANDLE)
            {
                vkFreeMemory(mDevice, s.mem, nullptr);
                s.mem = VK_NULL_HANDLE;
            }
        }
    }

    mSkinnedSlots.clear();
    mSkinnedSlotCursor.clear();
}

//==============================================================
// Sky UBO (set=1 binding=0)
//==============================================================
bool VKRenderer::CreateSkyUBO()
{
    if (!mDevice || !mPhysicalDevice || !mDescPool)
    {
        return false;
    }

    if (mSkyUniform.IsCreated())
    {
        return true;
    }

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, "SkyDome", 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateSkyUBO: SkyDome set1 null\n";
        return false;
    }

    if (!mSkyUniform.Create(mDevice, mPhysicalDevice, mDescPool, set1, sizeof(VKSkyUBO), frameCount))
    {
        std::cerr << "[VKRenderer] CreateSkyUBO failed\n";
        return false;
    }

    return true;
}

void VKRenderer::DestroySkyUBO()
{
    mSkyUniform.Destroy(mDevice, mDescPool);
}

//==============================================================
// Sky UBO update
//==============================================================
void VKRenderer::UpdateSkyUBO(const SkyDomePayload& sky)
{
    if (!mSkyUniform.IsCreated())
    {
        return;
    }

    VKSkyUBO ubo{};

    // ---------------------------------------------------------
    // world
    // ---------------------------------------------------------
    StoreMat4(ubo.world, sky.world);

    // ---------------------------------------------------------
    // time params
    //   x = uTime
    //   y = uTimeOfDay
    //   z = uWeatherType
    //   w = reserved
    // ---------------------------------------------------------
    ubo.timeParams[0] = sky.skyTime;
    ubo.timeParams[1] = sky.skyTimeOfDay;
    ubo.timeParams[2] = static_cast<float>(sky.skyWeatherType);
    ubo.timeParams[3] = 0.0f;

    // ---------------------------------------------------------
    // sun dir
    // ---------------------------------------------------------
    ubo.sunDir[0] = sky.skySunDir.x;
    ubo.sunDir[1] = sky.skySunDir.y;
    ubo.sunDir[2] = sky.skySunDir.z;
    ubo.sunDir[3] = 0.0f;

    // ---------------------------------------------------------
    // moon dir
    // ---------------------------------------------------------
    ubo.moonDir[0] = sky.skyMoonDir.x;
    ubo.moonDir[1] = sky.skyMoonDir.y;
    ubo.moonDir[2] = sky.skyMoonDir.z;
    ubo.moonDir[3] = 0.0f;

    // ---------------------------------------------------------
    // raw sky color
    // ---------------------------------------------------------
    ubo.rawSkyColor[0] = sky.skyRawSkyColor.x;
    ubo.rawSkyColor[1] = sky.skyRawSkyColor.y;
    ubo.rawSkyColor[2] = sky.skyRawSkyColor.z;
    ubo.rawSkyColor[3] = 0.0f;

    // ---------------------------------------------------------
    // raw cloud color
    // ---------------------------------------------------------
    ubo.rawCloudColor[0] = sky.skyRawCloudColor.x;
    ubo.rawCloudColor[1] = sky.skyRawCloudColor.y;
    ubo.rawCloudColor[2] = sky.skyRawCloudColor.z;
    ubo.rawCloudColor[3] = 0.0f;

    mSkyUniform.Upload(mDevice, mFrameIndex, &ubo, sizeof(ubo));
}

//==============================================================
// Overlay UBO (set=1 binding=0)
//==============================================================
bool VKRenderer::CreateOverlayUBO()
{
    if (!mDevice || !mPhysicalDevice || !mDescPool)
    {
        return false;
    }

    if (mOverlayUniform.IsCreated())
    {
        return true;
    }

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    // Alpha版を基準に set=1 layout を取得
    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, "WeatherOverlay", 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateOverlayUBO: WeatherOverlay set1 null\n";
        return false;
    }

    if (!mOverlayUniform.Create(mDevice, mPhysicalDevice, mDescPool, set1, sizeof(VKOverlayUBO), frameCount))
    {
        std::cerr << "[VKRenderer] CreateOverlayUBO failed\n";
        return false;
    }

    return true;
}

void VKRenderer::DestroyOverlayUBO()
{
    mOverlayUniform.Destroy(mDevice, mDescPool);
}

//==============================================================
// Overlay UBO update
//==============================================================
void VKRenderer::UpdateOverlayUBO(const OverlayPayload& overlay)
{
    if (!mOverlayUniform.IsCreated())
    {
        return;
    }

    VKOverlayUBO ubo{};

    // time
    ubo.time[0] = overlay.time;
    ubo.time[1] = 0.0f;
    ubo.time[2] = 0.0f;
    ubo.time[3] = 0.0f;

    // resolution
    ubo.resolution[0] = overlay.resolution.x;
    ubo.resolution[1] = overlay.resolution.y;
    ubo.resolution[2] = 0.0f;
    ubo.resolution[3] = 0.0f;

    // weather params
    ubo.weather[0] = overlay.rainAmount;
    ubo.weather[1] = overlay.snowAmount;
    ubo.weather[2] = overlay.fogAmount;
    ubo.weather[3] = 0.0f;

    // sun pos
    ubo.sunPos[0] = overlay.sunPos.x;
    ubo.sunPos[1] = overlay.sunPos.y;
    ubo.sunPos[2] = 0.0f;
    ubo.sunPos[3] = 0.0f;

    // flare
    ubo.flare[0] = overlay.flareIntensity;
    ubo.flare[1] = 0.0f;
    ubo.flare[2] = 0.0f;
    ubo.flare[3] = 0.0f;

    // flare color
    ubo.flareColor[0] = overlay.flareColor.x;
    ubo.flareColor[1] = overlay.flareColor.y;
    ubo.flareColor[2] = overlay.flareColor.z;
    ubo.flareColor[3] = 0.0f;

    mOverlayUniform.Upload(mDevice, mFrameIndex, &ubo, sizeof(ubo));
}

} // namespace toy
