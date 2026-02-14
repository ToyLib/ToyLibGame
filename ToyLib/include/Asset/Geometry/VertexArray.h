#pragma once

#include "Utils/MathUtil.h"
#include "Asset/Geometry/IVertexArrayBackend.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace toy {

//-------------------------------------------
// VertexArray
// ・CPU側：ポリゴン（物理用）を保持
// ・GPU側：バックエンド実装（GL/VK）に委譲（SetActive/Unload）
// ・公開インターフェースは変更しない（既存APIは維持）
//-------------------------------------------
class VertexArray
{
public:
    //=====================================================
    // ▼ 4頂点のみの簡易モデル（スプライト用）
    //   verts : 1頂点あたり 8 float (pos + normal + uv) を想定
    //=====================================================
    VertexArray(const float* verts,
                unsigned int numVerts,
                const unsigned int* indices,
                unsigned int numIndices);

    //=====================================================
    // ▼ 通常メッシュ / スキンなし
    //   verts : xyz * numVerts
    //   norms : normal
    //   uvs   : texcoord
    //=====================================================
    VertexArray(unsigned int numVerts,
                const float* verts,
                const float* norms,
                const float* uvs,
                unsigned int numIndices,
                const unsigned int* indices);

    //=====================================================
    // ▼ アニメーションメッシュ（スキンあり）
    //   boneids : 4本までのボーン ID (unsigned int x4)
    //   weights : 各ウェイト (float x4)
    //=====================================================
    VertexArray(unsigned int numVerts,
                const float* verts,
                const float* norms,
                const float* uvs,
                const unsigned int* boneids,
                const float* weights,
                unsigned int numIndices,
                const unsigned int* indices);

    //=====================================================
    // ▼ 雨粒やフルスクリーンエフェクト等の特殊用途
    //   （頂点が vec2 のみ）
    //=====================================================
    VertexArray(const float* verts,
                unsigned int numVerts,
                const unsigned int* indices,
                unsigned int numIndices,
                bool isVec2Only);

    virtual ~VertexArray();

    void Unload();

    //-----------------------------------------------
    // 描画時に VAO を bind（バックエンドへ委譲）
    //-----------------------------------------------
    void SetActive();

    //-----------------------------------------------
    // 使用するテクスチャ（MaterialIndex）を記録
    //-----------------------------------------------
    void SetTextureID(unsigned int id) { mTextureID = id; }
    unsigned int GetTextureID() const { return mTextureID; }

    //-----------------------------------------------
    // 基本情報取得
    //-----------------------------------------------
    unsigned int GetNumVerts() const   { return mNumVerts; }
    unsigned int GetNumIndices() const { return mNumIndices; }

    //-----------------------------------------------
    // 三角形ポリゴン（ローカル）取得
    //-----------------------------------------------
    const std::vector<struct Polygon>& GetPolygons() const { return mPolygons; }

    //-----------------------------------------------
    // 三角形ポリゴン（ワールド座標変換済み）を返す
    //-----------------------------------------------
    std::vector<struct Polygon> GetWorldPolygons(const Matrix4& worldTransform) const;

    //=====================================================
    // ▼ 追加：CPU頂点アクセス（VK 等のバックエンド用）
    //  - 目的：SpriteQuad 等 “小さい固定ジオメトリ” をVK VB/IB化する
    //  - 通常メッシュは巨大なので、ここでは「保持している場合だけ」返す
    //=====================================================

    enum class CpuVertexLayout : uint8_t
    {
        None = 0,

        // 1頂点 = 8 floats : pos3 + normal3 + uv2（SpriteQuadの想定）
        Pos3Nrm3UV2_F32,

        // 1頂点 = 4 floats : pos2 + uv2
        Pos2UV2_F32,

        // 1頂点 = 2 floats : pos2
        Pos2_F32,
    };

    // CPUコピーを持っていれば true。持っていなければ false（通常メッシュ等）
    bool HasCpuGeometry() const
    {
        return !mCpuVertexData.empty() &&
               !mCpuIndexData.empty()  &&
               (mCpuVertexStrideBytes > 0) &&
               (mCpuVertexCount > 0) &&
               (mCpuIndexCount > 0);
    }

    // VK側（EnsureSpriteGeometryVK）が使う想定の最小アクセサ
    const void* GetVertexData() const
    {
        return mCpuVertexData.empty() ? nullptr : mCpuVertexData.data();
    }

    const void* GetIndexData() const
    {
        return mCpuIndexData.empty() ? nullptr : mCpuIndexData.data();
    }

    size_t GetVertexDataSizeBytes() const { return mCpuVertexData.size(); }
    size_t GetIndexDataSizeBytes()  const { return mCpuIndexData.size();  }

    uint32_t GetVertexStrideBytes() const { return mCpuVertexStrideBytes; }
    uint32_t GetIndexCount()  const { return mCpuIndexCount; }
    uint32_t GetVertexCount() const { return mCpuVertexCount; }

    CpuVertexLayout GetCpuVertexLayout() const { return mCpuLayout; }
    
    // VertexArray.h に追加（API追加だけなので既存は壊れない）
    void* GetVKVertexBuffer() const { return mBackend ? mBackend->GetVKVertexBuffer() : nullptr; }
    void* GetVKIndexBuffer()  const { return mBackend ? mBackend->GetVKIndexBuffer()  : nullptr; }
    uint32_t GetVKIndexType() const  { return mBackend ? (uint32_t)mBackend->GetVKIndexType() : (uint32_t)VK_INDEX_TYPE_UINT32; }

private:
    //=====================================================
    // コピー禁止
    //=====================================================
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

private:
    //=====================================================
    // CPU側（物理用）
    //=====================================================
    unsigned int mNumVerts   { 0 };
    unsigned int mNumIndices { 0 };

    unsigned int mTextureID { 0 };

    std::vector<Polygon> mPolygons;

    //-----------------------------------------------
    // GPU側：バックエンド実装へ委譲
    //-----------------------------------------------
    std::unique_ptr<class IVertexArrayBackend> mBackend;

    //=====================================================
    // CPU copy (optional) : 小物ジオメトリ用
    //=====================================================
    std::vector<uint8_t> mCpuVertexData;
    std::vector<uint8_t> mCpuIndexData;

    uint32_t      mCpuVertexStrideBytes { 0 };
    uint32_t      mCpuVertexCount       { 0 };
    uint32_t      mCpuIndexCount        { 0 };
    CpuVertexLayout mCpuLayout          { CpuVertexLayout::None };

private:
    //-----------------------------------------------
    // ローカル頂点 → Polygon（三角形リスト）へ変換
    //-----------------------------------------------
    void CreatePolygons(const float* verts,
                        const unsigned int* indices,
                        unsigned int numIndices);

    // ★追加：stride 指定版（pos が先頭にある前提）
    void CreatePolygonsWithStride(const float* verts,
                                  unsigned int strideFloats,
                                  const unsigned int* indices,
                                  unsigned int numIndices);

    // ★追加：CPUコピー保持（必要なコンストラクタでだけ呼ぶ）
    void StoreCpuGeometryIfSmall(const float* verts,
                                 uint32_t vertexCount,
                                 uint32_t strideFloats,
                                 const unsigned int* indices,
                                 uint32_t indexCount,
                                 CpuVertexLayout layout);
};

} // namespace toy
