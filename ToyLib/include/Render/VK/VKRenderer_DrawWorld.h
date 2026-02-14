#pragma once

#include <vulkan/vulkan.h>
#include "Render/RenderItem.h"

namespace toy
{

class VKPipeline;

class VKRenderer
{
private:

    //=========================================================
    // Push Constants
    //=========================================================
    struct WorldPush
    {
        Matrix4 world;
    };

    //=========================================================
    // Per-frame UBO (set=0)
    //=========================================================
    struct alignas(16) WorldCommonUBO
    {
        Matrix4 viewProj;
        Matrix4 lightViewProj0;
        Matrix4 lightViewProj1;

        alignas(16) Vector3 cameraPos;   float _pad0;
        alignas(16) Vector3 ambientLight;float _pad1;

        alignas(16) Vector3 dirLightDir; float _pad2;
        alignas(16) Vector3 dirDiff;     float _pad3;
        alignas(16) Vector3 dirSpec;     float _pad4;

        float fogMaxDist;
        float fogMinDist;
        float _pad5[2];
        alignas(16) Vector3 fogColor; float _pad6;

        float shadowBias;
        int   useToon;
        float cascadeSplit0;
        float cascadeBlend;
    };

    //=========================================================
    // Material UBO (set=1)
    //=========================================================
    struct alignas(16) MaterialUBO
    {
        alignas(16) Vector3 diffuseColor; float _pad0;
        int useTexture;
        int overrideColor;
        float specPower;
        float _pad1;
        alignas(16) Vector3 overrideColorValue; float _pad2;
    };

private:

    //=========================================================
    // World Pass
    //=========================================================
    void DrawWorldPass() override;

    void DrawBucket_World(const std::vector<uint32_t>& bucket);

    void BindWorldCommon(VkCommandBuffer cmd,
                         const RenderItem& it,
                         VKPipeline* pipe);

    void BindWorldMaterial(VkCommandBuffer cmd,
                           const RenderItem& it,
                           VKPipeline* pipe);

    void BindGeometry(VkCommandBuffer cmd,
                      const RenderItem& it);

private:

    //=========================================================
    // Per-frame resources
    //=========================================================
    std::vector<VkBuffer>       mWorldCommonUBO;
    std::vector<VkDeviceMemory> mWorldCommonUBOMem;
    std::vector<void*>          mWorldCommonMapped;
    std::vector<VkDescriptorSet> mWorldCommonDescSets;

    std::vector<VkBuffer>       mMaterialUBO;
    std::vector<VkDeviceMemory> mMaterialUBOMem;
    std::vector<void*>          mMaterialMapped;

    // layout
    VkDescriptorSetLayout mWorldSetLayout0 { VK_NULL_HANDLE };
    VkDescriptorSetLayout mWorldSetLayout1 { VK_NULL_HANDLE };
};

}
