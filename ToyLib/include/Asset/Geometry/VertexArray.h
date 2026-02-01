#pragma once

#include "Utils/MathUtil.h"
#include <vector>
#include <cstdint>

namespace toy {

struct Polygon;

//-------------------------------------------
// VertexArray
// ・OpenGL の VAO/VBO/IBO をまとめて管理
// ・メッシュ、スキンメッシュ、スプライトなど用途ごとに複数の
//   コンストラクタを用意
// ・衝突判定用に三角形(Polygon)リストも保持
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
    // 描画時に VAO を bind
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
    // コピー禁止（AssetManager 経由の shared_ptr 前提）
    //=====================================================
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    // 頂点数・インデックス数
    unsigned int mNumVerts    { 0 };
    unsigned int mNumIndices  { 0 };

    //-----------------------------------------------
    // ★追加：実際に生成した VBO の本数（1/3/5）
    //-----------------------------------------------
    unsigned int mNumVBO      { 0 };

    //-----------------------------------------------
    // VBO 配列（最大 5 本：pos, normal, uv, boneID, weight）
    //-----------------------------------------------
    unsigned int mVertexBuffer[5] { 0, 0, 0, 0, 0 };

    //-----------------------------------------------
    // VAO / IBO
    //-----------------------------------------------
    unsigned int mVertexBufferID { 0 }; // VAO
    unsigned int mIndexBufferID  { 0 }; // EBO/IBO
    // VAO 同値スキップ用
    static unsigned int sCurrentVAO;

    //-----------------------------------------------
    // マテリアルインデックスとして使う TextureID
    //-----------------------------------------------
    unsigned int mTextureID { 0 };

    //-----------------------------------------------
    // 物理判定用の三角形リスト
    //-----------------------------------------------
    std::vector<Polygon> mPolygons;

private:
    //-----------------------------------------------
    // ローカル頂点 → Polygon（三角形リスト）へ変換
    //-----------------------------------------------
    void CreatePolygons(const float* verts,
                        const unsigned int* indices,
                        unsigned int numIndices);
};

} // namespace toy
