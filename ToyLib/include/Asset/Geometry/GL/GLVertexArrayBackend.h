#pragma once

#include "Asset/Geometry/VertexArrayBackend.h"
#include <array>
#include <cstdint>

namespace toy {

//-------------------------------------------
// GLVertexArrayBackend
// ・旧 VertexArray.cpp にあった GL(VAO/VBO/EBO) 処理を移植
// ・コンストラクタのシグネチャを VertexArray と揃えて「丸投げ」できる形
//-------------------------------------------
class GLVertexArrayBackend final : public IVertexArrayBackend
{
public:
    // Sprite (8float interleaved)
    GLVertexArrayBackend(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices);

    // Mesh (pos/norm/uv)
    GLVertexArrayBackend(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         unsigned int numIndices,
                         const unsigned int* indices);

    // Skinned mesh (pos/norm/uv + boneids + weights)
    GLVertexArrayBackend(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         const unsigned int* boneids,
                         const float* weights,
                         unsigned int numIndices,
                         const unsigned int* indices);

    // Vec2-only (fullscreen etc.)
    GLVertexArrayBackend(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices,
                         bool isVec2Only);

    ~GLVertexArrayBackend() override;

    void Bind() override;
    void Unload() override;

private:
    void ResetIds();

private:
    unsigned int mVAO { 0 };
    unsigned int mEBO { 0 };

    // 最大 5 本：pos, normal, uv, boneID, weight
    std::array<unsigned int, 5> mVBO { {0,0,0,0,0} };
    unsigned int mNumVBO { 0 };

    static unsigned int sCurrentVAO;
};

} // namespace toy
