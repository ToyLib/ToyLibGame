// Render/VK/VKRenderer_WorldDescriptors.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"
#include "Render/LightingManager.h"
#include "Graphics/Light/PointLightComponent.h"

#include "Render/RenderQueue.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Engine/Core/Application.h"

#include <iostream>
#include <cstring>

namespace toy
{

//============================================================
// UBO structs (std140 揃え)
//============================================================

struct alignas(16) UBO_WorldCommon
{
    Matrix4 uViewProj;

    Vector3 uCameraPos;     float _pad0 = 0.0f;
    Vector3 uAmbientLight;  float _pad1 = 0.0f;

    float   uFogMaxDist = 1000.0f;
    float   uFogMinDist = 0.00001f;
    float   _pad2[2]    = { 0.0f, 0.0f };

    Vector3 uFogColor; float _pad3 = 0.0f;

    // シャドウ未使用だが、shader側構造に合わせて残す
    Matrix4 uLightViewProj0;
    Matrix4 uLightViewProj1;

    float uCascadeSplit0 = 0.0f;
    float uCascadeBlend  = 0.0f;
    float uShadowBias    = 0.0f;

    int   uUseShadow = 0;
    int   uUseToon   = 0;
    float _pad4 = 0.0f;
};

struct alignas(16) UBO_MaterialParams
{
    Vector3 uDiffuseColor;  int uUseTexture = 0;
    Vector3 uUniformColor;  int uOverrideColor = 0;

    float uSpecPower = 1.0f;   // ★ 固定値
    float _pad0 = 0.0f;
    float _pad1 = 0.0f;
    float _pad2 = 0.0f;
};

struct alignas(16) UBO_DirLight
{
    Vector3 mDirection;     float _pad0 = 0.0f;
    Vector3 mDiffuseColor;  float _pad1 = 0.0f;
    Vector3 mSpecColor;     float _pad2 = 0.0f;
};

struct alignas(16) UBO_PointLight
{
    Vector3 position; float intensity = 0.0f;
    Vector3 color;    float constant  = 1.0f;

    float linear    = 0.0f;
    float quadratic = 0.0f;
    float radius    = 1.0f;
    float _pad      = 0.0f;
};

struct alignas(16) UBO_PointLightBlock
{
    int uNumPointLights = 0;
    int _padA = 0;
    int _padB = 0;
    int _padC = 0;

