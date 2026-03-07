//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO(World+UI) / SceneSet(World+UI)
//  - BaseMap set cache (set=1) : “専用 pool を増設”
//  - Fallback(1x1 white) texture & set=1 (pipelineごと)
//  - Skinned palette slots (set=2) : draw ごとに acquire して上書き事故を回避
//
// 方針（確定）:
//  - SceneUBO は World と UI を分離（mSceneUBO / mSceneUBO_UI）
//  - SceneSet も World と UI を分離（mSceneSet / mSceneSet_UI）
//  - Update は UpdateSceneUBO_World / UpdateSceneUBO_UI のみを使う
//  - Skinned palette は AcquireSkinnedSet() で set=2 を draw ごとに確保/更新
//  - BaseMap(set=1) は baseMapPools から確保し、枯れたら増設
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKUtil.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Asset/Material/VKTextureGPU.h"
#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/Texture.h"
#include "Render/LightingManager.h"
#include "Graphics/Light/PointLightComponent.h"

#include <iostream>
#include <cstring>
#include <unordered_map>
#include <algorithm>

namespace toy
{

static void StoreMat4(float out16[16], const Matrix4& m)
{
    std::memcpy(out16, &m, sizeof(float) * 16);
}


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

static std::string NormalizePipelineName(const char* name)
{
    return name ? std::string(name) : std::string();
}



static bool IsShadowPipelineName(const char* pipelineName)
{
    if (!pipelineName) return false;
    return (std::strncmp(pipelineName, "Shadow", 6) == 0);
}

//==============================================================
// DescriptorPool (UBO用: Scene + Skinned)
//==============================================================
bool VKRenderer::CreateDescriptorPool()
{
    if (!mDevice) return false;

    // UBO系（Scene + Skinned）を枯らさないために十分大きく
    constexpr uint32_t kMaxSetsTotal = 8192;   // UBO set の総数上限
    constexpr uint32_t kUBOCount     = 8192;   // UNIFORM_BUFFER の総数上限

    VkDescriptorPoolSize sizes[1]{};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kUBOCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSetsTotal;
    ci.poolSizeCount = 1;
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

    //----------------------------------------------------------
    // BaseMap pools は mDescPool と独立
    //----------------------------------------------------------
    ClearBaseMapSetCache();        // pool destroy を含む
    DestroyFallbackBaseMapSet();   // 念のため（Clear内で呼ぶが保険）

    //----------------------------------------------------------
    // Skinned slot pool (UBO + DS)
    //----------------------------------------------------------
    DestroySkinnedSlots();

    //----------------------------------------------------------
    // Scene sets は mDescPool 所有
    //----------------------------------------------------------
    if (mDescPool != VK_NULL_HANDLE)
    {
        for (auto& set : mSceneSet)
        {
            if (set != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &set);
                set = VK_NULL_HANDLE;
            }
        }
        mSceneSet.clear();

        for (auto& set : mSceneSet_UI)
        {
            if (set != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &set);
                set = VK_NULL_HANDLE;
            }
        }
        mSceneSet_UI.clear();

        for (auto& set : mSkySet)
        {
            if (set != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &set);
                set = VK_NULL_HANDLE;
            }
        }
        mSkySet.clear();
        
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
    else
    {
        mSceneSet.clear();
        mSceneSet_UI.clear();
        mSkySet.clear();
    }

    //----------------------------------------------------------
    // fallback image/sampler（pool所有ではない）
    //----------------------------------------------------------
    DestroyFallbackWhiteTexture();
}

//==============================================================
// Scene UBO layout (Shader 側 set=0 binding=0)
//==============================================================
//==============================================================
// Scene UBO layout (Shader 側 set=0 binding=0)
//==============================================================
struct VKPointLight
{
    float position_radius[4]; // xyz=pos, w=radius
    float color_intensity[4]; // xyz=color, w=intensity
    float atten[4];           // x=constant, y=linear, z=quadratic, w=pad
};

struct VKSceneUBO
{
    float viewProj[16];
    float cameraPos[4];
    float ambient[4];
    float dirDir[4];
    float dirDiffuse[4];
    float dirSpecular[4];

