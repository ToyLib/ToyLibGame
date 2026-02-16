//======================================================================
// VKRenderer_SpriteDescSetVK.cpp
//  - SpriteQueue(UI bucket) 用 DescriptorSet 管理
//  - Texture* 単位で「swapchain枚数分の VkDescriptorSet」をキャッシュ
//
// 前提:
//  - Sprite pipeline は set=0 binding=0 に CombinedImageSampler を要求
//  - VKRenderer.h 側に以下がある前提：
//      VkDescriptorPool mSpriteDescPool
//      std::unordered_map<const Texture*, std::vector<VkDescriptorSet>> mSpriteDescSetsVK
//      VkImageView mSpriteFallbackImageView
//      VkSampler   mSpriteFallbackSampler
//
// 注意:
//  - Texture -> VkImageView/VkSampler を引けない場合は fallback を使う
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Asset/Material/Texture.h"     // Texture 完全型
#include "Render/ITextureGPU.h"
#include "Render/VK/VKTextureGPU.h"     // VKTextureGPU の getter を使う
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKUBO.h"

#include <iostream>
#include <vector>

namespace toy
{

static Matrix4 MakeGLtoVK_ClipCorrection_RowVector()
{
    Matrix4 c = Matrix4::Identity;

    // x' = x
    c.mat[0][0] = 1.0f;

    // y' = y   ★ここを -1 にしない（viewport側で反転する）
    c.mat[1][1] = 1.0f;

    // z' = 0.5*z + 0.5*w  (GL [-1..1] -> VK [0..1])
    c.mat[2][2] = 0.5f;
    c.mat[3][2] = 0.5f;

    // w' = w
    c.mat[3][3] = 1.0f;

    return c;
}
static inline void WriteUBO(VkDevice dev, VkDeviceMemory mem, const void* src, size_t bytes)
{
    void* dst = nullptr;
    if (vkMapMemory(dev, mem, 0, bytes, 0, &dst) != VK_SUCCESS)
    {
        std::cerr << "[VK] vkMapMemory failed\n";
        return;
    }
    std::memcpy(dst, src, bytes);
    vkUnmapMemory(dev, mem);
}

//--------------------------------------------------------------
// Sprite用 DescriptorPool を lazily 作る
//--------------------------------------------------------------
bool VKRenderer::EnsureSpriteDescriptorPool()
{
    if (mSpriteDescPool != VK_NULL_HANDLE)
    {
        return true;
    }

    if (!mDevice)
    {
        return false;
    }

    const uint32_t scCount = (uint32_t)mSwapchainImageViews.size();
    if (scCount == 0)
    {
        std::cerr << "[VKRenderer] EnsureSpriteDescriptorPool: swapchain not ready.\n";
        return false;
    }

    // 「1 Texture = swapchain枚数分の set」を想定
    // 最小段階：とりあえず 256 Texture 分
    const uint32_t kMaxTextures   = 256;
    const uint32_t totalSets      = scCount * kMaxTextures;
    const uint32_t totalSamplers  = totalSets; // set あたり1 sampler

    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = totalSamplers;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.flags         = 0; // FREE を使うなら VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    pci.maxSets       = totalSets;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &ps;

    if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mSpriteDescPool) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] EnsureSpriteDescriptorPool: vkCreateDescriptorPool failed.\n";
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// TextureHandle -> VkImageView/VkSampler bridge
//
// 重要：Texture の public API は増やさない方針なので、Texture.h 側で
//   friend class VKRenderer;
// を追加して、VKRenderer だけが mGPU を覗ける前提。
//--------------------------------------------------------------
VkImageView VKRenderer::GetVkImageViewFromTextureHandle(TextureHandle h) const
{
    const Texture* tex = h.ptr;
    if (!tex)
    {
        return VK_NULL_HANDLE;
    }

    // Texture の GPU 実装を覗く（friend 前提）
    const ITextureGPU* gpu = tex->GetGPU();
    if (!gpu)
    {
        return VK_NULL_HANDLE;
    }

    // Vulkan 実装なら ImageView を返す
    if (auto* vkgpu = dynamic_cast<const VKTextureGPU*>(gpu))
    {
        return vkgpu->GetImageView();
    }

    return VK_NULL_HANDLE;
}

