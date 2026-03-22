#include "Asset/Geometry/Mesh.h"
#include "Asset/Material/Texture.h"
#include "Asset/AssetManager.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Bone.h"
#include "Asset/Geometry/Polygon.h"
#include "Asset/Material/Material.h"
#include "Utils/StringUtil.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <string>
#include <algorithm>
#include <array>
#include <cstdlib>

static void MatrixAi2Gl(Matrix4& mat, const aiMatrix4x4 aim)
{
    mat.mat[0][0] = aim.a1;
    mat.mat[0][1] = aim.b1;
    mat.mat[0][2] = aim.c1;
    mat.mat[0][3] = aim.d1;

    mat.mat[1][0] = aim.a2;
    mat.mat[1][1] = aim.b2;
    mat.mat[1][2] = aim.c2;
    mat.mat[1][3] = aim.d2;

    mat.mat[2][0] = aim.a3;
    mat.mat[2][1] = aim.b3;
    mat.mat[2][2] = aim.c3;
    mat.mat[2][3] = aim.d3;

    mat.mat[3][0] = aim.a4;
    mat.mat[3][1] = aim.b4;
    mat.mat[3][2] = aim.c4;
    mat.mat[3][3] = aim.d4;
}

namespace
{
    struct Influence
    {
        unsigned int id;
        float weight;
    };

    static void NormalizeBoneWeights(toy::VertexBoneData& boneData, float minWeightThreshold)
    {
        std::array<Influence, toy::NUM_BONES_PER_VERTEX> influences {};

        for (unsigned int i = 0; i < toy::NUM_BONES_PER_VERTEX; ++i)
        {
            influences[i].id     = boneData.IDs[i];
            influences[i].weight = boneData.Weights[i];

            if (influences[i].weight < minWeightThreshold)
            {
                influences[i].id     = 0;
                influences[i].weight = 0.0f;
            }
        }

        std::sort(
            influences.begin(),
            influences.end(),
            [](const Influence& a, const Influence& b)
            {
                return a.weight > b.weight;
            });

        float sum = 0.0f;
        for (const auto& inf : influences)
        {
            sum += inf.weight;
        }

        if (sum > 0.0f)
        {
            const float inv = 1.0f / sum;
            for (auto& inf : influences)
            {
                inf.weight *= inv;
            }
        }

        for (unsigned int i = 0; i < toy::NUM_BONES_PER_VERTEX; ++i)
        {
            boneData.IDs[i]     = influences[i].id;
            boneData.Weights[i] = influences[i].weight;
        }
    }
}

namespace toy {

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
    Unload();
}

void Mesh::BuildEvalNodes()
{
    mEvalNodes.clear();
    mRootEvalNodeIndex = -1;

    if (!mScene || !mScene->mRootNode)
    {
        return;
    }

    mEvalNodes.reserve(128);
    mRootEvalNodeIndex = BuildEvalNodeRecursive(mScene->mRootNode, -1);
}

int Mesh::BuildEvalNodeRecursive(const aiNode* node, int parentIndex)
{
    if (!node)
    {
        return -1;
    }

    EvalNode evalNode;
    evalNode.name = node->mName.C_Str();
    evalNode.parentIndex = parentIndex;
    MatrixAi2Gl(evalNode.defaultTransform, node->mTransformation);

    auto it = mBoneMapping.find(evalNode.name);
    if (it != mBoneMapping.end())
    {
        evalNode.boneIndex = static_cast<int>(it->second);
    }

    const int myIndex = static_cast<int>(mEvalNodes.size());
    mEvalNodes.emplace_back(std::move(evalNode));

    mEvalNodes[myIndex].children.reserve(node->mNumChildren);
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        const int childIndex = BuildEvalNodeRecursive(node->mChildren[i], myIndex);
        if (childIndex >= 0)
        {
            mEvalNodes[myIndex].children.emplace_back(childIndex);
        }
    }

    return myIndex;
}