    float fogColor[4];   // xyz
    float fogParams[4];  // x=minDist, y=maxDist

    // --------------------------
    // PointLights (GL互換: max 8)
    // std140 を強く意識して 16byte に揃える
    // --------------------------
    int   numPointLights;
    int   _plPad0;
    int   _plPad1;
    int   _plPad2;

    VKPointLight pointLights[8];

    // shadow (Step3)
    float shadowVP0[16];
    float shadowVP1[16];
    float shadowParams[4];
};

// SkyDome
struct VKSkyUBO
{
    float world[16];
    float timeParams[4];
    float sunDir[4];
    float moonDir[4];
    float rawSkyColor[4];
    float rawCloudColor[4];
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

    mSceneUBO.resize(frameCount, VK_NULL_HANDLE);
    mSceneUBOMem.resize(frameCount, VK_NULL_HANDLE);

    mSceneUBO_UI.resize(frameCount, VK_NULL_HANDLE);
    mSceneUBOMem_UI.resize(frameCount, VK_NULL_HANDLE);

    for (size_t i = 0; i < frameCount; ++i)
    {
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
    if (mSceneUBOMem.empty()) return;
    if (mFrameIndex >= mSceneUBOMem.size()) return;

    VKSceneUBO ubo{};

    // ToyLib 規約: viewProj = View * Proj（row-vector想定でも “CPU側” と一致してればOK）
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
    
    // --------------------------
    // PointLights（GLと同じ: 最大8）
    // --------------------------
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

            // ---- ここは PointLightComponent の実装に合わせて getter 名を調整 ----
            const Vector3 pos   = pl->GetPosition();   // 例
            const Vector3 color = pl->GetColor();           // 例 (0..1)
            const float   inten = pl->GetIntensity();       // 例
            const float   c     = pl->GetConstant();        // 例
            const float   l     = pl->GetLinear();          // 例
            const float   q     = pl->GetQuadratic();       // 例
            const float   r     = pl->GetRadius();          // 例
            // -----------------------------------------------------------------

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
    
    // --------------------------
    // Fog（GLと同じ契約）
    // --------------------------
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
    
    

    //========================
    // Shadow (Step3)
    //========================
    if ((int)mShadowCascades.size() == 2)
    {
        // non-biased LightVP（GL互換）
        StoreMat4(ubo.shadowVP0, mShadowCascades[0].lightVP);
        StoreMat4(ubo.shadowVP1, mShadowCascades[1].lightVP);

        ubo.shadowParams[0] = GetCascadeSplit0();  // split0
        ubo.shadowParams[1] = GetCascadeBlend();   // blend
        ubo.shadowParams[2] = 1.0f;                // strength: 影を普通に効かせるならまず 1
        ubo.shadowParams[3] = GetShadowBias();             // bias: まずは 0.001〜0.01 で調整（PCF 3x3前提）
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


    UploadToBuffer(mSceneUBOMem[mFrameIndex], &ubo, (VkDeviceSize)mSceneUBOSize);
}

//==============================================================
// Scene UBO update (UI)
//==============================================================
void VKRenderer::UpdateSceneUBO_UI(const Matrix4& uiViewProj)
{
    if (mSceneUBOMem_UI.empty()) return;
    if (mFrameIndex >= mSceneUBOMem_UI.size()) return;

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

    UploadToBuffer(mSceneUBOMem_UI[mFrameIndex], &ubo, (VkDeviceSize)mSceneUBOSize);
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

    // free old
    for (auto& ds : mSceneSet)
    {
        if (ds != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            ds = VK_NULL_HANDLE;
        }
    }
    mSceneSet.clear();

    for (auto& ds : mSceneSet_UI)
    {
        if (ds != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            ds = VK_NULL_HANDLE;
        }
    }
    mSceneSet_UI.clear();

    mSceneSet.resize(frameCount, VK_NULL_HANDLE);
    mSceneSet_UI.resize(frameCount, VK_NULL_HANDLE);

    // set0 layout は “Sprite” を基準に取得（set0は共通運用の前提）
    VkDescriptorSetLayout set0 = GetPipelineSetLayout(mPipelines, "Sprite", 0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateSceneDescriptorSet: set0 null\n";
        return false;
    }

    for (size_t i = 0; i < frameCount; ++i)
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set0;

        if (vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet[i]) != VK_SUCCESS)
        {
            std::cerr << "[VK] SceneSet(world) alloc failed frame=" << i << "\n";
            return false;
        }

        if (vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet_UI[i]) != VK_SUCCESS)
        {
            std::cerr << "[VK] SceneSet(ui) alloc failed frame=" << i << "\n";
            return false;
        }

