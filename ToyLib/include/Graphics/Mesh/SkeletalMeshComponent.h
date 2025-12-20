#pragma once
#include "Engine/Runtime/AnimationPlayer.h"
#include "Utils/MathUtil.h"
#include "Graphics/Mesh/MeshComponent.h"
#include <vector>
#include <memory>

// スキニング用ボーンの最大数（Shader側と合わせる）
const size_t MAX_SKELETON_BONES = 96;

namespace toy {

//------------------------------------------------------------
// SkeletalMeshComponent
//  - ボーンアニメーション付きメッシュの描画コンポーネント
//  - AnimationPlayer と Mesh のボーン情報を組み合わせて
//    スキニング行列をシェーダに渡す役割
//------------------------------------------------------------
class SkeletalMeshComponent : public MeshComponent
{
public:
    //--------------------------------------------------------
    // コンストラクタ
    //  - 基本は MeshComponent と同じだが
    //    mIsSkeletal = true の前提で使うクラス
    //--------------------------------------------------------
    SkeletalMeshComponent(class Actor* a,
                          int drawOrder = 100,
                          VisualLayer layer = VisualLayer::Effect3D);
    
    //--------------------------------------------------------
    // 描画
    //  - MeshComponent::Draw を拡張して
    //    スキニング用のボーン行列をシェーダへ送る
    //--------------------------------------------------------
    void Draw() override;
    void DrawShadow(int cascadeIndex) override;
    
    //--------------------------------------------------------
    // Update
    //  - AnimationPlayer の再生時間を進めて
    //    ボーン姿勢を更新
    //--------------------------------------------------------
    void Update(float deltaTime) override;
    
    //--------------------------------------------------------
    // SetAnimID
    //  - 再生するアニメーションの ID を指定
    //  - mode などの切り替えフラグも MeshComponent 経由で
    //    共通インターフェースを確保している
    //--------------------------------------------------------
    void SetAnimID(const unsigned int animID, const bool mode) override;
    
    //--------------------------------------------------------
    // SetMesh
    //  - スケルタルメッシュ用 Mesh をセット
    //  - ボーン構造やアニメーション情報を AnimationPlayer にも
    //    連携する想定
    //--------------------------------------------------------
    void SetMesh(std::shared_ptr<class Mesh> m) override;
    
    //--------------------------------------------------------
    // AnimationPlayer 取得
    //  - 外部から細かく再生制御したい場合に使用
    //--------------------------------------------------------
    class AnimationPlayer* GetAnimPlayer() { return mAnimPlayer.get(); }
    
private:
    // 現在のアニメーション再生時間（秒）
    float mAnimTime;
    
    // アニメーション再生制御クラス
    std::unique_ptr<class AnimationPlayer> mAnimPlayer;
};

} // namespace toy
