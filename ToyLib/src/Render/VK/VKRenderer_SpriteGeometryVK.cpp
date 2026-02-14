//======================================================================
// VKRenderer_DrawUI.cpp
//  - DrawUIPass(): RenderQueue の UI bucket を Vulkan で描画
//  - SpriteComponent が積んだ RenderItem(type=Sprite) を処理
//  - Sprite pipeline は CreateSpritePipeline() で生成済み前提
//
// NOTE:
//  - VulkanにはVAOは無いので、IRenderer::GetSpriteQuad() の頂点/インデックスを
//    VK用VB/IBに変換して使う（EnsureSpriteGeometryVK）
//  - VertexArray がCPU側の頂点/インデックスを保持していない場合は、
//    VertexArray に GetVertexData()/GetIndexData() 等を追加する必要あり。
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"

#include "Asset/Geometry/VertexArray.h"

#include <iostream>
#include <vector>
#include <cstring>

namespace toy {


bool VKRenderer::EnsureSpriteGeometryVK()
{
    if (!mDevice || !mPhysicalDevice)
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: device/phys missing.\n";
        return false;
    }

    auto quadShared = GetSpriteQuad();
    const VertexArray* quad = quadShared.get();

    if (!quad)
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: sprite quad is null.\n";
        return false;
    }

    // 既にキャッシュ済みならOK
    if (mSpriteGeoVK.find(quad) != mSpriteGeoVK.end())
    {
        return true;
    }

    // VertexArray が CPU 頂点/Index を保持していないと作れない
    if (!quad->HasCpuGeometry())
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: quad CPU geometry not available.\n";
        std::cerr << "  -> VertexArray needs CPU geometry accessors (HasCpuGeometry/GetVertexData/GetIndexData...)\n";
        return false;
    }

    const void*  vtx     = quad->GetVertexData();
    const void*  idx     = quad->GetIndexData();
    const size_t vtxSize = quad->GetVertexDataSizeBytes();
    const size_t idxSize = quad->GetIndexDataSizeBytes();
    const uint32_t indexCount = quad->GetIndexCount(); // uint32 で返す想定

    if (!vtx || !idx || vtxSize == 0 || idxSize == 0 || indexCount == 0)
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: invalid CPU buffers.\n";
        return false;
    }

    VKGeometry geo{};
    geo.vbBytes    = (VkDeviceSize)vtxSize;
    geo.ibBytes    = (VkDeviceSize)idxSize;
    geo.indexCount = indexCount;

    // HostVisible で最短（後で staging + deviceLocal に置き換え）
    if (!vkutil::CreateBuffer_HostVisible(
            mPhysicalDevice,
            mDevice,
            geo.vbBytes,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            geo.vb,
            geo.vbMem))
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: CreateBuffer_HostVisible(VB) failed.\n";
        return false;
    }

    if (!vkutil::CreateBuffer_HostVisible(
            mPhysicalDevice,
            mDevice,
            geo.ibBytes,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            geo.ib,
            geo.ibMem))
    {
        std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: CreateBuffer_HostVisible(IB) failed.\n";

        vkDestroyBuffer(mDevice, geo.vb, nullptr);
        vkFreeMemory(mDevice, geo.vbMem, nullptr);
        geo.vb = VK_NULL_HANDLE;
        geo.vbMem = VK_NULL_HANDLE;

        return false;
    }

    // Upload VB
    {
        void* p = nullptr;
        VkResult r = vkMapMemory(mDevice, geo.vbMem, 0, geo.vbBytes, 0, &p);
        if (r != VK_SUCCESS || !p)
        {
            std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: vkMapMemory(VB) failed: " << r << "\n";

            vkDestroyBuffer(mDevice, geo.ib, nullptr);
            vkFreeMemory(mDevice, geo.ibMem, nullptr);
            vkDestroyBuffer(mDevice, geo.vb, nullptr);
            vkFreeMemory(mDevice, geo.vbMem, nullptr);

            return false;
        }

        std::memcpy(p, vtx, (size_t)geo.vbBytes);
        vkUnmapMemory(mDevice, geo.vbMem);
    }

    // Upload IB
    {
        void* p = nullptr;
        VkResult r = vkMapMemory(mDevice, geo.ibMem, 0, geo.ibBytes, 0, &p);
        if (r != VK_SUCCESS || !p)
        {
            std::cerr << "[VKRenderer] EnsureSpriteGeometryVK: vkMapMemory(IB) failed: " << r << "\n";

            vkDestroyBuffer(mDevice, geo.ib, nullptr);
            vkFreeMemory(mDevice, geo.ibMem, nullptr);
            vkDestroyBuffer(mDevice, geo.vb, nullptr);
            vkFreeMemory(mDevice, geo.vbMem, nullptr);

            return false;
        }

        std::memcpy(p, idx, (size_t)geo.ibBytes);
        vkUnmapMemory(mDevice, geo.ibMem);
    }

    // Cache
    mSpriteGeoVK.emplace(quad, geo);
    return true;
}

//--------------------------------------------------------------
// Shutdown 用：Sprite geometry キャッシュを全破棄
//--------------------------------------------------------------
void VKRenderer::DestroySpriteGeometryVK()
{
    if (!mDevice)
    {
        mSpriteGeoVK.clear();
        return;
    }

    for (auto& kv : mSpriteGeoVK)
    {
        VKGeometry& g = kv.second;

        if (g.vb)
        {
            vkDestroyBuffer(mDevice, g.vb, nullptr);
            g.vb = VK_NULL_HANDLE;
        }
        if (g.vbMem)
        {
            vkFreeMemory(mDevice, g.vbMem, nullptr);
            g.vbMem = VK_NULL_HANDLE;
        }
        if (g.ib)
        {
            vkDestroyBuffer(mDevice, g.ib, nullptr);
            g.ib = VK_NULL_HANDLE;
        }
        if (g.ibMem)
        {
            vkFreeMemory(mDevice, g.ibMem, nullptr);
            g.ibMem = VK_NULL_HANDLE;
        }
    }

    mSpriteGeoVK.clear();
}

} // namespace toy
