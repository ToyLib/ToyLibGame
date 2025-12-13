#include "Asset/Geometry/Mesh.h"
#include "Asset/Material/Texture.h"
#include "Asset/AssetManager.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Bone.h"
#include "Asset/Geometry/Polygon.h"
#include "Asset/Material/Material.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <string>

//==============================================================
// aiMatrix4x4 → ToyLib::Matrix4 変換
// 左手座標系（OpenGL想定の列優先）でそのまま値を写す。
// Assimp の行列は右手系前提だが、ここでは「値を転写」。
// 実際の左/右手系の決定は aiProcess_MakeLeftHanded や
// aiProcess_FlipWindingOrder による。
//==============================================================
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

//==============================================================
// Mesh 本体
//==============================================================
namespace toy {

Mesh::Mesh()
    : mScene(nullptr)
    , mNumBones(0)
    , mSpecPower(1.0f)
{
}

Mesh::~Mesh()
{
    // shared_ptr がクリアされれば VAO は自動解放
    mVertexArray.clear();
}

//==============================================================
// ボーン階層を辿って FinalTransformation を計算
//
// 左手座標系（OpenGL）で、行列乗算順は ToyLib の Matrix4 に合わせる。
// ToyLib は「M * V」で適用されるため local * parent。
//==============================================================
void Mesh::ComputeBoneHierarchy(
    float animationTime,
    const aiNode* pNode,
    const Matrix4& parentTransform,
    const aiAnimation* pAnimation)
{
    std::string nodeName(pNode->mName.data);

    // ノードのデフォルト変換
    Matrix4 nodeTransformation;
    MatrixAi2Gl(nodeTransformation, pNode->mTransformation);

    // 対応するアニメーションチャネル（位置/回転/スケール）
    const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, nodeName);
    if (pNodeAnim)
    {
        // --- スケール ---
        Vector3 scaling;
        CalcInterpolatedScaling(scaling, animationTime, pNodeAnim);
        Matrix4 scalingM = Matrix4::CreateScale(scaling);

        // --- 回転（クォータニオン補間） ---
        Quaternion rotationQ;
        CalcInterpolatedRotation(rotationQ, animationTime, pNodeAnim);
        Matrix4 rotationM = Matrix4::CreateFromQuaternion(rotationQ);

        // --- 平行移動 ---
        Vector3 translation;
        CalcInterpolatedPosition(translation, animationTime, pNodeAnim);
        Matrix4 translationM = Matrix4::CreateTranslation(translation);

        // ※ 左手・右手の最終的な系は事前処理フラグで調整済み。
        nodeTransformation = rotationM * translationM * scalingM;
    }

    // 親のグローバル変換と合成
    Matrix4 globalTransformation = nodeTransformation * parentTransform;

    // ボーンとして登録されているノードなら FinalTransformation を計算
    if (mBoneMapping.find(nodeName) != mBoneMapping.end())
    {
        unsigned int boneIndex = mBoneMapping[nodeName];

        // Final = BoneOffset * Global * InvRoot
        //
        // BoneOffset : メッシュローカル座標 → ボーンローカル座標の変換
        // Global     : ボーンの世界行列
        // InvRoot    : ルートノードの逆変換
        //
        mBoneInfo[boneIndex].FinalTransformation =
            mBoneInfo[boneIndex].BoneOffset *
            globalTransformation *
            mGlobalInverseTransform;
    }

    // 子ノードを再帰処理
    for (unsigned int i = 0; i < pNode->mNumChildren; i++)
    {
        ComputeBoneHierarchy(
            animationTime,
            pNode->mChildren[i],
            globalTransformation,
            pAnimation);
    }
}

//==============================================================
// ノード名に一致するアニメーションチャネルを検索
//==============================================================
const aiNodeAnim* Mesh::FindNodeAnim(
    const aiAnimation* pAnimation,
    const std::string& nodeName)
{
    for (unsigned int i = 0; i < pAnimation->mNumChannels; i++)
    {
        const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
        if (std::string(pNodeAnim->mNodeName.data) == nodeName)
        {
            return pNodeAnim;
        }
    }
    return nullptr;
}