VkSampler VKRenderer::GetVkSamplerFromTextureHandle(TextureHandle h) const
{
    const Texture* tex = h.ptr;
    if (!tex)
    {
        return VK_NULL_HANDLE;
    }

    const ITextureGPU* gpu = tex->GetGPU();
    if (!gpu)
    {
        return VK_NULL_HANDLE;
    }

    if (auto* vkgpu = dynamic_cast<const VKTextureGPU*>(gpu))
    {
        return vkgpu->GetSampler();
    }

    return VK_NULL_HANDLE;
}

//--------------------------------------------------------------
// Sprite用 DescriptorSet を Texture* 単位でキャッシュ
// - swapchain枚数分をまとめて確保して、現在の mImageIndex の set を返す
//--------------------------------------------------------------
VkDescriptorSet VKRenderer::GetOrCreateSpriteDescSet(TextureHandle texH)
{
    if (!mDevice)
    {
        return VK_NULL_HANDLE;
    }

    const uint32_t scCount = (uint32_t)mSwapchainImageViews.size();
    if (scCount == 0)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: swapchain not ready.\n";
        return VK_NULL_HANDLE;
    }

    auto itPipe = mPipelines.find("Sprite");
    if (itPipe == mPipelines.end() || !itPipe->second)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: Sprite pipeline missing.\n";
        return VK_NULL_HANDLE;
    }

    VKPipeline* pipe = itPipe->second.get();
    if (pipe->setLayout0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: Sprite setLayout missing.\n";
        return VK_NULL_HANDLE;
    }

    if (!EnsureSpriteDescriptorPool())
    {
        return VK_NULL_HANDLE;
    }

    const Texture* texPtr = texH.ptr;

    // 今の view/sampler を解決（無ければ fallback）
    VkImageView view    = VK_NULL_HANDLE;
    VkSampler   sampler = VK_NULL_HANDLE;

    if (texPtr)
    {
        view    = GetVkImageViewFromTextureHandle(texH);
        sampler = GetVkSamplerFromTextureHandle(texH);
    }

    if (view == VK_NULL_HANDLE)    view = mSpriteFallbackImageView;
    if (sampler == VK_NULL_HANDLE) sampler = mSpriteFallbackSampler;

    if (view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: fallback imageView/sampler missing.\n";
        return VK_NULL_HANDLE;
    }

    // キャッシュ取得（無ければ作る）
    auto& entry = mSpriteDescSetsVK[texPtr];

    // 既存 sets があるか？
    const bool hasSets = (!entry.sets.empty());

    // swapchain 枚数が変わってたら作り直し（recreate対応の最低限）
    if (hasSets && entry.sets.size() != scCount)
    {
        entry.sets.clear();
        entry.lastView = VK_NULL_HANDLE;
        entry.lastSampler = VK_NULL_HANDLE;
    }

    // allocate が必要ならここで作る
    if (entry.sets.empty())
    {
        entry.sets.assign(scCount, VK_NULL_HANDLE);

        std::vector<VkDescriptorSetLayout> layouts(scCount, pipe->setLayout0);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mSpriteDescPool;
        ai.descriptorSetCount = scCount;
        ai.pSetLayouts        = layouts.data();

        const VkResult r = vkAllocateDescriptorSets(mDevice, &ai, entry.sets.data());
        if (r != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: vkAllocateDescriptorSets failed: " << r << "\n";
            entry.sets.clear();
            return VK_NULL_HANDLE;
        }

        // 初回は必ず update する
        entry.lastView    = VK_NULL_HANDLE;
        entry.lastSampler = VK_NULL_HANDLE;
    }

    // ★重要：view/sampler が変わっていたら descriptor を更新し直す
    if (entry.lastView != view || entry.lastSampler != sampler)
    {
        for (uint32_t i = 0; i < scCount; ++i)
        {
            VkDescriptorImageInfo ii{};
            ii.sampler     = sampler;
            ii.imageView   = view;
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = entry.sets[i];
            w.dstBinding      = 0;
            w.descriptorCount = 1;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo      = &ii;

            vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
        }

        entry.lastView    = view;
        entry.lastSampler = sampler;
    }

    const uint32_t idx = (mImageIndex < (uint32_t)entry.sets.size()) ? mImageIndex : 0;
    return entry.sets[idx];
}


