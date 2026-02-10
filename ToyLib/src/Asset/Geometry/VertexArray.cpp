#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Polygon.h"
#include "Asset/Geometry/VertexArrayBackend.h"
#include "Asset/Geometry/GL/GLVertexArrayBackend.h"
#include "Render/RenderBackendState.h"

#include <utility>

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

    // GPU用：現状は GL を生成（判定方法は後で差し替え）
    mBackend = std::make_unique<GLVertexArrayBackend>(
        numVerts, verts, norms, uvs, boneids, weights, numIndices, indices);
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
        // GPU用：現状は GL
        mBackend = std::make_unique<GLVertexArrayBackend>(
                                                          numVerts, verts, norms, uvs, numIndices, indices);
    }
}

//==============================================================
// ctor（スプライト）
//  - 8 float/vertex の interleaved（pos3 + normal3 + uv2）
//  - ★仕様維持：スプライトからも GetPolygons() を取れるようにする
//==============================================================
VertexArray::VertexArray(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices)
{
    mNumVerts   = numVerts;
    mNumIndices = numIndices;

    // ★重要：8 float/vertex でも pos は先頭 (x,y,z) なので stride=8 で読める
    CreatePolygonsWithStride(verts, 8, indices, mNumIndices);

    if (RenderBackendState::Get().IsGL())
    {
        // GPU用：現状は GL
        mBackend = std::make_unique<GLVertexArrayBackend>(
                                                          verts, numVerts, indices, numIndices);
    }
}

//==============================================================
// ctor（vec2 only：フルスクリーン等）
//  - 物理用ポリゴンは不要（従来どおり）
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

    if (RenderBackendState::Get().IsGL())
    {
        // GPU用：現状は GL
        mBackend = std::make_unique<GLVertexArrayBackend>(
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
    if (!verts) return;
    if (num == 0) return;

    // indices が無いケース（wireframeなど：numIndices=0 + indices=nullptr）を安全に通す
    if (!indices) return;

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