//==============================================================
// 位置キー補間
//==============================================================
void Mesh::CalcInterpolatedPosition(
    Vector3& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumPositionKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mPositionKeys[0].mValue.x,
            pNodeAnim->mPositionKeys[0].mValue.y,
            pNodeAnim->mPositionKeys[0].mValue.z);
        return;
    }

    unsigned int index = FindPosition(animationTime, pNodeAnim);
    unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumPositionKeys);

    float deltaTime = (float)(pNodeAnim->mPositionKeys[nextIndex].mTime - pNodeAnim->mPositionKeys[index].mTime);

    float factor = (animationTime -
        (float)pNodeAnim->mPositionKeys[index].mTime) / deltaTime;

    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiVector3D& start = pNodeAnim->mPositionKeys[index].mValue;
    const aiVector3D& end   = pNodeAnim->mPositionKeys[nextIndex].mValue;

    aiVector3D delta = end - start;
    aiVector3D result = start + factor * delta;

    outVec.Set(result.x, result.y, result.z);
}

//==============================================================
// 回転キー補間（球面線形補間）
//==============================================================
void Mesh::CalcInterpolatedRotation(
    Quaternion& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumRotationKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mRotationKeys[0].mValue.x,
            pNodeAnim->mRotationKeys[0].mValue.y,
            pNodeAnim->mRotationKeys[0].mValue.z,
            pNodeAnim->mRotationKeys[0].mValue.w);
        return;
    }

    unsigned int index = FindRotation(animationTime, pNodeAnim);
    unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumRotationKeys);

    float deltaTime =
        (float)(pNodeAnim->mRotationKeys[nextIndex].mTime -
                pNodeAnim->mRotationKeys[index].mTime);

    float factor = (animationTime -
        (float)pNodeAnim->mRotationKeys[index].mTime) / deltaTime;

    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiQuaternion& start = pNodeAnim->mRotationKeys[index].mValue;
    const aiQuaternion& end   = pNodeAnim->mRotationKeys[nextIndex].mValue;

    aiQuaternion q;
    aiQuaternion::Interpolate(q, start, end, factor);
    q.Normalize();

    outVec.Set(q.x, q.y, q.z, q.w);
    outVec.Normalize();
}

//==============================================================
// スケールキー補間
//==============================================================
void Mesh::CalcInterpolatedScaling(
    Vector3& outVec,
    float animationTime,
    const aiNodeAnim* pNodeAnim)
{
    if (pNodeAnim->mNumScalingKeys == 1)
    {
        outVec.Set(
            pNodeAnim->mScalingKeys[0].mValue.x,
            pNodeAnim->mScalingKeys[0].mValue.y,
            pNodeAnim->mScalingKeys[0].mValue.z);
        return;
    }

    unsigned int index = FindScaling(animationTime, pNodeAnim);
    unsigned int nextIndex = index + 1;
    assert(nextIndex < pNodeAnim->mNumScalingKeys);

    float deltaTime =
        (float)(pNodeAnim->mScalingKeys[nextIndex].mTime -
                pNodeAnim->mScalingKeys[index].mTime);

    float factor = (animationTime -
        (float)pNodeAnim->mScalingKeys[index].mTime) / deltaTime;

    factor = std::clamp(factor, 0.0f, 1.0f);

    const aiVector3D& start = pNodeAnim->mScalingKeys[index].mValue;
    const aiVector3D& end   = pNodeAnim->mScalingKeys[nextIndex].mValue;

    aiVector3D delta = end - start;
    aiVector3D sc = start + factor * delta;

    outVec.Set(sc.x, sc.y, sc.z);
}

