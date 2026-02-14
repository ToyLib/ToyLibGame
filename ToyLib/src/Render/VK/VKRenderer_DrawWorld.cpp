// Render/VK/VKRenderer_DrawWorld.cpp

#include "Render/VK/VKRenderer.h"

#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"
#include "Render/RenderHandles.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"

#include <vulkan/vulkan.h>

namespace toy {

//==============================================================
// Helpers
//==============================================================

static VkCommandBuffer GetCmd(VKRenderer& r)
{
    return r.GetActiveCommandBuffer();
}

static VKPipeline* GetVKPipeFromHandle(const PipelineHandle& h)
{
    if (!h.IsValidVK())
    {
        return nullptr;
    }
    return reinterpret_cast<VKPipeline*>(h.ptrVKPipeline);
}

static bool BindPipelineIfValid(VkCommandBuffer cmd, VKPipeline* pipe)
{
    if (!cmd || !pipe)
    {
        return false;
    }
    if (pipe->pipeline == VK_NULL_HANDLE || pipe->pipelineLayout == VK_NULL_HANDLE)
    {
        return false;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);
    return true;
}

// VertexArrayBackend(VK) から VB/IB を取る。
// ※あなたの VertexArrayBackend は GetVKVertexBuffer/GetVKIndexBuffer を持ってる前提
static bool BindGeometryFromVertexArray(VkCommandBuffer cmd,
                                       const RenderItem& it,
                                       VkDeviceSize vbOffset = 0)
{
    if (!cmd) return false;
    if (!it.geometry.ptr) return false;

    VertexArray* va = it.geometry.ptr;

    // VK backend でなければ描けない
    // ここは IVertexArrayBackend を返す API が無い場合、
    // VertexArray 側に GetBackend() を追加するか、
    // “VK用アクセサ”を VertexArray に生やしておく必要がある。
    //
    // ただ、あなたは既に backend に GetVKVertexBuffer/GetVKIndexBuffer を実装しているので、
    // それを VertexArray::SetActive() ではなく Draw 側から参照できるようにしているはず。
    //
    // もし現状 VertexArray から backend を取れないなら、
    // いったん VertexArray に下の2つだけ足してOK：
    //   void* GetVKVertexBuffer() const;
    //   void* GetVKIndexBuffer() const;

//    VkBuffer vb = (VkBuffer)va->GetVKVertexBuffer(); // ★要：VertexArrayに薄い転送関数
    VkBuffer vb = (VkBuffer)va->GetBackend()->GetVKVertexBuffer();
    VkBuffer ib = (VkBuffer)va->GetBackend()->GetVKIndexBuffer();  // ★要：VertexArrayに薄い転送関数

    if (!vb || !ib)
    {
        return false;
    }

    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vbOffset);
    vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
    return true;
}

//==============================================================
// VKRenderer : World binding (minimum)
//==============================================================
//
// まずは「表示優先」なので、UBOは後回し。
// いまは Sprite と同様に PushConstants で world/viewProj を渡す方針。
// （GL と同じ row-vector を崩さない）
//==============================================================

struct WorldPush
{
    // row-vector 前提：shader 側が v * world * viewProj で使う
    float world[16];
    float viewProj[16];

    float colorAlpha[4]; // optional: 使わないなら 1,1,1,1 を詰める
};

static void CopyMat16(const Matrix4& m, float out16[16])
{
    // Matrix4 が float mat[4][4] の row-major で並んでいる前提
    // （あなたの MathUtil そのままの順）
    const float* p = m.GetAsFloatPtr();
    for (int i = 0; i < 16; ++i) out16[i] = p[i];
}

bool VKRenderer::BindWorldCommon(VkCommandBuffer cmd,
                                VKPipeline* pipe,
                                const RenderItem& it)
{
    if (!cmd || !pipe) return false;

    WorldPush pc{};
    CopyMat16(it.world, pc.world);
    CopyMat16(it.viewProj, pc.viewProj);

    pc.colorAlpha[0] = 1.0f;
    pc.colorAlpha[1] = 1.0f;
    pc.colorAlpha[2] = 1.0f;
    pc.colorAlpha[3] = 1.0f;

    vkCmdPushConstants(cmd,
                       pipe->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, (uint32_t)sizeof(WorldPush), &pc);
    return true;
}

