#include "Asset/Geometry/GL/GLVertexArrayBackend.h"
#include "glad/glad.h"

#include <algorithm>

namespace toy {

static inline void FillZero(std::array<unsigned int, 5>& a)
{
    a.fill(0u);
}


void GLVertexArrayBackend::ResetIds()
{
    mVAO = 0;
    mEBO = 0;
    mNumVBO = 0;
    FillZero(mVBO);
}

//==============================================================
// ctor（スキンメッシュ）
//==============================================================
GLVertexArrayBackend::GLVertexArrayBackend(unsigned int numVerts,
                                           const float* verts,
                                           const float* norms,
                                           const float* uvs,
                                           const unsigned int* boneids,
                                           const float* weights,
                                           unsigned int numIndices,
                                           const unsigned int* indices)
{
    ResetIds();
    mNumVBO = 5;

    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    // EBO
    glGenBuffers(1, &mEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(indices[0]) * numIndices,
                 indices,
                 GL_STATIC_DRAW);

    // VBOs
    glGenBuffers(5, mVBO.data());

    // pos
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(verts[0]) * numVerts * 3,
                 verts,
                 GL_STATIC_DRAW);

    // norm
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(norms[0]) * numVerts * 3,
                 norms,
                 GL_STATIC_DRAW);

    // uv
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[2]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(uvs[0]) * numVerts * 2,
                 uvs,
                 GL_STATIC_DRAW);

    // bone ids (uvec4)
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[3]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(boneids[0]) * numVerts * 4,
                 boneids,
                 GL_STATIC_DRAW);

    // weights (vec4)
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[4]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(weights[0]) * numVerts * 4,
                 weights,
                 GL_STATIC_DRAW);

    // attributes
    glEnableVertexAttribArray(0); // pos
    glEnableVertexAttribArray(1); // norm
    glEnableVertexAttribArray(2); // uv
    glEnableVertexAttribArray(3); // bone id
    glEnableVertexAttribArray(4); // weight

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[1]);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[2]);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[3]);
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[4]);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//==============================================================
// ctor（通常メッシュ）
//==============================================================
GLVertexArrayBackend::GLVertexArrayBackend(unsigned int numVerts,
                                           const float* verts,
                                           const float* norms,
                                           const float* uvs,
                                           unsigned int numIndices,
                                           const unsigned int* indices)
{
    ResetIds();
    mNumVBO = 3;

    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    // EBO
    glGenBuffers(1, &mEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(indices[0]) * numIndices,
                 indices,
                 GL_STATIC_DRAW);

    // VBOs
    glGenBuffers(3, mVBO.data());

    // pos
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * numVerts * 3,
                 verts,
                 GL_STATIC_DRAW);

    // norm
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * numVerts * 3,
                 norms,
                 GL_STATIC_DRAW);

    // uv
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[2]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * numVerts * 2,
                 uvs,
                 GL_STATIC_DRAW);

    // attributes
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[1]);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO[2]);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//==============================================================
// ctor（スプライト：interleaved 8float）
//==============================================================
GLVertexArrayBackend::GLVertexArrayBackend(const float* verts,
                                           unsigned int numVerts,
                                           const unsigned int* indices,
                                           unsigned int numIndices)
{
    ResetIds();
    mNumVBO = 1;

    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    const unsigned int vertexSize = 8 * sizeof(float);

    // VBO
    glGenBuffers(1, &mVBO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 numVerts * vertexSize,
                 verts,
                 GL_STATIC_DRAW);

    // EBO（スプライトは idx あり前提）
    glGenBuffers(1, &mEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 numIndices * sizeof(unsigned int),
                 indices,
                 GL_STATIC_DRAW);

    // attributes（pos3, normal3, uv2）
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(float) * 3));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(float) * 6));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//==============================================================
// ctor（vec2-only）
//==============================================================
GLVertexArrayBackend::GLVertexArrayBackend(const float* verts,
                                           unsigned int numVerts,
                                           const unsigned int* indices,
                                           unsigned int numIndices,
                                           bool /*isVec2Only*/)
{
    ResetIds();
    mNumVBO = 1;

    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    // VBO (vec2)
    glGenBuffers(1, &mVBO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * 2 * numVerts,
                 verts,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // EBO（0 でも作ってOK。numIndices==0 の場合は空）
    if (indices && numIndices > 0)
    {
        glGenBuffers(1, &mEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(unsigned int) * numIndices,
                     indices,
                     GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//==============================================================
// dtor
//==============================================================
GLVertexArrayBackend::~GLVertexArrayBackend()
{
    Unload();
}

//==============================================================
// Unload
//==============================================================
void GLVertexArrayBackend::Unload()
{
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (mNumVBO > 0)
    {
        glDeleteBuffers((GLsizei)mNumVBO, mVBO.data());
        mNumVBO = 0;
    }

    if (mEBO != 0)
    {
        glDeleteBuffers(1, &mEBO);
        mEBO = 0;
    }

    if (mVAO != 0)
    {
        glDeleteVertexArrays(1, &mVAO);
        mVAO = 0;
    }

    FillZero(mVBO);

}

//==============================================================
// Bind（同値スキップ）
//==============================================================
void GLVertexArrayBackend::Bind()
{
    glBindVertexArray(mVAO);
}

} // namespace toy