//==============================================================
// キーインデックス検索（位置）
//==============================================================
unsigned int Mesh::FindPosition(float animationTime, const aiNodeAnim* pNodeAnim)
{
    for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
    {
        if (animationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
        {
            return i;
        }
    }
    return pNodeAnim->mNumPositionKeys - 2;
}

//==============================================================
// キーインデックス検索（回転）
//==============================================================
unsigned int Mesh::FindRotation(float animationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumRotationKeys > 0);

    for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
    {
        if (animationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
        {
            return i;
        }
    }
    return pNodeAnim->mNumRotationKeys - 2;
}

//==============================================================
// キーインデックス検索（スケール）
//==============================================================
unsigned int Mesh::FindScaling(float animationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumScalingKeys > 0);

    for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
    {
        if (animationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
        {
            return i;
        }
    }
    return pNodeAnim->mNumScalingKeys - 2;
}

//==============================================================
// ボーン情報の読み込み
// 各 aiMesh の Bone 配列から VertexBoneData を埋める。
//==============================================================
void Mesh::LoadBones(const aiMesh* m, std::vector<VertexBoneData>& bones)
{
    for (unsigned int i = 0; i < m->mNumBones; i++)
    {
        unsigned int boneIndex = 0;
        std::string boneName(m->mBones[i]->mName.data);

        // 未登録ボーンなら新規インデックスを割り当て
        if (mBoneMapping.find(boneName) == mBoneMapping.end())
        {
            boneIndex = mNumBones;
            mNumBones++;

            BoneInfo bi;
            mBoneInfo.push_back(bi);

            // オフセット行列（メッシュローカル → ボーンローカル）
            Matrix4 mat;
            MatrixAi2Gl(mat, m->mBones[i]->mOffsetMatrix);
            mBoneInfo[boneIndex].BoneOffset = mat;

            mBoneMapping[boneName] = boneIndex;
        }
        else
        {
            boneIndex = mBoneMapping[boneName];
        }

        // 各頂点に BoneID / Weight を割り当てる
        for (unsigned int j = 0; j < m->mBones[i]->mNumWeights; j++)
        {
            unsigned int vertexID = m->mBones[i]->mWeights[j].mVertexId;
            float        weight   = m->mBones[i]->mWeights[j].mWeight;

            bones[vertexID].AddBoneData(boneIndex, weight);
        }
    }
}

//==============================================================
// スキンメッシュ（ボーン付き）頂点バッファ生成
//==============================================================
void Mesh::CreateMeshBone(const aiMesh* m)
{
    std::vector<float>         vertexBuffer;   // XYZ
    std::vector<float>         normalBuffer;   // XYZ
    std::vector<float>         uvBuffer;       // UV
    std::vector<unsigned int>  boneIDs;        // 4 IDs/vertex
    std::vector<float>         boneWeights;    // 4 weights/vertex
    std::vector<unsigned int>  indexBuffer;

    // 頂点数ぶんのボーンデータを確保
    std::vector<VertexBoneData> bones;
    bones.resize(m->mNumVertices);

    // ボーン → VertexBoneData へ反映
    LoadBones(m, bones);

    // 頂点属性をバッファへ詰める
    for (unsigned int i = 0; i < m->mNumVertices; i++)
    {
        // 位置
        vertexBuffer.push_back(m->mVertices[i].x);
        vertexBuffer.push_back(m->mVertices[i].y);
        vertexBuffer.push_back(m->mVertices[i].z);

        // 法線
        normalBuffer.push_back(m->mNormals[i].x);
        normalBuffer.push_back(m->mNormals[i].y);
        normalBuffer.push_back(m->mNormals[i].z);

        // UV（なければ 0,0）
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

        // ボーン ID/Weight（最大4本）
        boneIDs.push_back(bones[i].IDs[0]);
        boneIDs.push_back(bones[i].IDs[1]);
        boneIDs.push_back(bones[i].IDs[2]);
        boneIDs.push_back(bones[i].IDs[3]);

        boneWeights.push_back(bones[i].Weights[0]);
        boneWeights.push_back(bones[i].Weights[1]);
        boneWeights.push_back(bones[i].Weights[2]);
        boneWeights.push_back(bones[i].Weights[3]);
    }

    // インデックス（三角形のみ想定）
    for (unsigned int i = 0; i < m->mNumFaces; i++)
    {
        const aiFace& face = m->mFaces[i];
        assert(face.mNumIndices == 3);
        indexBuffer.push_back(face.mIndices[0]);
        indexBuffer.push_back(face.mIndices[1]);
        indexBuffer.push_back(face.mIndices[2]);
    }

    // VAO を生成
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

    // このメッシュで使うマテリアル番号を覚えておく
    mVertexArray.back()->SetTextureID(m->mMaterialIndex);
}

//==============================================================
// 通常メッシュ（ボーンなし）頂点バッファ生成
//==============================================================
void Mesh::CreateMesh(const aiMesh* m)
{
    std::vector<float>         vertexBuffer;   // XYZ
    std::vector<float>         normalBuffer;   // XYZ
    std::vector<float>         uvBuffer;       // UV
    std::vector<unsigned int>  indexBuffer;

    for (unsigned int i = 0; i < m->mNumVertices; i++)
    {
        // 位置
        vertexBuffer.push_back(m->mVertices[i].x);
        vertexBuffer.push_back(m->mVertices[i].y);
        vertexBuffer.push_back(m->mVertices[i].z);

        // 法線
        normalBuffer.push_back(m->mNormals[i].x);
        normalBuffer.push_back(m->mNormals[i].y);
        normalBuffer.push_back(m->mNormals[i].z);

        // UV（なければ 0,0）
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

    // インデックス（三角形のみ想定）
    for (unsigned int i = 0; i < m->mNumFaces; i++)
    {
        const aiFace& face = m->mFaces[i];
        assert(face.mNumIndices == 3);
        indexBuffer.push_back(face.mIndices[0]);
        indexBuffer.push_back(face.mIndices[1]);
        indexBuffer.push_back(face.mIndices[2]);
    }

    // VAO を生成
    mVertexArray.push_back(
        std::make_shared<VertexArray>(
            static_cast<unsigned int>(vertexBuffer.size()) / 3,
            vertexBuffer.data(),
            normalBuffer.data(),
            uvBuffer.data(),
            static_cast<unsigned int>(indexBuffer.size()),
            indexBuffer.data()));

    // このメッシュで使うマテリアル番号を覚えておく
    mVertexArray.back()->SetTextureID(m->mMaterialIndex);
}

//==============================================================
// メッシュ読み込み
//
// isRightHanded = true  : 右手系モデルをそのまま扱いたい場合
// isRightHanded = false : 左手系用に aiProcess_MakeLeftHanded を適用
//
// ※実際の最終的な座標系は Renderer / MathUtil の扱いに依存。
//==============================================================
bool Mesh::Load(const std::string& fileName,
                AssetManager* assetMamager,
                bool isRightHanded)
{
    unsigned int ASSIMP_LOAD_FLAGS =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes;

    // 右手系モデルを扱うなら「カリングなどで対応」するため
    // 反時計/時計の反転だけ行う。
    if (isRightHanded)
    {
        ASSIMP_LOAD_FLAGS |= aiProcess_FlipWindingOrder;
    }
    else
    {
        // 左手系に反転 (Assimp 側で行列/面の向きを変換)
        ASSIMP_LOAD_FLAGS |= aiProcess_MakeLeftHanded;
    }

    std::string fullName = assetMamager->GetAssetsPath() + fileName;
    mScene = mImporter.ReadFile(fullName, ASSIMP_LOAD_FLAGS);
    if (!mScene)
    {
        std::cerr << "Assimp Load Error: " << mImporter.GetErrorString() << std::endl;
        return false;
    }

    // ルートノードの逆変換（ボーン計算で使用）
    aiMatrix4x4 inv = mScene->mRootNode->mTransformation;
    inv = inv.Inverse();
    MatrixAi2Gl(mGlobalInverseTransform, inv);

    LoadMeshData();
    LoadMaterials(assetMamager);
    LoadAnimations();

    return true;
}

//==============================================================
// シーン中の全 aiMesh から VAO を構築
//==============================================================
void Mesh::LoadMeshData()
{
    for (int i = 0; i < static_cast<int>(mScene->mNumMeshes); i++)
    {
        aiMesh* m = mScene->mMeshes[i];

        if (m->HasBones())
        {
            CreateMeshBone(m);
        }
        else
        {
            CreateMesh(m);
        }
    }
}

//==============================================================
// マテリアル読み込み
// - Ambient / Diffuse / Specular / Shininess を Material に反映
// - Diffuse テクスチャ（外部 or 埋め込み）を読み込む
//==============================================================
void Mesh::LoadMaterials(AssetManager* assetMamager)
{
    for (unsigned int i = 0; i < mScene->mNumMaterials; i++)
    {
        aiMaterial* pMaterial = mScene->mMaterials[i];
        std::shared_ptr<Material> mat = std::make_shared<Material>();

        // 色（Ambient / Diffuse / Specular）
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

        // スペキュラー強度
        float shininess = 32.0f;
        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_SHININESS, shininess))
        {
            mat->SetSpecPower(shininess);
        }

        // Diffuse テクスチャ
        aiString path;
        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
        {
            std::string texPath = path.C_Str();

            // 埋め込みテクスチャ "*0" のような形式
            if (!texPath.empty() && texPath[0] == '*')
            {
                int index = std::atoi(texPath.c_str() + 1);
                if (index >= 0 && index < static_cast<int>(mScene->mNumTextures))
                {
                    aiTexture* aiTex = mScene->mTextures[index];
                    std::string key = "_EMBED_" + std::to_string(index);

                    const uint8_t* imageData = reinterpret_cast<const uint8_t*>(aiTex->pcData);
                    size_t imageSize = (aiTex->mHeight == 0)
                        ? aiTex->mWidth
                        : aiTex->mWidth * aiTex->mHeight * 4;

                    auto tex = assetMamager->GetEmbeddedTexture(key, imageData, imageSize);
                    if (tex)
                    {
                        mat->SetDiffuseMap(tex);
                    }
                }
            }
            else
            {
                // 通常のファイルパス
                auto tex = assetMamager->GetTexture(texPath);
                if (tex)
                {
                    mat->SetDiffuseMap(tex);
                }
            }
        }

        mMaterials.push_back(mat);
    }
}

