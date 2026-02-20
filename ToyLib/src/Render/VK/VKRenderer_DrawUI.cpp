//======================================================================
// VKRenderer_DrawUI.cpp
//  - DrawUIPass(): RenderQueue の UI bucket を Vulkan で描画
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
    static void CopyMatrixToFloat16(const Matrix4& m, float out16[16])
    {
        std::memcpy(out16, &m, sizeof(float) * 16);
    }
}

//--------------------------------------------------------------
// VKRenderer::DrawUIPass
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
        return;
    }

    VKPipeline* pipe = itPipe->second.get();
    if (pipe->pipeline == VK_NULL_HANDLE || pipe->pipelineLayout == VK_NULL_HANDLE)
    {
        return;
    }

    // SpriteQuad（IRenderer共通ジオメトリ）をVKに用意
    if (!EnsureSpriteGeometryVK())
    {
        return;
    }

    // SpriteCommon(set=1) を用意（viewProj UBO）
    if (!EnsureSpriteCommonDescriptors())
    {
        return;
    }

    // 毎フレーム安全側で更新（最適化は後でOK）
    UpdateSpriteCommonUBO(mImageIndex);

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
    vp.y        = (float)mSwapchainExtent.height;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = -(float)mSwapchainExtent.height;
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

    //======================================================
    // set=1 (SpriteCommon) を取得（per-image）
    //======================================================
    const uint32_t spriteFrameIdx =
        (mImageIndex < (uint32_t)mSpriteFrames.size()) ? mImageIndex : 0;

    VkDescriptorSet set1 = mSpriteFrames[spriteFrameIdx].descSet1_SpriteCommon;
    if (set1 == VK_NULL_HANDLE)
    {
        return;
    }

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

        // set=0 : texture sampler
        VkDescriptorSet set0 = GetOrCreateSpriteDescSet(it.texture);
        if (set0 == VK_NULL_HANDLE)
        {
            continue;
        }

        //======================================================
        // ★ set0 + set1 をまとめて bind（安全側）
        //======================================================
        VkDescriptorSet sets[2] = { set0, set1 };
        vkCmdBindDescriptorSets(frame.cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipe->pipelineLayout,
                                0, 2, sets,
                                0, nullptr);

        //------------------------------------------------------
        // PushConstants: world + colorAlpha（80 bytes）
        //------------------------------------------------------
        SpritePush pc{};
        CopyMatrixToFloat16(it.world, pc.world);

        //======================================================
        // ★ SpritePayload を常に反映（0も有効値の可能性があるため）
        //======================================================
        //Vector3 col = it.color;
        //float   alp = it.alpha;

        {
            const SpritePayload& sp = mRenderQueue.GetSpritePayload(it.payloadIndex);
            //col = sp.color;
            //alp = sp.alpha;
        }

        //pc.colorAlpha[0] = col.x;
        //pc.colorAlpha[1] = col.y;
        //pc.colorAlpha[2] = col.z;
        //pc.colorAlpha[3] = alp;

        vkCmdPushConstants(frame.cmd,
                           pipe->pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, (uint32_t)sizeof(SpritePush), &pc);

        const uint32_t indexCount =
            (it.indexCount > 0) ? (uint32_t)it.indexCount : geo.indexCount;

        vkCmdDrawIndexed(frame.cmd, indexCount, 1, 0, 0, 0);

        AddDrawCall();
    }
}

} // namespace toy