// Material / Texture の descriptor を bind する（最小）
// - “Mesh用” descriptor set を用意する想定
// - まだ未実装なら、fallback（white）だけでも描けるようにする
bool VKRenderer::BindWorldMaterial(VkCommandBuffer cmd,
                                  VKPipeline* pipe,
                                  const RenderItem& it)
{
    if (!cmd || !pipe) return false;

    // ここは Sprite と同じ思想で
    //   TextureHandle -> VkImageView/VkSampler -> VkDescriptorSet
    // を作る仕組みを Mesh 用にも作る。
    //
    // 今は “表示優先” なので、Texture が無ければ fallback を使う想定だけ先に置く。
    //
    // ※ 実装済みの Sprite 用 descriptor を流用するなら、
    //    “uTexture” だけでよい間は GetOrCreateSpriteDescSet を使ってもOK。
    //
    //    ただし将来的に Mesh は
    //      - shadow maps
    //      - material params
    //      - normal map
    //    などが増えるので、Mesh 専用 set を持つのが健全。

    VkDescriptorSet set0 = VK_NULL_HANDLE;

    // 例：暫定で「material.diffuse を 1枚だけ」扱うなら
    // set0 = GetOrCreateMeshDescSet(it.material);
    //
    // まだ無いなら fallback で “バインドしないでも動く” shader にするか、
    // あるいは Sprite の set を暫定流用：
    if (it.texture.IsValid())
    {
        set0 = GetOrCreateSpriteDescSet(it.texture);
    }
    else
    {
        // texture 無しは fallback を返すようにしておくと楽
        set0 = GetOrCreateSpriteDescSet(TextureHandle{});
    }

    if (set0 == VK_NULL_HANDLE)
    {
        return false;
    }

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipe->pipelineLayout,
                            0, 1, &set0,
                            0, nullptr);
    return true;
}

//==============================================================
// Bucket draw
//==============================================================

void VKRenderer::DrawBucket_World(const std::vector<uint32_t>& bucket)
{
    if (bucket.empty()) return;

    VkCommandBuffer cmd = GetCmd(*this);
    if (!cmd) return;

    const auto& items = mRenderQueue.Items();

    // dynamic viewport/scissor（swapchain full）
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = (float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = mSwapchainExtent;

    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // pipeline cache（バインド削減）
    VKPipeline* lastPipe = nullptr;

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;

        const RenderItem& it = items[idx];

        // World bucket なので pass は World を期待
        if (it.pass != RenderPass::World)
        {
            continue;
        }

        // Mesh / Skinned を分けたい（GL踏襲）
        // RenderItemType に合わせて pipeline 名を決める方針ならここで分岐
        // ただ今は RenderItem 側が it.pipeline を持ってるので、それを尊重する
        VKPipeline* pipe = GetVKPipeFromHandle(it.pipeline);
        if (!pipe) continue;

        if (pipe != lastPipe)
        {
            if (!BindPipelineIfValid(cmd, pipe))
            {
                lastPipe = nullptr;
                continue;
            }
            lastPipe = pipe;
        }

        // geometry bind
        if (!BindGeometryFromVertexArray(cmd, it))
        {
            continue;
        }

        // common push constants
        if (!BindWorldCommon(cmd, pipe, it))
        {
            continue;
        }

        // material bind（texture）
        // shader側が必須なら必ず bind。必須でないなら失敗しても続行でもOK
        (void)BindWorldMaterial(cmd, pipe, it);

        const uint32_t indexCount =
            (it.indexCount > 0) ? (uint32_t)it.indexCount
                                : (uint32_t)it.geometry.ptr->GetNumIndices();

        if (indexCount == 0)
        {
            continue;
        }

        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
        AddDrawCall();
    }
}

//==============================================================
// DrawWorldPass
//==============================================================

void VKRenderer::DrawWorldPass()
{
    // RenderPass は BeginFrame() 側で BeginRenderPass 済み前提（Spriteと同じ）
    // ここでは bucket を順に描画するだけ。

    // Opaque
    DrawBucket_World(mBuckets.worldOpaque);

    // Effect (Pre)
    DrawBucket_World(mBuckets.effectPre);

    // Transparent
    DrawBucket_World(mBuckets.worldTransparent);

    // Effect (Overlay)
    DrawBucket_World(mBuckets.effectOverlay);
}

} // namespace toy