        // world
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

        // ui
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

    // BaseMap 側は pool chain を使う（枯れ対策）
    if (!CreateFallbackBaseMapSet("Sprite"))         return false;
    if (!CreateFallbackBaseMapSet("Mesh"))           return false;
    if (!CreateFallbackBaseMapSet("Mesh_CW"))        return false;
    if (!CreateFallbackBaseMapSet("SkinnedMesh"))    return false;
    if (!CreateFallbackBaseMapSet("SkinnedMesh_CW")) return false;
    if (!CreateFallbackBaseMapSet("UnlitQuad"))      return false;

    return true;
}

//==============================================================
// BaseMap pools
//==============================================================
VkDescriptorPool VKRenderer::CreateBaseMapPool(uint32_t maxSets, uint32_t samplerCount)
{
    if (!mDevice)
    {
        return VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize sizes[1]{};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = samplerCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = 0; // ★個別freeしない運用（poolごと破棄）
    ci.maxSets       = maxSets;
    ci.poolSizeCount = 1;
    ci.pPoolSizes    = sizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult vr = vkCreateDescriptorPool(mDevice, &ci, nullptr, &pool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] CreateBaseMapPool failed vr=" << vr << "\n";
        return VK_NULL_HANDLE;
    }
    return pool;
}

VkDescriptorPool VKRenderer::GetActiveBaseMapPool()
{
    if (mBaseMapPools.empty())
    {
        VkDescriptorPool p = CreateBaseMapPool(/*maxSets*/8192, /*samplerCount*/8192);
        if (p) mBaseMapPools.push_back(p);
        mBaseMapPoolCursor = 0;
    }
    return mBaseMapPools.empty() ? VK_NULL_HANDLE : mBaseMapPools[mBaseMapPoolCursor];
}

VkDescriptorPool VKRenderer::GrowBaseMapPoolAndGet()
{
    const uint32_t n = (uint32_t)mBaseMapPools.size();
    const uint32_t maxSets   = 8192u + 4096u * n;
    const uint32_t samplers  = 8192u + 4096u * n;

    VkDescriptorPool p = CreateBaseMapPool(maxSets, samplers);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }

    mBaseMapPools.push_back(p);
    mBaseMapPoolCursor = (uint32_t)mBaseMapPools.size() - 1;
    return p;
}

//==============================================================
// BaseMap set cache (set=1 binding=0 CombinedImageSampler)
//==============================================================
void VKRenderer::ClearBaseMapSetCache()
{
    // cacheは “poolごと破棄” するので、個別 vkFree は不要
    mBaseMapSetCache.clear();

    // fallback DS も baseMap pool 所有なので破棄対象
    mFallbackBaseMapSetByPipe.clear();
    mFallbackBaseMapSet = VK_NULL_HANDLE;

    // baseMap pools destroy
    if (mDevice)
    {
        for (auto& p : mBaseMapPools)
        {
            if (p != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(mDevice, p, nullptr);
                p = VK_NULL_HANDLE;
            }
        }
    }
    mBaseMapPools.clear();
    mBaseMapPoolCursor = 0;
}

VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    if (!mDevice || !pipelineName)
    {
        std::cerr << "[VK] BaseMapSet: invalid state dev/name\n";
        return VK_NULL_HANDLE;
    }

    const std::string pipeName = NormalizePipelineName(pipelineName);

    // Shadow pass は set=1(BaseMap) を使わない設計。
    // ここに来たら呼び出し側の設計ミスなので弾く。
    if (IsShadowPipelineName(pipelineName))
    {
        std::cerr << "[VK] BaseMapSet: Shadow pipeline requested set=1 (BUG) name="
                  << pipelineName << "\n";
        return VK_NULL_HANDLE;
    }
    
    //----------------------------------------------------------
    // fallback
    //----------------------------------------------------------
    if (!tex)
    {
        auto it = mFallbackBaseMapSetByPipe.find(pipeName);
        if (it != mFallbackBaseMapSetByPipe.end() && it->second.set != VK_NULL_HANDLE)
        {
            return it->second.set;
        }

        if (CreateFallbackBaseMapSet(pipelineName))
        {
            it = mFallbackBaseMapSetByPipe.find(pipeName);
            if (it != mFallbackBaseMapSetByPipe.end())
            {
                return it->second.set;
            }
        }

        std::cerr << "[VK] BaseMapSet: fallback missing (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // frame-aware cache
    //----------------------------------------------------------
    BaseMapKey key{};
    key.frame = mFrameIndex;
    key.tex = tex;
    key.pipelineName = pipeName;

    if (auto it = mBaseMapSetCache.find(key); it != mBaseMapSetCache.end())
    {
        return it->second.set;
    }

    //----------------------------------------------------------
    // layout
    //----------------------------------------------------------
    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: set1 layout NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // GPU
    //----------------------------------------------------------
    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        std::cerr << "[VK] BaseMapSet: tex GPU NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        std::cerr << "[VK] BaseMapSet: GPU not VKTextureGPU (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    const VkSampler sampler = vkgpu->GetSampler();
    const VkImageView view  = vkgpu->GetImageView();
    if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: sampler/view NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // alloc from active baseMap pool
    //----------------------------------------------------------
    VkDescriptorPool pool = GetActiveBaseMapPool();
    if (pool == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: baseMap pool null (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto allocOnce = [&](VkDescriptorPool p, VkDescriptorSet& outSet) -> VkResult
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = p;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set1;

        return vkAllocateDescriptorSets(mDevice, &ai, &outSet);
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult vr = allocOnce(pool, ds);

    // 枯れたら増設してもう一回
    if (vr == VK_ERROR_OUT_OF_POOL_MEMORY || vr == VK_ERROR_FRAGMENTED_POOL)
    {
        pool = GrowBaseMapPoolAndGet();
        if (pool == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] BaseMapSet: grow pool failed (" << pipelineName << ")\n";
            return VK_NULL_HANDLE;
        }

        ds = VK_NULL_HANDLE;
        vr = allocOnce(pool, ds);
    }

    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: alloc failed vr=" << vr
                  << " (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // write
    //----------------------------------------------------------
    VkDescriptorImageInfo ii{};
    ii.sampler     = sampler;
    ii.imageView   = view;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = ds;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    //----------------------------------------------------------
    // cache
    //----------------------------------------------------------
    CachedDescriptorSet cds{};
    cds.pool = pool;
    cds.set  = ds;
    mBaseMapSetCache[key] = cds;

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

//==============================================================
// Fallback BaseMap DS (set=1)
//  - BaseMapPoolから allocate（枯れ対策）
//==============================================================
bool VKRenderer::CreateFallbackBaseMapSet(const char* pipelineName)
{
    if (!mDevice || !pipelineName)
    {
        return false;
    }
    if (mFallbackWhiteView == VK_NULL_HANDLE || mFallbackWhiteSampler == VK_NULL_HANDLE)
    {
        return false;
    }

    const std::string pipeName = NormalizePipelineName(pipelineName);

    {
        auto it = mFallbackBaseMapSetByPipe.find(pipeName);
        if (it != mFallbackBaseMapSetByPipe.end() && it->second.set != VK_NULL_HANDLE)
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

    VkDescriptorPool pool = GetActiveBaseMapPool();
    if (pool == VK_NULL_HANDLE)
    {
        return false;
    }

    auto allocOnce = [&](VkDescriptorPool p, VkDescriptorSet& outSet) -> VkResult
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = p;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set1;
        return vkAllocateDescriptorSets(mDevice, &ai, &outSet);
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult vr = allocOnce(pool, ds);

    if (vr == VK_ERROR_OUT_OF_POOL_MEMORY || vr == VK_ERROR_FRAGMENTED_POOL)
    {
        pool = GrowBaseMapPoolAndGet();
        if (pool == VK_NULL_HANDLE)
        {
            return false;
        }
        ds = VK_NULL_HANDLE;
        vr = allocOnce(pool, ds);
    }

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

    CachedDescriptorSet cds{};
    cds.pool = pool;
    cds.set  = ds;

    mFallbackBaseMapSetByPipe[pipeName] = cds;

    // backward compat
    if (std::strcmp(pipelineName, "Sprite") == 0)
    {
        mFallbackBaseMapSet = ds;
    }

    return true;
}

void VKRenderer::DestroyFallbackBaseMapSet()
{
    // baseMap pool を “まとめて破棄” する設計なので、
    // ここでは map をクリアするだけでOK（pool破棄は ClearBaseMapSetCache で行う）
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
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }

    if (!mSkyUBO.empty())
    {
        return true;
    }

    mSkyUBOSize = sizeof(VKSkyUBO);

    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    mSkyUBO.resize(frameCount, VK_NULL_HANDLE);
    mSkyUBOMem.resize(frameCount, VK_NULL_HANDLE);

    for (size_t i = 0; i < frameCount; ++i)
    {
        if (!CreateBufferHostVisible(
                (VkDeviceSize)mSkyUBOSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                mSkyUBO[i],
                mSkyUBOMem[i]))
        {
            std::cerr << "[VKRenderer] CreateSkyUBO failed frame " << i << "\n";
            DestroySkyUBO();
            return false;
        }
    }

    return true;
}

void VKRenderer::DestroySkyUBO()
{
    if (!mDevice)
    {
        return;
    }

    for (size_t i = 0; i < mSkyUBO.size(); ++i)
    {
        if (mSkyUBO[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(mDevice, mSkyUBO[i], nullptr);
            mSkyUBO[i] = VK_NULL_HANDLE;
        }
        if (mSkyUBOMem[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(mDevice, mSkyUBOMem[i], nullptr);
            mSkyUBOMem[i] = VK_NULL_HANDLE;
        }
    }

    mSkyUBO.clear();
    mSkyUBOMem.clear();
    mSkyUBOSize = 0;
}

//==============================================================
// Sky UBO update
//==============================================================
void VKRenderer::UpdateSkyUBO(const SkyDomePayload& sky)
{
    if (mSkyUBOMem.empty())
    {
        return;
    }
    if (mFrameIndex >= mSkyUBOMem.size())
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

    UploadToBuffer(
        mSkyUBOMem[mFrameIndex],
        &ubo,
        static_cast<VkDeviceSize>(mSkyUBOSize));
}

//==============================================================
// Sky Descriptor Set (set=1 binding=0 UBO)
//==============================================================
bool VKRenderer::CreateSkyDescriptorSet()
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

    if (mSkyUBO.size() != frameCount)
    {
        std::cerr << "[VK] CreateSkyDescriptorSet: SkyUBO not ready.\n";
        return false;
    }

    for (auto& ds : mSkySet)
    {
        if (ds != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            ds = VK_NULL_HANDLE;
        }
    }
    mSkySet.clear();

    mSkySet.resize(frameCount, VK_NULL_HANDLE);

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, "SkyDome", 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] CreateSkyDescriptorSet: SkyDome set1 null\n";
        return false;
    }

    for (size_t i = 0; i < frameCount; ++i)
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set1;

        if (vkAllocateDescriptorSets(mDevice, &ai, &mSkySet[i]) != VK_SUCCESS)
        {
            std::cerr << "[VK] SkySet alloc failed frame=" << i << "\n";
            return false;
        }

        VkDescriptorBufferInfo bi{};
        bi.buffer = mSkyUBO[i];
        bi.offset = 0;
        bi.range  = mSkyUBOSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = mSkySet[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    return true;
}

} // namespace toy
