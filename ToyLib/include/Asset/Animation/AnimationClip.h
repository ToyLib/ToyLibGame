#pragma once

#include <string>
#include <assimp/scene.h>

namespace toy {

//======================================================================
// AnimationClip
//   - Assimp から取得した aiAnimation 1本分をラップする軽量構造体
//   - 「クリップ名」「Assimp の生ポインタ」「長さ」「再生レート」を保持
//   - 実際のポーズ計算は Mesh::ComputePoseAtTime() などが担当
//======================================================================
struct AnimationClip
{
    // クリップ名
    //  - FBX/GLTF 内のアニメーション名や、ゲーム側での識別子を入れる
    std::string       mName;

    // Assimp のアニメーションデータ（所有権は Assimp/Mesh 側）
    //  - AnimationPlayer などがこのポインタを使ってサンプリングする
    const aiAnimation* mAnimation{nullptr};

    // アニメーションの長さ（Assimp の Tick 単位）
    //  - 実際の再生時間(sec) = mDuration / mTicksPerSecond
    float             mDuration{};

    // 1秒あたりの Tick 数
    //  - Assimp の mTicksPerSecond をキャッシュ
    float             mTicksPerSecond{};
};

} // namespace toy