bool VKRenderer::EnsureSpriteCommonDescriptors()
{
    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0) return false;
    if (mSpriteSetLayout1_Common == VK_NULL_HANDLE) return false;

    auto ready = [&]() -> bool
    {
        if (mSpriteCommonDescPool == VK_NULL_HANDLE) return false;
        if (mSpriteFrames.size() != imageCount) return false;

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            const auto& f = mSpriteFrames[i];
            if (f.descSet1_SpriteCommon == VK_NULL_HANDLE) return false;
            if (f.spriteCommonUBO == VK_NULL_HANDLE || f.spriteCommonMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready()) return true;

    DestroySpriteCommonDescriptors();

    // pool: UBO * 1 * imageCount
    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = imageCount * 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets       = imageCount;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &ps;

    if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mSpriteCommonDescPool) != VK_SUCCESS)
    {
        std::cerr << "[VK] sprite common desc pool create failed\n";
        return false;
    }

    // allocate set=1 per image
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mSpriteSetLayout1_Common);
    std::vector<VkDescriptorSet> sets(imageCount, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mSpriteCommonDescPool;
    ai.descriptorSetCount = imageCount;
    ai.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(mDevice, &ai, sets.data()) != VK_SUCCESS)
    {
        std::cerr << "[VK] sprite common desc set alloc failed\n";
        DestroySpriteCommonDescriptors();
        return false;
    }

    mSpriteFrames.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto& f = mSpriteFrames[i];
        f.descSet1_SpriteCommon = sets[i];

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_SpriteCommon),
                                  f.spriteCommonUBO, f.spriteCommonMem))
        {
            DestroySpriteCommonDescriptors();
            return false;
        }

        // binding=0
        vkutil::WriteDesc_UBO(mDevice, f.descSet1_SpriteCommon, 0,
                              f.spriteCommonUBO, sizeof(UBO_SpriteCommon));
    }

    // init
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        UpdateSpriteCommonUBO(i);
    }

    return true;
}

void VKRenderer::DestroySpriteCommonDescriptors()
{
    auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(mDevice, b, nullptr);
        if (m) vkFreeMemory(mDevice, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    for (auto& f : mSpriteFrames)
    {
        destroyBuf(f.spriteCommonUBO, f.spriteCommonMem);
        f.descSet1_SpriteCommon = VK_NULL_HANDLE;
    }
    mSpriteFrames.clear();

    if (mSpriteCommonDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mSpriteCommonDescPool, nullptr);
        mSpriteCommonDescPool = VK_NULL_HANDLE;
    }

    // mSpriteSetLayout1_Common は “共有” なので、renderer shutdownでまとめて破棄でもOK
}

void VKRenderer::UpdateSpriteCommonUBO(uint32_t imageIndex)
{
    if (mSpriteFrames.empty() || imageIndex >= (uint32_t)mSpriteFrames.size()) return;

    VkDeviceMemory mem = mSpriteFrames[imageIndex].spriteCommonMem;
    if (mem == VK_NULL_HANDLE) return;

    // UIスケール（SpriteComponentと同じ値で viewProj を作る）
    const UIScaleInfo ui = GetUIScaleInfo();
    const float sw = ui.screenW;
    const float sh = ui.screenH;

    UBO_SpriteCommon u{};

    // GL的な2D viewProj を作って、ZだけVK向け補正（あなたの corr が Z補正のみの前提）
    const Matrix4 vpGL = Matrix4::CreateSimpleViewProj(sw, sh);
    const Matrix4 corr = MakeGLtoVK_ClipCorrection_RowVector();
    u.uViewProj = vpGL * corr;

    WriteUBO(mDevice, mem, &u, sizeof(u));
}
} // namespace toy