const Mesh::AnimationCache* Mesh::FindAnimationCache(const aiAnimation* pAnimation) const
{
    auto it = mAnimationCacheIndex.find(pAnimation);
    if (it == mAnimationCacheIndex.end())
    {
        return nullptr;
    }

    const size_t index = it->second;
    if (index >= mAnimationCaches.size())
    {
        return nullptr;
    }

    return &mAnimationCaches[index];
}

void Mesh::ComputeBoneHierarchyCached(float animationTime,
                                      int evalNodeIndex,
                                      const Matrix4& parentTransform,
                                      const Mesh::AnimationCache* animCache)
{
    const EvalNode& node = mEvalNodes[evalNodeIndex];

    Matrix4 nodeTransformation = node.defaultTransform;

    const aiNodeAnim* pNodeAnim = nullptr;
    if (animCache && static_cast<size_t>(evalNodeIndex) < animCache->channelsByEvalNode.size())
    {
        pNodeAnim = animCache->channelsByEvalNode[evalNodeIndex];
    }

    if (pNodeAnim)
    {
        Vector3 scaling(1.0f, 1.0f, 1.0f);
        CalcInterpolatedScaling(scaling, animationTime, pNodeAnim);
        Matrix4 scalingM = Matrix4::CreateScale(scaling);

        Quaternion rotationQ = Quaternion::Identity;
        CalcInterpolatedRotation(rotationQ, animationTime, pNodeAnim);
        Matrix4 rotationM = Matrix4::CreateFromQuaternion(rotationQ);

        Vector3 translation(0.0f, 0.0f, 0.0f);
        CalcInterpolatedPosition(translation, animationTime, pNodeAnim);
        Matrix4 translationM = Matrix4::CreateTranslation(translation);

        nodeTransformation = rotationM * translationM * scalingM;
    }

    const Matrix4 globalTransformation = nodeTransformation * parentTransform;

    if (node.boneIndex >= 0)
    {
        const unsigned int boneIndex = static_cast<unsigned int>(node.boneIndex);
        mBoneInfo[boneIndex].FinalTransformation =
            mBoneInfo[boneIndex].BoneOffset *
            globalTransformation *
            mGlobalInverseTransform;
    }

    for (int childIndex : node.children)
    {
        ComputeBoneHierarchyCached(animationTime, childIndex, globalTransformation, animCache);
    }
}

void Mesh::CalcInterpolatedPosition(
    Vector3& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumPositionKeys == 0)
    {
        outVec.Set(0.0f, 0.0f, 0.0f);
        return;
    }

    if (pNodeAnim->mNumPositionKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mPositionKeys[0].mValue.x,
            pNodeAnim->mPositionKeys[0].mValue.y,
            pNodeAnim->mPositionKeys[0].mValue.z);
        return;
    }

    const unsigned int index = FindPosition(animationTime, pNodeAnim);
    const unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumPositionKeys);

    const float t0 = static_cast<float>(pNodeAnim->mPositionKeys[index].mTime);
    const float t1 = static_cast<float>(pNodeAnim->mPositionKeys[nextIndex].mTime);
    const float deltaTime = t1 - t0;

    if (Math::NearZero(deltaTime))
    {
        const aiVector3D& v = pNodeAnim->mPositionKeys[index].mValue;
        outVec.Set(v.x, v.y, v.z);
        return;
    }

    float factor = (animationTime - t0) / deltaTime;
    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiVector3D& start = pNodeAnim->mPositionKeys[index].mValue;
    const aiVector3D& end   = pNodeAnim->mPositionKeys[nextIndex].mValue;

    const aiVector3D delta  = end - start;
    const aiVector3D result = start + factor * delta;

    outVec.Set(result.x, result.y, result.z);
}

