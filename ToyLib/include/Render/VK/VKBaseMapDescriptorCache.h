//======================================================================
// Render/VK/VKBaseMapDescriptorCache.h
//
// BaseMap(set=1, テクスチャ用CombinedImageSampler)のDescriptorSetを
// テクスチャ単位でキャッシュし、Poolが枯れたら増設する。
// あわせて1x1 whiteのfallbackテクスチャ/Setも管理する。
// VKRendererから「テクスチャ→DescriptorSet」の管理責務を切り出したもの。
//======================================================================
#pragma once

#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace toy
{

class Texture;
class VKPipelineLibrary;

class VKBaseMapDescriptorCache
{
public:
    void Init(VkDevice device, VkPhysicalDevice phys);

    // テクスチャ単位でキャッシュされたBaseMap(set=1) DescriptorSetを返す。
    // tex==nullptr の場合は fallback(1x1 white) のセットを返す。
    VkDescriptorSet GetOrCreateBaseMapSet(VKPipelineLibrary& pipelines, const Texture* tex, const char* pipelineName);

    // テクスチャ破棄時にキャッシュから該当エントリのみ除去する。
    void RemoveTexture(const Texture* tex);

    // pool/cache/fallbackSetByPipe 一式を破棄する（DescriptorPool破棄前に呼ぶこと）。
    // fallbackの1x1 whiteテクスチャ自体は DestroyFallbackWhiteTexture() で別途破棄する。
    void Clear();

    // 1x1 white の fallback テクスチャを用意する。
    // beginOneTime/endOneTime でコマンド記録の開始/終了をVKRenderer側から注入してもらう
    // （one-time command の発行主体はVKRenderer側に残すため）。
    bool CreateFallbackWhiteTexture(std::function<VkCommandBuffer()> beginOneTime,
                                    std::function<void(VkCommandBuffer)> endOneTime);
    void DestroyFallbackWhiteTexture();

    bool CreateFallbackBaseMapSet(VKPipelineLibrary& pipelines, const char* pipelineName);

private:
    struct BaseMapKey
    {
        // テクスチャ破棄時は RemoveTexture() で個別に、それ以外は Clear() で一括解放する。
        const Texture* tex = nullptr;
        std::string pipelineName;

        bool operator==(const BaseMapKey& o) const
        {
            return tex == o.tex && pipelineName == o.pipelineName;
        }
    };

    struct BaseMapKeyHash
    {
        size_t operator()(const BaseMapKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v)
            {
                h ^= v;
                h *= 1099511628211ull;
            };

            mix(std::hash<const void*>{}(k.tex));
            mix(std::hash<std::string>{}(k.pipelineName));
            return h;
        }
    };

    struct CachedDescriptorSet
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    VkDescriptorPool CreatePool(uint32_t maxSets, uint32_t samplerCount);
    VkDescriptorPool GetActivePool();
    VkDescriptorPool GrowPoolAndGet();

    VkDevice mDevice{VK_NULL_HANDLE};
    VkPhysicalDevice mPhysicalDevice{VK_NULL_HANDLE};

    // "専用DescriptorPoolを増設"して枯れを回避する運用。個別freeはしない。
    std::vector<VkDescriptorPool> mPools;
    uint32_t mPoolCursor = 0;

    std::unordered_map<BaseMapKey, CachedDescriptorSet, BaseMapKeyHash> mSetCache;

    // Fallback 1x1 white texture
    VkImage mFallbackWhiteImg{VK_NULL_HANDLE};
    VkDeviceMemory mFallbackWhiteMem{VK_NULL_HANDLE};
    VkImageView mFallbackWhiteView{VK_NULL_HANDLE};
    VkSampler mFallbackWhiteSampler{VK_NULL_HANDLE};

    // pipeline毎のfallback DS（pool は mPools 側が保持）
    std::unordered_map<std::string, CachedDescriptorSet> mFallbackSetByPipe;
};

} // namespace toy
