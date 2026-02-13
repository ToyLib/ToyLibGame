#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Polygon.h"
#include "Asset/Geometry/VertexArrayBackend.h"

#include "Asset/Geometry/GL/GLVertexArrayBackend.h"
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"

#include "Render/RenderBackendState.h"

#include <utility>
#include <cstring> // memcpy
#include <iostream>

namespace toy {

//==============================================================
// ctor（スキンあり）
//==============================================================
VertexArray::VertexArray(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         const unsigned int* boneids,
                         const float* weights,
                         unsigned int numIndices,
                         const unsigned int* indices)
{
    mNumVerts   = numVerts;
    mNumIndices = numIndices;

    // 物理用：xyz 配列（stride=3）
    CreatePolygons(verts, indices, mNumIndices);

    if (RenderBackendState::Get().IsGL())
    {
        mBackend = std::make_unique<GLVertexArrayBackend>(
            numVerts, verts, norms, uvs, boneids, weights, numIndices, indices);
    }
    else if (RenderBackendState::Get().IsVK())
    {
        mBackend = std::make_unique<VKVertexArrayBackend>(
            numVerts, verts, norms, uvs, boneids, weights, numIndices, indices);
    }
}

//==============================================================
// ctor（スキンなし）
//==============================================================
VertexArray::VertexArray(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         unsigned int numIndices,
                         const unsigned int* indices)
{
    mNumVerts   = numVerts;
    mNumIndices = numIndices;

    // 物理用：xyz 配列（stride=3）
    CreatePolygons(verts, indices, mNumIndices);

    if (RenderBackendState::Get().IsGL())
    {
        mBackend = std::make_unique<GLVertexArrayBackend>(
            numVerts, verts, norms, uvs, numIndices, indices);
    }
    else if (RenderBackendState::Get().IsVK())
    {
        mBackend = std::make_unique<VKVertexArrayBackend>(
            numVerts, verts, norms, uvs, numIndices, indices);
    }
}

//==============================================================
// ctor（スプライト）
//  - 8 float/vertex の interleaved（pos3 + normal3 + uv2）
//  - ★仕様維持：スプライトからも GetPolygons() を取れるようにする
//  - ★VK用：CPUコピーを保持する（SpriteQuadで必須）
//==============================================================
VertexArray::VertexArray(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices)
{
    mNumVerts   = numVerts;
    mNumIndices = numIndices;

    // pos は先頭 (x,y,z) なので stride=8 で読める
    CreatePolygonsWithStride(verts, 8, indices, mNumIndices);

    // ★追加：小物のCPUコピーを保持（VKのEnsureSpriteGeometryVKが読む）
    StoreCpuGeometryIfSmall(
        verts,
        (uint32_t)numVerts,
        8,
        indices,
        (uint32_t)numIndices,
        CpuVertexLayout::Pos3Nrm3UV2_F32);

    if (RenderBackendState::Get().IsGL())
    {
        mBackend = std::make_unique<GLVertexArrayBackend>(
            verts, numVerts, indices, numIndices);
    }
    else if (RenderBackendState::Get().IsVK())
    {
        mBackend = std::make_unique<VKVertexArrayBackend>(
            verts, numVerts, indices, numIndices);
    }
}

//==============================================================
// ctor（vec2 only：フルスクリーン等）
//  - 物理用ポリゴンは不要（従来どおり）
//  - ★VK用：CPUコピーを保持する（必要なら）
//==============================================================
VertexArray::VertexArray(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices,
                         bool isVec2Only)
{
    (void)isVec2Only;

    mNumVerts   = numVerts;
    mNumIndices = numIndices;

    // vec2-only はポリゴン生成しない（従来どおり）

    // ★追加：必要ならCPUコピー保持（pos2のみ想定）
    // もし vec2-only が「pos2+uv2」なら stride=4 / Pos2UV2_F32 に変えてOK
    StoreCpuGeometryIfSmall(
        verts,
        (uint32_t)numVerts,
        2,
        indices,
        (uint32_t)numIndices,
        CpuVertexLayout::Pos2_F32);

    if (RenderBackendState::Get().IsGL())
    {
        mBackend = std::make_unique<GLVertexArrayBackend>(
            verts, numVerts, indices, numIndices, true);
    }
    else if (RenderBackendState::Get().IsVK())
    {
        mBackend = std::make_unique<VKVertexArrayBackend>(
            verts, numVerts, indices, numIndices, true);
    }
}

//==============================================================
// dtor
//==============================================================
VertexArray::~VertexArray()
{
    Unload();
    mPolygons.clear();
    mCpuVertexData.clear();
    mCpuIndexData.clear();
}

//==============================================================
// Unload（GPUリソース破棄）
//==============================================================
void VertexArray::Unload()
{
    if (mBackend)
    {
        mBackend->Unload();
        mBackend.reset();
    }
}

//==============================================================
// SetActive（GPU bind）
//==============================================================
void VertexArray::SetActive()
{
    if (mBackend)
    {
        // GL: VAO bind
        // VK: no-op（cmd で BindVertexBuffers/IndexBuffer する）
        mBackend->Bind();
    }
}

//==============================================================
// ポリゴンデータ生成（xyz専用）
//==============================================================
void VertexArray::CreatePolygons(const float* verts,
                                 const unsigned int* indices,
                                 unsigned int num)
{
    // 旧仕様維持：xyz 配列（stride=3）
    CreatePolygonsWithStride(verts, 3, indices, num);
}

//==============================================================
// ★追加：stride 指定版（pos が先頭にある前提）
//==============================================================
void VertexArray::CreatePolygonsWithStride(const float* verts,
                                           unsigned int strideFloats,
                                           const unsigned int* indices,
                                           unsigned int num)
{
    if (!verts)    return;
    if (num == 0)  return;
    if (!indices)  return;

    mPolygons.reserve(mPolygons.size() + (num / 3));

    for (unsigned int i = 0; i < num / 3; ++i)
    {
        Polygon poly;

        const unsigned int ia = indices[i * 3 + 0];
        const unsigned int ib = indices[i * 3 + 1];
        const unsigned int ic = indices[i * 3 + 2];

        const unsigned int a = ia * strideFloats;
        const unsigned int b = ib * strideFloats;
        const unsigned int c = ic * strideFloats;

        // pos は常に先頭 (x,y,z) 前提
        poly.a = Vector3(verts[a + 0], verts[a + 1], verts[a + 2]);
        poly.b = Vector3(verts[b + 0], verts[b + 1], verts[b + 2]);
        poly.c = Vector3(verts[c + 0], verts[c + 1], verts[c + 2]);

        mPolygons.emplace_back(poly);
    }
}

//==============================================================
// ★追加：CPUコピー保持（小物だけ）
//==============================================================
void VertexArray::StoreCpuGeometryIfSmall(const float* verts,
                                         uint32_t vertexCount,
                                         uint32_t strideFloats,
                                         const unsigned int* indices,
                                         uint32_t indexCount,
                                         CpuVertexLayout layout)
{
    // “小物だけ保持” の安全弁（SpriteQuad想定：4 verts / 6 idx）
    constexpr uint32_t kMaxKeepVerts  = 1024;
    constexpr uint32_t kMaxKeepIndex  = 4096;

    if (!verts || !indices) return;
    if (vertexCount == 0 || indexCount == 0) return;
    if (strideFloats == 0) return;

    if (vertexCount > kMaxKeepVerts || indexCount > kMaxKeepIndex)
    {
        return;
    }

    mCpuLayout            = layout;
    mCpuVertexCount       = vertexCount;
    mCpuIndexCount        = indexCount;
    mCpuVertexStrideBytes = strideFloats * (uint32_t)sizeof(float);

    // Vertex data (float array -> byte array)
    const size_t vBytes = (size_t)vertexCount * (size_t)mCpuVertexStrideBytes;
    mCpuVertexData.resize(vBytes);
    std::memcpy(mCpuVertexData.data(), verts, vBytes);

    // Index data (uint32)
    mCpuIndexData.resize((size_t)indexCount * sizeof(uint32_t));
    uint32_t* dst = reinterpret_cast<uint32_t*>(mCpuIndexData.data());
    for (uint32_t i = 0; i < indexCount; ++i)
    {
        dst[i] = (uint32_t)indices[i];
    }
}

//==============================================================
// ワールド行列を適用した三角形リストを返す
//==============================================================
std::vector<Polygon> VertexArray::GetWorldPolygons(const Matrix4& worldTransform) const
{
    std::vector<Polygon> result;
    result.reserve(mPolygons.size());

    for (const auto& poly : mPolygons)
    {
        Polygon wp;
        wp.a = Vector3::Transform(poly.a, worldTransform);
        wp.b = Vector3::Transform(poly.b, worldTransform);
        wp.c = Vector3::Transform(poly.c, worldTransform);
        result.emplace_back(wp);
    }

    return result;
}

} // namespace toy