void Mesh::CalcInterpolatedRotation(
    Quaternion& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumRotationKeys == 0)
    {
        outVec = Quaternion::Identity;
        return;
    }

    if (pNodeAnim->mNumRotationKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mRotationKeys[0].mValue.x,
            pNodeAnim->mRotationKeys[0].mValue.y,
            pNodeAnim->mRotationKeys[0].mValue.z,
            pNodeAnim->mRotationKeys[0].mValue.w);
        outVec.Normalize();
        return;
    }

    const unsigned int index = FindRotation(animationTime, pNodeAnim);
    const unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumRotationKeys);

    const float t0 = static_cast<float>(pNodeAnim->mRotationKeys[index].mTime);
    const float t1 = static_cast<float>(pNodeAnim->mRotationKeys[nextIndex].mTime);
    const float deltaTime = t1 - t0;

    if (Math::NearZero(deltaTime))
    {
        const aiQuaternion& q = pNodeAnim->mRotationKeys[index].mValue;
        outVec.Set(q.x, q.y, q.z, q.w);
        outVec.Normalize();
        return;
    }

    float factor = (animationTime - t0) / deltaTime;
    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiQuaternion& start = pNodeAnim->mRotationKeys[index].mValue;
    const aiQuaternion& end   = pNodeAnim->mRotationKeys[nextIndex].mValue;

    aiQuaternion q;
    aiQuaternion::Interpolate(q, start, end, factor);
    q.Normalize();

    outVec.Set(q.x, q.y, q.z, q.w);
    outVec.Normalize();
}

void Mesh::CalcInterpolatedScaling(
    Vector3& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumScalingKeys == 0)
    {
        outVec.Set(1.0f, 1.0f, 1.0f);
        return;
    }

    if (pNodeAnim->mNumScalingKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mScalingKeys[0].mValue.x,
            pNodeAnim->mScalingKeys[0].mValue.y,
            pNodeAnim->mScalingKeys[0].mValue.z);
        return;
    }

    const unsigned int index = FindScaling(animationTime, pNodeAnim);
    const unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumScalingKeys);

    const float t0 = static_cast<float>(pNodeAnim->mScalingKeys[index].mTime);
    const float t1 = static_cast<float>(pNodeAnim->mScalingKeys[nextIndex].mTime);
    const float deltaTime = t1 - t0;

    if (Math::NearZero(deltaTime))
    {
        const aiVector3D& v = pNodeAnim->mScalingKeys[index].mValue;
        outVec.Set(v.x, v.y, v.z);
        return;
    }

    float factor = (animationTime - t0) / deltaTime;
    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiVector3D& start = pNodeAnim->mScalingKeys[index].mValue;
    const aiVector3D& end   = pNodeAnim->mScalingKeys[nextIndex].mValue;

    const aiVector3D delta = end - start;
    const aiVector3D sc    = start + factor * delta;

    outVec.Set(sc.x, sc.y, sc.z);
}

unsigned int Mesh::FindPosition(float animationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumPositionKeys > 0);

    if (pNodeAnim->mNumPositionKeys <= 1)
    {
        return 0;
    }

    const aiVectorKey* begin = pNodeAnim->mPositionKeys;
    const aiVectorKey* end   = begin + pNodeAnim->mNumPositionKeys;

    auto it = std::upper_bound(
        begin, end,
        static_cast<double>(animationTime),
        [](double t, const aiVectorKey& key)
        {
            return t < key.mTime;
        });

    if (it == begin)
    {
        return 0;
    }

    const unsigned int index = static_cast<unsigned int>((it - begin) - 1);
    return std::min(index, pNodeAnim->mNumPositionKeys - 2);
}

unsigned int Mesh::FindRotation(float animationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumRotationKeys > 0);

    if (pNodeAnim->mNumRotationKeys <= 1)
    {
        return 0;
    }

    const aiQuatKey* begin = pNodeAnim->mRotationKeys;
    const aiQuatKey* end   = begin + pNodeAnim->mNumRotationKeys;

    auto it = std::upper_bound(
        begin, end,
        static_cast<double>(animationTime),
        [](double t, const aiQuatKey& key)
        {
            return t < key.mTime;
        });

    if (it == begin)
    {
        return 0;
    }

    const unsigned int index = static_cast<unsigned int>((it - begin) - 1);
    return std::min(index, pNodeAnim->mNumRotationKeys - 2);
}

