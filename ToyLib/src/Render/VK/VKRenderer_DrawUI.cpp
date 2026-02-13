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

namespace
{
    //==========================================================
    // PushConstants（Sprite.vert/frag）
    //  - mat4 world
    //  - mat4 viewProj
    //  - vec4 colorAlpha (rgb + a)
    //
    // total: 64 + 64 + 16 = 144 bytes
    //==========================================================
    struct SpritePush
    {
        float world[16];
        float viewProj[16];
        float colorAlpha[4];
    };

    static void CopyMatrixToFloat16(const Matrix4& m, float out16[16])
    {
        // Matrix4 が 16float 連続の前提（ToyLibのMathUtilに合わせて）
        std::memcpy(out16, &m, sizeof(float) * 16);
    }
}

//--------------------------------------------------------------
// VKRenderer::DrawUIPass（RenderQueueのUIスプライトを描く）
//--------------------------------------------------------------
void VKRenderer::DrawUIPass()
{
    
    // BeginFrame() で vkCmdBeginRenderPass 済み前提
    if (!mDevice || mFrames.empty())
    {
        return;
    }

    FrameSync& frame = mFrames[mFrameIndex];
    if (!frame.cmd)
    {
        return;
    }

    if (mRenderPass == VK_NULL_HANDLE || mFramebuffers.empty())
    {
        return;
    }

    // UI bucket に何も無いなら何もしない
    if (mBuckets.ui.empty())
    {
        return;
    }

    // Sprite pipeline を取得
    auto itPipe = mPipelines.find("Sprite");
    if (itPipe == mPipelines.end() || !itPipe->second)
    {
        // まだ CreateSpritePipeline() してない
        return;
    }

    VKPipeline* pipe = itPipe->second.get();
    if (pipe->pipeline == VK_NULL_HANDLE || pipe->pipelineLayout == VK_NULL_HANDLE)
    {
        return;
    }

    // UI resources（sampler / white fallback texture）を用意
    // ここが無いと descriptor set 作れない
    if (!EnsureUIResources())
    {
        return;
    }

    // SpriteQuad（IRenderer共通ジオメトリ）をVKに用意
    if (!EnsureSpriteGeometryVK())
    {
        return;
    }

    // SpriteQuad の VB/IB を取り出す
    VertexArray* quad = GetSpriteQuad().get();
    auto itGeo = mSpriteGeoVK.find(quad);
    if (itGeo == mSpriteGeoVK.end())
    {
        return;
    }

    const VKGeometry& geo = itGeo->second;
    if (geo.vb == VK_NULL_HANDLE || geo.ib == VK_NULL_HANDLE || geo.indexCount == 0)
    {
        return;
    }

    // viewport/scissor（dynamic）
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = (float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = mSwapchainExtent;

    vkCmdSetViewport(frame.cmd, 0, 1, &vp);
    vkCmdSetScissor(frame.cmd, 0, 1, &sc);

    // pipeline bind
    vkCmdBindPipeline(frame.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

    // VB/IB bind（SpriteQuad固定）
    VkDeviceSize vbOff = 0;
    vkCmdBindVertexBuffers(frame.cmd, 0, 1, &geo.vb, &vbOff);
    vkCmdBindIndexBuffer(frame.cmd, geo.ib, 0, VK_INDEX_TYPE_UINT32);

    // RenderQueue items
    const auto& items = mRenderQueue.Items();

    for (uint32_t itemIndex : mBuckets.ui)
    {
        if (itemIndex >= items.size())
        {
            continue;
        }

        const RenderItem& it = items[itemIndex];

        // UI bucket には Sprite 以外が混ざる可能性もあるのでフィルタ
        if (it.type != RenderItemType::Sprite)
        {
            continue;
        }

        // texture → descriptor set（swapchain枚数分のうち mImageIndex を返す）
        VkDescriptorSet set0 = GetOrCreateSpriteDescSet(it.texture);
        if (set0 == VK_NULL_HANDLE)
        {
            continue;
        }

        vkCmdBindDescriptorSets(frame.cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipe->pipelineLayout,
                                0, 1, &set0,
                                0, nullptr);

        //------------------------------------------------------
        // PushConstants: world / viewProj / colorAlpha
        //
        // Shader は「row-vector (pos * world * viewProj)」想定なので
        // C++側でも it.world / it.viewProj をそのまま渡す。
        //
        // useMVP の場合：
        //   it.mvp = world*viewProj を事前計算した互換用として扱い、
        //   ここでは world=I, viewProj=it.mvp に寄せる（最小互換）。
        //------------------------------------------------------
        SpritePush pc{};

        if (it.useMVP)
        {
            // MVP互換：world=Identity, viewProj=MVP
            CopyMatrixToFloat16(Matrix4::Identity, pc.world);
            CopyMatrixToFloat16(it.mvp, pc.viewProj);
        }
        else
        {
            CopyMatrixToFloat16(it.world, pc.world);
            CopyMatrixToFloat16(it.viewProj, pc.viewProj);
        }

        pc.colorAlpha[0] = it.color.x;
        pc.colorAlpha[1] = it.color.y;
        pc.colorAlpha[2] = it.color.z;
        pc.colorAlpha[3] = it.alpha;

        vkCmdPushConstants(frame.cmd,
                           pipe->pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, (uint32_t)sizeof(SpritePush), &pc);

        // indexCount:
        // - SpriteQuad固定なら geo.indexCount でOK
        // - RenderItem側で indexCount が入っているならそれを優先しても良い
        const uint32_t indexCount =
            (it.indexCount > 0) ? (uint32_t)it.indexCount : geo.indexCount;

        vkCmdDrawIndexed(frame.cmd, indexCount, 1, 0, 0, 0);

        AddDrawCall();
    }
}

//--------------------------------------------------------------
// VKRenderer::EnsureSpriteGeometryVK
//  - IRenderer::GetSpriteQuad() の CPU頂点/Index から VK VB/IB を作る
//  - 最短: HostVisible + Coherent
//--------------------------------------------------------------
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
