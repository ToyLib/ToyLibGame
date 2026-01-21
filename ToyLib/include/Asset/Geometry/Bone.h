#pragma once

#include "Utils/MathUtil.h"
#include <cstring>

namespace toy {

// 1頂点あたり影響を持つボーン数（最大4本）
constexpr unsigned int NUM_BONES_PER_VERTEX = 4;

//==============================================================
// VertexBoneData
//   - 1頂点に影響するボーンIDとウェイトを保持
//   - 実際の追加処理は VertexBoneData::AddBoneData() (cpp側) で行う
//==============================================================
struct VertexBoneData
{
    unsigned int IDs[NUM_BONES_PER_VERTEX]{};   // ボーンID
    float        Weights[NUM_BONES_PER_VERTEX]{}; // 各ボーンのウェイト

    VertexBoneData()
    {
        Reset();
    }

    // すべてのボーンID/ウェイトを 0 クリア
    void Reset()
    {
        for (unsigned int i = 0; i < NUM_BONES_PER_VERTEX; ++i)
        {
            IDs[i]     = 0;
            Weights[i] = 0.0f;
        }
    }

    // 頂点に影響するボーンIDとウェイトを追加
    // 実装は VertexBoneData.cpp 側
    void AddBoneData(unsigned int boneID, float weight);
};

//==============================================================
// BoneInfo
//   - 各ボーンごとの変換情報
//   - BoneOffset: モデル空間 → ボーンローカルへのオフセット行列
//   - FinalTransformation: アニメーション適用後の最終行列（スキニング用）
//==============================================================
struct BoneInfo
{
    Matrix4 BoneOffset{};          // オフセット行列（BindPoseで埋める）
    Matrix4 FinalTransformation{}; // アニメ後の最終行列（GPUに送る）

    BoneInfo()
        : BoneOffset(Matrix4::Identity)
        , FinalTransformation(Matrix4::Identity)
    {
    }
};

} // namespace toy