unsigned int Mesh::FindScaling(float animationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumScalingKeys > 0);

    if (pNodeAnim->mNumScalingKeys <= 1)
    {
        return 0;
    }

    const aiVectorKey* begin = pNodeAnim->mScalingKeys;
    const aiVectorKey* end   = begin + pNodeAnim->mNumScalingKeys;

    auto it = std::upper_bound(
        begin, end,
        static_cast<double>(animationTime),
        [](double t, const aiVectorKey& key)
        {
            return t < key.mTime;
        });

    if (it == begin)
    {
        return 0;
    }

    const unsigned int index = static_cast<unsigned int>((it - begin) - 1);
    return std::min(index, pNodeAnim->mNumScalingKeys - 2);
}

void Mesh::LoadBones(const aiMesh* m, std::vector<VertexBoneData>& bones)
{
    for (unsigned int i = 0; i < m->mNumBones; ++i)
    {
        unsigned int boneIndex = 0;
        std::string boneName(m->mBones[i]->mName.C_Str());

        auto it = mBoneMapping.find(boneName);
        if (it == mBoneMapping.end())
        {
            boneIndex = mNumBones;
            ++mNumBones;

            BoneInfo bi;
            mBoneInfo.push_back(bi);

            Matrix4 mat;
            MatrixAi2Gl(mat, m->mBones[i]->mOffsetMatrix);
            mBoneInfo[boneIndex].BoneOffset = mat;

            mBoneMapping.emplace(boneName, boneIndex);
        }
        else
        {
            boneIndex = it->second;
        }

        for (unsigned int j = 0; j < m->mBones[i]->mNumWeights; ++j)
        {
            const unsigned int vertexID = m->mBones[i]->mWeights[j].mVertexId;
            const float        weight   = m->mBones[i]->mWeights[j].mWeight;

            if (vertexID >= bones.size())
            {
                continue;
            }
            if (weight < mMinBoneWeightThreshold)
            {
                continue;
            }

            bones[vertexID].AddBoneData(boneIndex, weight);
        }
    }
}

void Mesh::CreateMeshBone(const aiMesh* m)
{
    std::vector<float>         vertexBuffer;
    std::vector<float>         normalBuffer;
    std::vector<float>         uvBuffer;
    std::vector<unsigned int>  boneIDs;
    std::vector<float>         boneWeights;
    std::vector<unsigned int>  indexBuffer;

    vertexBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 3);
    normalBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 3);
    uvBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 2);
    boneIDs.reserve(static_cast<size_t>(m->mNumVertices) * NUM_BONES_PER_VERTEX);
    boneWeights.reserve(static_cast<size_t>(m->mNumVertices) * NUM_BONES_PER_VERTEX);
    indexBuffer.reserve(static_cast<size_t>(m->mNumFaces) * 3);

    std::vector<VertexBoneData> bones;
    bones.resize(m->mNumVertices);

    LoadBones(m, bones);

    for (unsigned int i = 0; i < m->mNumVertices; ++i)
    {
        NormalizeBoneWeights(bones[i], mMinBoneWeightThreshold);

        vertexBuffer.push_back(m->mVertices[i].x);
        vertexBuffer.push_back(m->mVertices[i].y);
        vertexBuffer.push_back(m->mVertices[i].z);

        if (m->HasNormals())
        {
            normalBuffer.push_back(m->mNormals[i].x);
            normalBuffer.push_back(m->mNormals[i].y);
            normalBuffer.push_back(m->mNormals[i].z);
        }
        else
        {
            normalBuffer.push_back(0.0f);
            normalBuffer.push_back(0.0f);
            normalBuffer.push_back(1.0f);
        }

        if (m->HasTextureCoords(0))
        {
            uvBuffer.push_back(m->mTextureCoords[0][i].x);
            uvBuffer.push_back(m->mTextureCoords[0][i].y);
        }
        else
        {
            uvBuffer.push_back(0.0f);
            uvBuffer.push_back(0.0f);
        }

        for (unsigned int b = 0; b < NUM_BONES_PER_VERTEX; ++b)
        {
            boneIDs.push_back(bones[i].IDs[b]);
            boneWeights.push_back(bones[i].Weights[b]);
        }
    }

    for (unsigned int i = 0; i < m->mNumFaces; ++i)
    {
        const aiFace& face = m->mFaces[i];
        assert(face.mNumIndices == 3);
        indexBuffer.push_back(face.mIndices[0]);
        indexBuffer.push_back(face.mIndices[1]);
        indexBuffer.push_back(face.mIndices[2]);
    }

    mVertexArray.push_back(
        std::make_shared<VertexArray>(
            static_cast<unsigned int>(vertexBuffer.size()) / 3,
            vertexBuffer.data(),
            normalBuffer.data(),
            uvBuffer.data(),
            boneIDs.data(),
            boneWeights.data(),
            static_cast<unsigned int>(indexBuffer.size()),
            indexBuffer.data()));

    mVertexArray.back()->SetTextureID(m->mMaterialIndex);
}

