#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Polygon.h"
#include "glad/glad.h"

#include <algorithm> // std::fill

namespace toy {

//--------------------------------------------------------------
// 内部：安全初期化
//--------------------------------------------------------------
static inline void ResetGLIds(unsigned int& vao,
                             unsigned int& ebo,
                             unsigned int (&vbo)[5],
                             unsigned int& numVbo)
{
    vao = 0;
    ebo = 0;
    numVbo = 0;
    std::fill(std::begin(vbo), std::end(vbo), 0u);
}

//==============================================================
// コンストラクタ（スキンメッシュ用）
//  - 頂点：位置・法線・UV
//  - ボーン：ID と Weight（最大 4 本）
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
    ResetGLIds(mVertexBufferID, mIndexBufferID, mVertexBuffer, mNumVBO);

    mNumVerts   = numVerts;
    mNumIndices = numIndices;
    mNumVBO     = 5;

    // VAO 生成
    glGenVertexArrays(1, &mVertexBufferID);
    glBindVertexArray(mVertexBufferID);

    //------------------------------------------
    // インデックスバッファ（EBO）
    //------------------------------------------
    glGenBuffers(1, &mIndexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(indices[0]) * mNumIndices,
                 indices,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // VBO を 5 本用意
    //------------------------------------------
    glGenBuffers(5, mVertexBuffer);

    // 位置
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(verts[0]) * mNumVerts * 3,
                 verts,
                 GL_STATIC_DRAW);

    // 法線
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(norms[0]) * mNumVerts * 3,
                 norms,
                 GL_STATIC_DRAW);

    // UV
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[2]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(uvs[0]) * mNumVerts * 2,
                 uvs,
                 GL_STATIC_DRAW);

    // BoneID（unsigned int x4）
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[3]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(boneids[0]) * mNumVerts * 4,
                 boneids,
                 GL_STATIC_DRAW);

    // Weight（float x4）
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[4]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(weights[0]) * mNumVerts * 4,
                 weights,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // 頂点属性の関連付け
    //------------------------------------------
    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // normal
    glEnableVertexAttribArray(2); // uv
    glEnableVertexAttribArray(3); // bone id
    glEnableVertexAttribArray(4); // weight

    // position : layout(location = 0)
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // normal : layout(location = 1)
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[1]);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // uv : layout(location = 2)
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[2]);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // bone id : layout(location = 3)  ※ unsigned int なので UNSIGNED_INT
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[3]);
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, 0, nullptr);

    // weight : layout(location = 4)
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[4]);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    // 任意：unbind（EBOはVAOに紐付くのでVAOを外す）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 三角形ポリゴン（ローカル座標）生成
    CreatePolygons(verts, indices, mNumIndices);
}

//==============================================================
// コンストラクタ（通常メッシュ用：ボーンなし）
//  - 頂点：位置・法線・UV
//==============================================================
VertexArray::VertexArray(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         unsigned int numIndices,
                         const unsigned int* indices)
{
    ResetGLIds(mVertexBufferID, mIndexBufferID, mVertexBuffer, mNumVBO);

    mNumVerts   = numVerts;
    mNumIndices = numIndices;
    mNumVBO     = 3;

    // VAO
    glGenVertexArrays(1, &mVertexBufferID);
    glBindVertexArray(mVertexBufferID);

    //------------------------------------------
    // インデックスバッファ（EBO）
    //------------------------------------------
    glGenBuffers(1, &mIndexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(indices[0]) * mNumIndices,
                 indices,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // VBO 3 本：位置・法線・UV
    //------------------------------------------
    glGenBuffers(3, mVertexBuffer);

    // 位置
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * mNumVerts * 3,
                 verts,
                 GL_STATIC_DRAW);

    // 法線
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * mNumVerts * 3,
                 norms,
                 GL_STATIC_DRAW);

    // UV
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[2]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * mNumVerts * 2,
                 uvs,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // 頂点属性設定
    //------------------------------------------
    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // normal
    glEnableVertexAttribArray(2); // uv

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[1]);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[2]);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 三角形ポリゴン（ローカル座標）生成
    CreatePolygons(verts, indices, mNumIndices);
}