//==============================================================
// シーン内のアニメーションを AnimationClip に変換
//==============================================================
void Mesh::LoadAnimations()
{
    mAnimationClips.clear();

    if (!mScene || mScene->mNumAnimations == 0)
    {
        std::cerr << "[Mesh] No animations found in scene." << std::endl;
        return;
    }

    std::cerr << "[Mesh] Found " << mScene->mNumAnimations << " animation(s)." << std::endl;

    for (unsigned int i = 0; i < mScene->mNumAnimations; i++)
    {
        const aiAnimation* anim = mScene->mAnimations[i];

        AnimationClip clip;
        clip.mAnimation       = anim;
        clip.mName            = anim->mName.C_Str(); // 空文字の場合もあり
        clip.mDuration        = static_cast<float>(anim->mDuration);
        clip.mTicksPerSecond  = (anim->mTicksPerSecond != 0.0)
                                  ? static_cast<float>(anim->mTicksPerSecond)
                                  : 25.0f; // TicksPerSecond が 0 の場合のデフォルト

        mAnimationClips.emplace_back(clip);
    }
}

//==============================================================
// メッシュリソース解放
//==============================================================
void Mesh::Unload()
{
    mScene = nullptr;
    mVertexArray.clear();
    mMaterials.clear();
    mAnimationClips.clear();
    mBoneInfo.clear();
    mBoneMapping.clear();
    mNumBones = 0;
}

//==============================================================
// インデックスから Material を取得
//==============================================================
std::shared_ptr<Material> Mesh::GetMaterial(size_t index)
{
    if (index < mMaterials.size())
    {
        return mMaterials[index];
    }
    return nullptr;
}

//==============================================================
// 指定アニメーションの指定時刻のボーン行列配列を計算
// outTransforms には「ボーン数ぶん」の行列が詰められる。
//==============================================================
void Mesh::ComputePoseAtTime(
    float animationTime,
    const aiAnimation* pAnimation,
    std::vector<Matrix4>& outTransforms)
{
    Matrix4 identity = Matrix4::Identity;

    // ルートノードからボーン階層を再帰的に更新
    ComputeBoneHierarchy(animationTime, mScene->mRootNode, identity, pAnimation);

    // FinalTransformation をそのまま出力配列にコピー
    outTransforms.resize(mNumBones);
    for (unsigned int i = 0; i < mNumBones; i++)
    {
        outTransforms[i] = mBoneInfo[i].FinalTransformation;
    }
}

} // namespace toy