void Mesh::CreateMesh(const aiMesh* m)
{
    std::vector<float>         vertexBuffer;
    std::vector<float>         normalBuffer;
    std::vector<float>         uvBuffer;
    std::vector<unsigned int>  indexBuffer;

    vertexBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 3);
    normalBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 3);
    uvBuffer.reserve(static_cast<size_t>(m->mNumVertices) * 2);
    indexBuffer.reserve(static_cast<size_t>(m->mNumFaces) * 3);

    for (unsigned int i = 0; i < m->mNumVertices; ++i)
    {
        vertexBuffer.push_back(m->mVertices[i].x);
        vertexBuffer.push_back(m->mVertices[i].y);
        vertexBuffer.push_back(m->mVertices[i].z);

        if (m->HasNormals())
        {
            normalBuffer.push_back(m->mNormals[i].x);
            normalBuffer.push_back(m->mNormals[i].y);
            normalBuffer.push_back(m->mNormals[i].z);
        }
        else
        {
            normalBuffer.push_back(0.0f);
            normalBuffer.push_back(0.0f);
            normalBuffer.push_back(1.0f);
        }

        if (m->HasTextureCoords(0))
        {
            uvBuffer.push_back(m->mTextureCoords[0][i].x);
            uvBuffer.push_back(m->mTextureCoords[0][i].y);
        }
        else
        {
            uvBuffer.push_back(0.0f);
            uvBuffer.push_back(0.0f);
        }
    }

    for (unsigned int i = 0; i < m->mNumFaces; ++i)
    {
        const aiFace& face = m->mFaces[i];
        assert(face.mNumIndices == 3);
        indexBuffer.push_back(face.mIndices[0]);
        indexBuffer.push_back(face.mIndices[1]);
        indexBuffer.push_back(face.mIndices[2]);
    }

    mVertexArray.push_back(
        std::make_shared<VertexArray>(
            static_cast<unsigned int>(vertexBuffer.size()) / 3,
            vertexBuffer.data(),
            normalBuffer.data(),
            uvBuffer.data(),
            static_cast<unsigned int>(indexBuffer.size()),
            indexBuffer.data()));

    mVertexArray.back()->SetTextureID(m->mMaterialIndex);
}