//==============================================================
// コンストラクタ（スプライト用）
//  - 1頂点あたり 8 float (pos + normal + uv)
//==============================================================
VertexArray::VertexArray(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices)
{
    ResetGLIds(mVertexBufferID, mIndexBufferID, mVertexBuffer, mNumVBO);

    mNumVerts   = numVerts;
    mNumIndices = numIndices;
    mNumVBO     = 1;

    // VAO
    glGenVertexArrays(1, &mVertexBufferID);
    glBindVertexArray(mVertexBufferID);

    const unsigned int vertexSize = 8 * sizeof(float); // xyz + normal + uv

    //------------------------------------------
    // 頂点バッファ（1本にすべて詰める）
    //------------------------------------------
    glGenBuffers(1, &mVertexBuffer[0]);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 mNumVerts * vertexSize,
                 verts,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // インデックスバッファ（EBO）
    //------------------------------------------
    glGenBuffers(1, &mIndexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mNumIndices * sizeof(unsigned int),
                 indices,
                 GL_STATIC_DRAW);

    //------------------------------------------
    // 頂点属性
    //------------------------------------------
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(float) * 3));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(float) * 6));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 三角形ポリゴン（ローカル座標）生成
    CreatePolygons(verts, indices, mNumIndices);
}

//==============================================================
// コンストラクタ（vec2 専用：フルスクリーンクアッド等）
//  - 位置のみ (x, y)
//==============================================================
VertexArray::VertexArray(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices,
                         bool /*isVec2Only*/)
{
    ResetGLIds(mVertexBufferID, mIndexBufferID, mVertexBuffer, mNumVBO);

    mNumVerts   = numVerts;
    mNumIndices = numIndices;
    mNumVBO     = 1;

    // VAO
    glGenVertexArrays(1, &mVertexBufferID);
    glBindVertexArray(mVertexBufferID);

    //------------------------------------------
    // 頂点バッファ（vec2）
    //------------------------------------------
    glGenBuffers(1, &mVertexBuffer[0]);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * 2 * mNumVerts,
                 verts,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    //------------------------------------------
    // インデックスバッファ（EBO）
    //------------------------------------------
    glGenBuffers(1, &mIndexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(unsigned int) * mNumIndices,
                 indices,
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // vec2-only の場合は物理用ポリゴンは不要なので作成しない
}

//==============================================================
// ポリゴンデータ生成（ローカル座標の三角形）
//  - verts: xyz xyz ...
//  - indices: 3つで1ポリゴン
//==============================================================
void VertexArray::CreatePolygons(const float* verts,
                                 const unsigned int* indices,
                                 const unsigned int num)
{
    // ※この関数は verts が xyz 配列の時だけ呼ぶ前提（現状の運用どおり）
    for (unsigned int i = 0; i < num / 3; ++i)
    {
        Polygon poly;

        // a
        poly.a.x = verts[indices[i * 3] * 3];
        poly.a.y = verts[indices[i * 3] * 3 + 1];
        poly.a.z = verts[indices[i * 3] * 3 + 2];

        // b
        poly.b.x = verts[indices[i * 3 + 1] * 3];
        poly.b.y = verts[indices[i * 3 + 1] * 3 + 1];
        poly.b.z = verts[indices[i * 3 + 1] * 3 + 2];

        // c
        poly.c.x = verts[indices[i * 3 + 2] * 3];
        poly.c.y = verts[indices[i * 3 + 2] * 3 + 1];
        poly.c.z = verts[indices[i * 3 + 2] * 3 + 2];

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

//==============================================================
// デストラクタ
//==============================================================
VertexArray::~VertexArray()
{
    mPolygons.clear();
    Unload();
}

//==============================================================
// 生成済みの VBO / IBO / VAO を破棄
//==============================================================
void VertexArray::Unload()
{
    // 先にunbind（保険）
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // ★作成した本数だけ消す（ここが重要）
    if (mNumVBO > 0)
    {
        glDeleteBuffers((GLsizei)mNumVBO, mVertexBuffer);
        mNumVBO = 0;
    }

    if (mIndexBufferID != 0)
    {
        glDeleteBuffers(1, &mIndexBufferID);
        mIndexBufferID = 0;
    }

    if (mVertexBufferID != 0)
    {
        glDeleteVertexArrays(1, &mVertexBufferID);
        mVertexBufferID = 0;
    }

    // 念のため 0 で埋める
    std::fill(std::begin(mVertexBuffer), std::end(mVertexBuffer), 0u);
}

//==============================================================
// 描画時に VAO を bind
//==============================================================
void VertexArray::SetActive()
{
    glBindVertexArray(mVertexBufferID);
}

} // namespace toy