    UBO_PointLight uPointLights[8];
};

//============================================================
// Local helper
//============================================================

static bool CreateHostVisibleUBO(
    VkPhysicalDevice phys,
    VkDevice device,
    VkDeviceSize sizeBytes,
    VkBuffer& outBuf,
    VkDeviceMemory& outMem)
{
    return vkutil::CreateBuffer_HostVisible(
        phys,
        device,
        sizeBytes,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        outBuf,
        outMem);
}

//============================================================
// EnsureWorldDescriptors
//============================================================
inline void WriteDesc_CombinedImageSampler(VkDevice device,
                                          VkDescriptorSet set,
                                          uint32_t binding,
                                          VkImageView view,
                                          VkSampler sampler,
                                          VkImageLayout layout)
{
    VkDescriptorImageInfo ii{};
    ii.imageView   = view;
    ii.sampler     = sampler;
    ii.imageLayout = layout;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = set;
    w.dstBinding      = binding;
    w.dstArrayElement = 0;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
}
bool VKRenderer::EnsureWorldDescriptors()
{
    if (mWorldDescPool != VK_NULL_HANDLE && !mWorldDescSets.empty())
    {
        return true;
    }

    auto it = mPipelines.find("Mesh");
    if (it == mPipelines.end() || !it->second)
    {
        std::cerr << "[VKRenderer] Mesh pipeline missing\n";
        return false;
    }

    VKPipeline* meshPipe = it->second.get();
    if (meshPipe->setLayout1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] Mesh setLayout1 is null\n";
        return false;
    }

    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0)
    {
        std::cerr << "[VKRenderer] swapchain not ready\n";
        return false;
    }

    // ------------------------------------------------------------
    // UBO buffers（あなたの現状：1本ずつ・mappedは別管理 or Update関数内で map）
    // ------------------------------------------------------------
    if (mWorldCommonUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(
                mPhysicalDevice, mDevice,
                sizeof(UBO_WorldCommon),
                mWorldCommonUBO,
                mWorldCommonUBOMem))
        {
            return false;
        }
    }

    if (mMaterialParamsUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(
                mPhysicalDevice, mDevice,
                sizeof(UBO_MaterialParams),
                mMaterialParamsUBO,
                mMaterialParamsUBOMem))
        {
            return false;
        }
    }

    if (mDirLightUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(
                mPhysicalDevice, mDevice,
                sizeof(UBO_DirLight),
                mDirLightUBO,
                mDirLightUBOMem))
        {
            return false;
        }
    }

    if (mPointLightUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(
                mPhysicalDevice, mDevice,
                sizeof(UBO_PointLightBlock),
                mPointLightUBO,
                mPointLightUBOMem))
        {
            return false;
        }
    }

    // ------------------------------------------------------------
    // Descriptor pool（UBO x4 / set = imageCount）
    // ------------------------------------------------------------
    VkDescriptorPoolSize poolUBO{};
    poolUBO.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolUBO.descriptorCount = imageCount * 4;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets       = imageCount;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &poolUBO;

    if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mWorldDescPool) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateDescriptorPool failed\n";
        return false;
    }

    // ------------------------------------------------------------
    // Allocate sets（layout = setLayout1）
    // ------------------------------------------------------------
    mWorldDescSets.resize(imageCount);

    std::vector<VkDescriptorSetLayout> layouts(imageCount, meshPipe->setLayout1);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mWorldDescPool;
    ai.descriptorSetCount = imageCount;
    ai.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(mDevice, &ai, mWorldDescSets.data()) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets failed\n";
        return false;
    }

    // ------------------------------------------------------------
    // Update sets：binding 0..3 を必ず全部書く ★ここが本命
    // ------------------------------------------------------------
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkDescriptorSet set = mWorldDescSets[i];

        vkutil::WriteDesc_UBO(mDevice, set, 0, mWorldCommonUBO,    sizeof(UBO_WorldCommon));
        vkutil::WriteDesc_UBO(mDevice, set, 1, mMaterialParamsUBO, sizeof(UBO_MaterialParams));
        vkutil::WriteDesc_UBO(mDevice, set, 2, mDirLightUBO,       sizeof(UBO_DirLight));
        vkutil::WriteDesc_UBO(mDevice, set, 3, mPointLightUBO,     sizeof(UBO_PointLightBlock));
    }

    // 初期更新（あなたの Update 関数が map/unmap するならそれでOK）
    UpdateWorldCommonUBO(0);
    UpdateDirLightUBO();
    UpdatePointLightUBO();

    // もし MaterialParams も別関数なら呼ぶ（なければ UpdateMaterialParamsUBO を後で追加）
    // UpdateMaterialParamsUBO();

    return true;
}
//============================================================
// Destroy
//============================================================

void VKRenderer::DestroyWorldDescriptors()
{
    mWorldDescSets.clear();

    if (mWorldDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mWorldDescPool, nullptr);
        mWorldDescPool = VK_NULL_HANDLE;
    }

    auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(mDevice, b, nullptr);
        if (m) vkFreeMemory(mDevice, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    destroyBuf(mWorldCommonUBO,    mWorldCommonUBOMem);
    destroyBuf(mMaterialParamsUBO, mMaterialParamsUBOMem);
    destroyBuf(mDirLightUBO,       mDirLightUBOMem);
    destroyBuf(mPointLightUBO,     mPointLightUBOMem);
}

//============================================================
// UBO Updates
//============================================================