bool Mesh::Load(const std::string& fileName,
                AssetManager* assetMamager,
                bool isRightHanded)
{
    Unload();

    unsigned int ASSIMP_LOAD_FLAGS =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes;

    if (isRightHanded)
    {
        ASSIMP_LOAD_FLAGS |= aiProcess_FlipWindingOrder;
    }
    else
    {
        ASSIMP_LOAD_FLAGS |= aiProcess_MakeLeftHanded;
    }

    const std::string fullName = assetMamager->GetAssetsPath() + fileName;
    mScene = mImporter.ReadFile(fullName, ASSIMP_LOAD_FLAGS);
    if (!mScene)
    {
        std::cerr << "Assimp Load Error: " << mImporter.GetErrorString() << std::endl;
        return false;
    }

    aiMatrix4x4 inv = mScene->mRootNode->mTransformation;
    inv = inv.Inverse();
    MatrixAi2Gl(mGlobalInverseTransform, inv);

    mVertexArray.reserve(mScene->mNumMeshes);
    mMaterials.reserve(mScene->mNumMaterials);
    mAnimationClips.reserve(mScene->mNumAnimations);
    mAnimationCaches.reserve(mScene->mNumAnimations);

    LoadMeshData();
    BuildEvalNodes();
    LoadMaterials(assetMamager, fileName);
    LoadAnimations();

    return true;
}

void Mesh::LoadMeshData()
{
    if (!mScene)
    {
        return;
    }

    bool hasSkinnedMesh = false;

    for (unsigned int i = 0; i < mScene->mNumMeshes; ++i)
    {
        aiMesh* m = mScene->mMeshes[i];

        if (m->HasBones())
        {
            CreateMeshBone(m);
            hasSkinnedMesh = true;
            //std::cerr << "[Mesh] Boned Mesh Loaded : " << i << std::endl;
        }
        else
        {
            CreateMesh(m);
            //std::cerr << "[Mesh] NoBone Mesh Loaded : " << i << std::endl;
        }
    }

    mShaderName = hasSkinnedMesh ? "Skinned" : "Mesh";
}

void Mesh::LoadMaterials(AssetManager* assetManager, const std::string& meshFilename)
{
    if (!mScene)
    {
        return;
    }

    for (unsigned int i = 0; i < mScene->mNumMaterials; ++i)
    {
        aiMaterial* pMaterial = mScene->mMaterials[i];
        std::shared_ptr<Material> mat = std::make_shared<Material>();

        aiColor3D color(0.0f, 0.0f, 0.0f);

        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color))
        {
            mat->SetAmbientColor(Vector3(color.r, color.g, color.b));
        }
        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color))
        {
            mat->SetDiffuseColor(Vector3(color.r, color.g, color.b));
        }
        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color))
        {
            mat->SetSpecularColor(Vector3(color.r, color.g, color.b));
        }

        float shininess = 32.0f;
        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_SHININESS, shininess))
        {
            mat->SetSpecPower(shininess);
        }

        aiString path;
        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
        {
            const std::string texPath = path.C_Str();

            if (!texPath.empty() && texPath[0] == '*')
            {
                const int index = std::atoi(texPath.c_str() + 1);
                if (index >= 0 && index < static_cast<int>(mScene->mNumTextures))
                {
                    aiTexture* aiTex = mScene->mTextures[index];
                    const std::string key = "_EMBED_" + std::to_string(index);

                    const uint8_t* imageData =
                        reinterpret_cast<const uint8_t*>(aiTex->pcData);
                    const size_t imageSize = (aiTex->mHeight == 0)
                        ? aiTex->mWidth
                        : static_cast<size_t>(aiTex->mWidth) *
                          static_cast<size_t>(aiTex->mHeight) * 4;

                    auto tex = assetManager->GetEmbeddedTexture(key, imageData, imageSize);
                    if (tex)
                    {
                        mat->SetDiffuseMap(tex);
                    }
                }
            }
            else
            {
                const std::string meshPath = StringUtil::GetDirectory(meshFilename);
                auto tex = assetManager->GetTexture(meshPath + texPath);
                if (tex)
                {
                    mat->SetDiffuseMap(tex);
                }
            }

            mat->SetUseTexture(true);
        }
        else
        {
            auto tex = assetManager->GetWhite1x1Texture();
            if (tex)
            {
                mat->SetDiffuseMap(tex);
                mat->SetUseTexture(false);
            }
        }

        mMaterials.push_back(mat);
    }
}

