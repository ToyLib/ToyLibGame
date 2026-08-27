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

#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/Texture.h"
#include "Render/LightingManager.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKTextureGPU.h"
#include "Render/VK/VKUtil.h"

#include <cstring>
#include <iostream>
#include <unordered_map>

namespace toy
{

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

static const char* NormalizePipelineNameForSet2(const char* pipelineName)
{
    if (!pipelineName)
    {
        return nullptr;
    }

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
VkDescriptorSet VKRenderer::AcquireSkinnedSet(const Matrix4* palette, uint32_t paletteCount, const char* pipelineName)
{
    if (!mDevice || !mDescPool || !pipelineName)
    {
        return VK_NULL_HANDLE;
    }
    if (!palette || paletteCount == 0)
    {
        return VK_NULL_HANDLE;
    }
    if (paletteCount > kPaletteSanityCap)
    {
        // シェーダ側は runtime-sized array なので設計上の上限は無いが、
        // 壊れたデータで無制限に確保してしまうのを防ぐための安全弁。
        static bool s_warned = false;
        if (!s_warned)
        {
            std::cerr << "[VK] AcquireSkinnedSet: paletteCount=" << paletteCount
                      << " exceeds sanity cap (" << kPaletteSanityCap << "), clamped. Data may be corrupted.\n";
            s_warned = true;
        }
        paletteCount = kPaletteSanityCap;
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
    // ★ カーソルは確保成功後にインクリメント（失敗時の early return でずれないように）

    const bool isNewSlot = (idx >= mSkinnedSlots[mFrameIndex].size());
    if (isNewSlot)
    {
        mSkinnedSlots[mFrameIndex].push_back(SkinnedPaletteSlot{});
    }

    SkinnedPaletteSlot& s = mSkinnedSlots[mFrameIndex][idx];

    const VkDeviceSize requiredBytes = sizeof(float) * 16 * static_cast<VkDeviceSize>(paletteCount);

    // 新規スロット、または前回よりボーン数が増えた場合だけバッファを（再）確保する。
    // ★同一frameIndexのスロットはBeginFrame()のfence waitでGPU使用完了が保証された後に
    //   触るので、ここでバッファを破棄・差し替えても安全。
    if (isNewSlot || paletteCount > s.capacity)
    {
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
        s.capacity = 0;

        if (!CreateBufferHostVisible(requiredBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s.ubo, s.mem))
        {
            return VK_NULL_HANDLE;
        }
        s.capacity = paletteCount;

        // descriptor set 自体は初回のみ確保し、以降は使い回す
        if (s.set == VK_NULL_HANDLE)
        {
            const char* set2PipeName = NormalizePipelineNameForSet2(pipelineName);

            VkDescriptorSetLayout set2 = GetPipelineSetLayout(mPipelines, set2PipeName, 2);
            if (set2 == VK_NULL_HANDLE)
            {
                // 最後の保険（運用上 set=2 layout は SkinnedMesh と共通であるべき）
                set2 = GetPipelineSetLayout(mPipelines, "SkinnedMesh", 2);
            }

            if (set2 == VK_NULL_HANDLE)
            {
                std::cerr << "[VK] AcquireSkinnedSet: set2 layout null pipe=" << pipelineName
                          << " (normalized=" << (set2PipeName ? set2PipeName : "null") << ")\n";
                vkDestroyBuffer(mDevice, s.ubo, nullptr);
                vkFreeMemory(mDevice, s.mem, nullptr);
                s.ubo = VK_NULL_HANDLE;
                s.mem = VK_NULL_HANDLE;
                s.capacity = 0;
                return VK_NULL_HANDLE;
            }

            VkDescriptorSetAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool = mDescPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &set2;

            if (vkAllocateDescriptorSets(mDevice, &ai, &s.set) != VK_SUCCESS || s.set == VK_NULL_HANDLE)
            {
                vkDestroyBuffer(mDevice, s.ubo, nullptr);
                vkFreeMemory(mDevice, s.mem, nullptr);
                s.ubo = VK_NULL_HANDLE;
                s.mem = VK_NULL_HANDLE;
                s.capacity = 0;
                s.set = VK_NULL_HANDLE;
                return VK_NULL_HANDLE;
            }
        }

        // バッファの実体が変わった（新規 or 差し替え）ので descriptor を書き直す
        VkDescriptorBufferInfo bi{};
        bi.buffer = s.ubo;
        bi.offset = 0;
        bi.range = requiredBytes;

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = s.set;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &bi;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    // 確保成功後にインクリメント（失敗時の early return でずれないように）
    mSkinnedSlotCursor[mFrameIndex]++;

    UploadToBuffer(s.mem, palette, requiredBytes);

    return s.set;
}

} // namespace toy
