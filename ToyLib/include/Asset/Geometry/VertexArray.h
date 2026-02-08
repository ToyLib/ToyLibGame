#pragma once

#include "Utils/MathUtil.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace toy {

struct Polygon;

//-------------------------------------------
// VertexArray
// ・CPU側：ポリゴン（物理用）を保持
// ・GPU側：バックエンド実装（GL/VK）に委譲（SetActive/Unload）
// ・公開インターフェースは変更しない
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
    const std::vector<Polygon>& GetPolygons() const { return mPolygons; }

    //-----------------------------------------------
    // 三角形ポリゴン（ワールド座標変換済み）を返す
    //-----------------------------------------------
    std::vector<Polygon> GetWorldPolygons(const Matrix4& worldTransform) const;

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
    //  - GL 実装は追加ファイル側（GLVertexArrayBackend）
    //  - VK 実装は後で追加できる
    //-----------------------------------------------
    std::unique_ptr<class IVertexArrayBackend> mBackend;

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
};

} // namespace toy