void Mesh::LoadAnimations()
{
    mAnimationClips.clear();
    mAnimationCaches.clear();
    mAnimationCacheIndex.clear();

    if (!mScene || mScene->mNumAnimations == 0)
    {
        //std::cerr << "[Mesh] No animations found in scene." << std::endl;
        return;
    }

    //std::cerr << "[Mesh] Found " << mScene->mNumAnimations << " animation(s)." << std::endl;

    for (unsigned int i = 0; i < mScene->mNumAnimations; ++i)
    {
        const aiAnimation* anim = mScene->mAnimations[i];

        AnimationClip clip;
        clip.mAnimation      = anim;
        clip.mName           = anim->mName.C_Str();
        clip.mDuration       = static_cast<float>(anim->mDuration);
        clip.mTicksPerSecond = (anim->mTicksPerSecond != 0.0)
            ? static_cast<float>(anim->mTicksPerSecond)
            : 25.0f;

        mAnimationClips.emplace_back(clip);

        AnimationCache cache;
        cache.animation = anim;
        cache.channelsByEvalNode.resize(mEvalNodes.size(), nullptr);

        std::unordered_map<std::string, const aiNodeAnim*> channelMap;
        channelMap.reserve(anim->mNumChannels);

        for (unsigned int ch = 0; ch < anim->mNumChannels; ++ch)
        {
            const aiNodeAnim* nodeAnim = anim->mChannels[ch];
            channelMap.emplace(nodeAnim->mNodeName.C_Str(), nodeAnim);
        }

        for (size_t nodeIndex = 0; nodeIndex < mEvalNodes.size(); ++nodeIndex)
        {
            auto it = channelMap.find(mEvalNodes[nodeIndex].name);
            if (it != channelMap.end())
            {
                cache.channelsByEvalNode[nodeIndex] = it->second;
            }
        }

        mAnimationCacheIndex.emplace(anim, mAnimationCaches.size());
        mAnimationCaches.emplace_back(std::move(cache));
    }
}

void Mesh::Unload()
{
    mScene = nullptr;

    for (auto& v : mVertexArray)
    {
        if (v)
        {
            v->Unload();
        }
    }

    mVertexArray.clear();
    mMaterials.clear();
    mAnimationClips.clear();
    mEvalNodes.clear();
    mAnimationCaches.clear();
    mAnimationCacheIndex.clear();
    mBoneInfo.clear();
    mBoneMapping.clear();

    mNumBones = 0;
    mRootEvalNodeIndex = -1;
    mShaderName.clear();
    mSpecPower = 1.0f;
    mGlobalInverseTransform = Matrix4::Identity;
}

std::shared_ptr<Material> Mesh::GetMaterial(size_t index)
{
    if (index < mMaterials.size())
    {
        return mMaterials[index];
    }
    return nullptr;
}

void Mesh::ComputePoseAtTime(
    float animationTime,
    const aiAnimation* pAnimation,
    std::vector<Matrix4>& outTransforms)
{
    if (!mScene || !pAnimation || mRootEvalNodeIndex < 0)
    {
        outTransforms.clear();
        return;
    }

    const Mesh::AnimationCache* animCache = FindAnimationCache(pAnimation);

    const Matrix4 identity = Matrix4::Identity;
    ComputeBoneHierarchyCached(animationTime, mRootEvalNodeIndex, identity, animCache);

    if (outTransforms.size() != mNumBones)
    {
        outTransforms.resize(mNumBones);
    }

    for (unsigned int i = 0; i < mNumBones; ++i)
    {
        outTransforms[i] = mBoneInfo[i].FinalTransformation;
    }
}

} // namespace toy
