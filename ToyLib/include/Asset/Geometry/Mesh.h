#pragma once

#include "Utils/MathUtil.h"
#include "Asset/Animation/AnimationClip.h"

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace toy {

// アニメーション対応メッシュ
class Mesh
{
public:
    Mesh();
    ~Mesh();

    // メッシュファイルを読み込む
    // isRightHanded = true のとき右手系 → 左手系などの変換を行う想定
    virtual bool Load(const std::string& fileName,
                      class AssetManager* assetMamager,
                      bool isRightHanded = false);

    // メッシュとリソースの解放
    void Unload();

    // 頂点配列（VAO）を取得
    // （1つのファイル内に複数 aiMesh がある場合に対応）
    const std::vector<std::shared_ptr<class VertexArray>>& GetVertexArray() const
    {
        return mVertexArray;
    }

    // マテリアルを取得（メッシュインデックスに対応）
    std::shared_ptr<class Material> GetMaterial(size_t index);

    // 使用するシェーダー名を取得（"Mesh", "Skinned" など）
    const std::string& GetShaderName() const { return mShaderName; }

    // スペキュラー強度
    float GetSpecPower() const { return mSpecPower; }

    // Assimp のシーンアクセス（必要があれば）
    const aiScene* GetScene() const { return mScene; }

    // 指定時刻のボーン姿勢（スキンメッシュ用）を計算
    void ComputePoseAtTime(float animationTime,
                           const aiAnimation* pAnimation,
                           std::vector<Matrix4>& outTransforms);

    // 読み込まれているアニメーションクリップ一覧
    const std::vector<class AnimationClip>& GetAnimationClips() const
    {
        return mAnimationClips;
    }

    // アニメーションが1つ以上存在するか
    bool HasAnimation() const { return !mAnimationClips.empty(); }

private:
    // 先に前方宣言しておく
    struct EvalNode;
    struct AnimationCache;

private:
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void LoadMeshData();
    void LoadMaterials(class AssetManager* assetMamager, const std::string& meshFilename);
    void LoadAnimations();

    void BuildEvalNodes();
    int  BuildEvalNodeRecursive(const aiNode* node, int parentIndex);

    void CreateMesh(const aiMesh* m);
    void CreateMeshBone(const aiMesh* m);
    void LoadBones(const aiMesh* m, std::vector<struct VertexBoneData>& bones);

    void ComputeBoneHierarchyCached(float animationTime,
                                    int evalNodeIndex,
                                    const Matrix4& parentTransform,
                                    const AnimationCache* animCache);

    void CalcInterpolatedScaling(Vector3& outVec,
                                 float animationTime,
                                 const aiNodeAnim* pNodeAnim);

    void CalcInterpolatedRotation(Quaternion& outQuat,
                                  float animationTime,
                                  const aiNodeAnim* pNodeAnim);

    void CalcInterpolatedPosition(Vector3& outVec,
                                  float animationTime,
                                  const aiNodeAnim* pNodeAnim);

    unsigned int FindScaling(float animationTime,
                             const aiNodeAnim* pNodeAnim);
    unsigned int FindRotation(float animationTime,
                              const aiNodeAnim* pNodeAnim);
    unsigned int FindPosition(float animationTime,
                              const aiNodeAnim* pNodeAnim);

    const AnimationCache* FindAnimationCache(const aiAnimation* pAnimation) const;

private:
    struct EvalNode
    {
        std::string name;
        int         parentIndex { -1 };
        int         boneIndex   { -1 };
        Matrix4     defaultTransform { Matrix4::Identity };
        std::vector<int> children;
    };

    struct AnimationCache
    {
        const aiAnimation* animation { nullptr };
        std::vector<const aiNodeAnim*> channelsByEvalNode;
    };

private:
    Assimp::Importer mImporter {};
    const aiScene*   mScene { nullptr };

    std::unordered_map<std::string, uint32_t> mBoneMapping {};

    uint32_t mNumBones { 0 };
    std::vector<struct BoneInfo> mBoneInfo;

    Matrix4 mGlobalInverseTransform { Matrix4::Identity };

    std::vector<std::shared_ptr<class VertexArray>> mVertexArray;
    std::vector<std::shared_ptr<class Material>> mMaterials;
    std::vector<class AnimationClip> mAnimationClips;

    std::vector<EvalNode> mEvalNodes;
    int mRootEvalNodeIndex { -1 };

    std::vector<AnimationCache> mAnimationCaches;
    std::unordered_map<const aiAnimation*, size_t> mAnimationCacheIndex;

    std::string mShaderName;
    float mSpecPower { 1.0f };

    float mMinBoneWeightThreshold { 0.01f };
};

} // namespace toy