void VKRenderer::UpdateWorldCommonUBO(uint32_t /*imageIndex*/)
{
    UBO_WorldCommon ubo{};

    ubo.uViewProj = GetViewProjMatrix();
    ubo.uCameraPos = GetCameraPosition();

    const auto lm = GetLightingManager();
    if (lm)
    {
        ubo.uAmbientLight = lm->GetAmbientColor();

        const FogInfo fog = lm->GetFogInfo();
        ubo.uFogMaxDist = fog.MaxDist;
        ubo.uFogMinDist = fog.MinDist;
        ubo.uFogColor   = fog.Color;
    }
    else
    {
        ubo.uAmbientLight = Vector3(0.8f, 0.8f, 0.8f);
        ubo.uFogColor     = Vector3(0.5f, 0.5f, 0.5f);
    }

    ubo.uUseShadow = 0;
    ubo.uUseToon   = 0;

    ubo.uLightViewProj0 = Matrix4::Identity;
    ubo.uLightViewProj1 = Matrix4::Identity;

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mWorldCommonUBOMem, 0, sizeof(UBO_WorldCommon), 0, &mapped) == VK_SUCCESS)
    {
        std::memcpy(mapped, &ubo, sizeof(UBO_WorldCommon));
        vkUnmapMemory(mDevice, mWorldCommonUBOMem);
    }
}

void VKRenderer::UpdateMaterialParamsUBO(const RenderItem& it)
{
    UBO_MaterialParams ubo{};

    ubo.uSpecPower = 1.0f;  // ★ 常に固定

    const Material* mat =
        (it.material.IsValid() ? it.material.ptr : nullptr);

    if (it.overrideColor)
    {
        ubo.uOverrideColor = 1;
        ubo.uUniformColor  = it.overrideColorValue;
    }
    else if (mat)
    {
        ubo.uDiffuseColor = mat->GetDiffuseColor();
        ubo.uUseTexture   = mat->UseTextureFinal() ? 1 : 0;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mMaterialParamsUBOMem, 0, sizeof(UBO_MaterialParams), 0, &mapped) == VK_SUCCESS)
    {
        std::memcpy(mapped, &ubo, sizeof(UBO_MaterialParams));
        vkUnmapMemory(mDevice, mMaterialParamsUBOMem);
    }
}

void VKRenderer::UpdateDirLightUBO()
{
    UBO_DirLight ubo{};

    const auto lm = GetLightingManager();
    if (lm)
    {
        const DirectionalLight& dl = lm->GetDirectionalLight();

        ubo.mDirection    = dl.GetDirection();
        ubo.mDiffuseColor = dl.DiffuseColor;
        ubo.mSpecColor    = dl.SpecColor;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mDirLightUBOMem, 0, sizeof(UBO_DirLight), 0, &mapped) == VK_SUCCESS)
    {
        std::memcpy(mapped, &ubo, sizeof(UBO_DirLight));
        vkUnmapMemory(mDevice, mDirLightUBOMem);
    }
}

void VKRenderer::UpdatePointLightUBO()
{
    UBO_PointLightBlock blk{};

    const auto lm = GetLightingManager();
    if (lm)
    {
        const auto& pls = lm->GetPointLights();

        int outCount = 0;

        for (size_t i = 0; i < pls.size() && outCount < 8; ++i)
        {
            const PointLightComponent* p = pls[i];
            if (!p || !p->IsEnabled()) continue;

            auto& dst = blk.uPointLights[outCount];

            dst.position  = p->GetPosition();
            dst.color     = p->GetColor();
            dst.intensity = p->GetIntensity();
            dst.constant  = p->GetConstant();
            dst.linear    = p->GetLinear();
            dst.quadratic = p->GetQuadratic();
            dst.radius    = p->GetRadius();

            ++outCount;
        }

        blk.uNumPointLights = outCount;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mPointLightUBOMem, 0, sizeof(UBO_PointLightBlock), 0, &mapped) == VK_SUCCESS)
    {
        std::memcpy(mapped, &blk, sizeof(UBO_PointLightBlock));
        vkUnmapMemory(mDevice, mPointLightUBOMem);
    }
}

} // namespace toy
