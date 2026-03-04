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

#include <iostream>
#include <cstring>
#include <unordered_map>

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

static const char* NormalizePipelineNameForSet2(const char* pipelineName)
{
    if (!pipelineName) return nullptr;

    // ShadowSkinnedMesh は set=2 が SkinnedMesh と同じでOK（運用を安定化）
    if (std::strcmp(pipelineName, "ShadowSkinned") == 0)
    {
        return "ShadowSkinned";
    }

    // 将来、CW 版を作るならここも追加する
    // if (std::strcmp(pipelineName, "ShadowSkinnedMesh_CW") == 0) return "SkinnedMesh_CW";

    return pipelineName;
}

//==============================================================
// DescriptorPool (UBO用: Scene + Skinned)
//==============================================================
VkDescriptorSet VKRenderer::AcquireSkinnedSet(const Matrix4* palette,
                                              uint32_t paletteCount,
                                              const char* pipelineName)
{
    if (!mDevice || !mDescPool || !pipelineName)
    {
        return VK_NULL_HANDLE;
    }
    if (!palette || paletteCount == 0)
    {
        return VK_NULL_HANDLE;
    }
    if (paletteCount > kMaxPalette)
    {
        paletteCount = kMaxPalette;
    }

    const size_t frameCount = mFrames.size();
    if (frameCount == 0 || mFrameIndex >= frameCount)
    {
        return VK_NULL_HANDLE;
    }

    if (mSkinnedSlots.size() != frameCount)
    {
        mSkinnedSlots.resize(frameCount);
    }
    if (mSkinnedSlotCursor.size() != frameCount)
    {
        mSkinnedSlotCursor.resize(frameCount, 0);
    }

    const uint32_t idx = mSkinnedSlotCursor[mFrameIndex];
    mSkinnedSlotCursor[mFrameIndex]++;

    if (idx >= mSkinnedSlots[mFrameIndex].size())
    {
        SkinnedPaletteSlot slot{};

        if (!CreateBufferHostVisible(kSkinnedUBOSize,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     slot.ubo,
                                     slot.mem))
        {
            return VK_NULL_HANDLE;
        }

        const char* set2PipeName = NormalizePipelineNameForSet2(pipelineName);

        VkDescriptorSetLayout set2 = GetPipelineSetLayout(mPipelines, set2PipeName, 2);
        if (set2 == VK_NULL_HANDLE)
        {
            // 最後の保険（運用上 set=2 layout は SkinnedMesh と共通であるべき）
            set2 = GetPipelineSetLayout(mPipelines, "SkinnedMesh", 2);
        }

        if (set2 == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] AcquireSkinnedSet: set2 layout null pipe="
                      << pipelineName << " (normalized=" << (set2PipeName ? set2PipeName : "null") << ")\n";
            vkDestroyBuffer(mDevice, slot.ubo, nullptr);
            vkFreeMemory(mDevice, slot.mem, nullptr);
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set2;

        if (vkAllocateDescriptorSets(mDevice, &ai, &slot.set) != VK_SUCCESS ||
            slot.set == VK_NULL_HANDLE)
        {
            vkDestroyBuffer(mDevice, slot.ubo, nullptr);
            vkFreeMemory(mDevice, slot.mem, nullptr);
            return VK_NULL_HANDLE;
        }

        VkDescriptorBufferInfo bi{};
        bi.buffer = slot.ubo;
        bi.offset = 0;
        bi.range  = kSkinnedUBOSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = slot.set;
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

        mSkinnedSlots[mFrameIndex].push_back(slot);
    }

    Matrix4 tmp[kMaxPalette];
    for (uint32_t i = 0; i < kMaxPalette; ++i)
    {
        tmp[i] = Matrix4::Identity;
    }
    for (uint32_t i = 0; i < paletteCount; ++i)
    {
        tmp[i] = palette[i];
    }

    SkinnedPaletteSlot& s = mSkinnedSlots[mFrameIndex][idx];
    UploadToBuffer(s.mem, tmp, kSkinnedUBOSize);

    return s.set;
}

} // namespace toy
