#pragma once

#include "Utils/MathUtil.h"
#include "Asset/Animation/AnimationClip.h"

#include <vector>
#include <string>
#include <map>
#include <memory>

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
    // コピー禁止（AssetManager 経由の shared_ptr 前提）
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // メッシュデータ読み込み（頂点/インデックス、ボーン有無の判定）
    void LoadMeshData();

    // マテリアル読み込み
    void LoadMaterials(class AssetManager* assetMamager, const std::string& meshFilename);

    // アニメーションクリップ読み込み
    void LoadAnimations();

    // 通常メッシュ生成（ボーンなし）
    void CreateMesh(const aiMesh* m);

    // スキンメッシュ生成（ボーンあり）
    void CreateMeshBone(const aiMesh* m);

    // 単一 aiMesh のボーン情報を収集
    void LoadBones(const aiMesh* m, std::vector<struct VertexBoneData>& bones);

    // ボーン階層を再帰的に巡回してボーン行列を計算
    void ComputeBoneHierarchy(float animationTime,
                              const aiNode* pNode,
                              const Matrix4& parentTransform,
                              const aiAnimation* pAnimation);

    // ノード名に一致するアニメーションチャネルを探す
    const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation,
                                   const std::string& nodeName);

    // 補間計算（スケール / 回転 / 平行移動）
    void CalcInterpolatedScaling(Vector3& outVec,
                                 float animationTime,
                                 const aiNodeAnim* pNodeAnim);

    void CalcInterpolatedRotation(Quaternion& outQuat,
                                  float animationTime,
                                  const aiNodeAnim* pNodeAnim);

    void CalcInterpolatedPosition(Vector3& outVec,
                                  float animationTime,
                                  const aiNodeAnim* pNodeAnim);

    // 補間に使うキーインデックスを探す
    unsigned int FindScaling(float animationTime,
                             const aiNodeAnim* pNodeAnim);
    unsigned int FindRotation(float animationTime,
                              const aiNodeAnim* pNodeAnim);
    unsigned int FindPosition(float animationTime,
                              const aiNodeAnim* pNodeAnim);

private:
    // Assimp シーンデータ
    Assimp::Importer mImporter;
    const aiScene*   mScene;

    // ボーン名 → ボーンインデックス
    std::map<std::string, unsigned int> mBoneMapping;

    // ボーン数
    unsigned int mNumBones;

    // ボーンごとのオフセット行列・最終変換行列
    std::vector<struct BoneInfo> mBoneInfo;

    // ルートノードの逆変換行列
    Matrix4 mGlobalInverseTransform;

    // 頂点配列（1ファイルに複数メッシュがある場合も考慮）
    std::vector<std::shared_ptr<class VertexArray>> mVertexArray;

    // マテリアル一覧（VertexArray の MaterialIndex と対応）
    std::vector<std::shared_ptr<class Material>> mMaterials;

    // アニメーションクリップ一覧
    std::vector<class AnimationClip> mAnimationClips;

    // 使用シェーダー名（例："Mesh", "Skinned"）
    std::string mShaderName;

    // スペキュラー係数
    float mSpecPower;
};

} // namespace toy
