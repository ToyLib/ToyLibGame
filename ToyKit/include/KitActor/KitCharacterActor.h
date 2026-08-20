#pragma once

#include "Engine/Core/Actor.h"
#include "Physics/ColliderFlags.h"
#include "ToyLib.h"
#include <string>

namespace toy::kit {

//=============================================================================
// KitCharacterActor
//
//  RPG / アクションゲームのキャラクター（プレイヤー・NPC・敵）に共通する
//  「ボイラープレート」を吸収する基底クラス。
//
//  【提供するもの】
//    - SetupMesh / SetupCollider / SetupGravity  : セットアップヘルパー
//    - SetupTargetSprites                        : candidate/locked 足元表示の自動管理
//    - SetupNameBoard                            : 名前ビルボード（別 Actor）の自動管理
//    - mMesh / mCollider / mGravity              : 派生クラスからアクセス可能な参照
//
//  【派生クラスの書き方】
//    コンストラクタで Setup* 系を呼んでコンポーネントを揃える。
//    行動ロジックは UpdateCharacter() に書く。
//
//    class WolfActor : public toy::kit::KitCharacterActor
//    {
//    public:
//        WolfActor(toy::Application* a);
//    protected:
//        void UpdateCharacter(float dt) override { mFSM.Update(dt); }
//    private:
//        toy::kit::KitStateMachine<WolfState> mFSM;
//    };
//
//  【ターゲット表示の自動管理】
//    SetupTargetSprites() を呼ぶと、毎フレーム ColliderComponent の
//    TargetState を見て candidate/locked スプライトの表示を自動切換えする。
//    派生クラスは SetVisible を自分で書かなくてよい。
//
//  【名前ビルボードの自動管理】
//    SetupNameBoard() を呼ぶと別 Actor を内部で生成し、毎フレーム
//    キャラクターの位置に追従させる。デストラクタで自動的に Dead 化する。
//=============================================================================

class KitCharacterActor : public toy::Actor
{
public:
    KitCharacterActor(toy::Application* a);
    ~KitCharacterActor() override;

    // UpdateActor はここで終端し、UpdateCharacter を呼ぶ（派生はそちらを override）
    void UpdateActor(float deltaTime) final;

protected:
    //=========================================================================
    // 行動フック（派生クラスが override して行動ロジックを書く）
    //=========================================================================
    virtual void UpdateCharacter(float deltaTime) {}

    //=========================================================================
    // セットアップヘルパー
    //  コンストラクタの中から必要なものだけ呼ぶ（すべて省略可）
    //=========================================================================

    // スケルタルメッシュを生成して mMesh にセットする
    //  order : 描画優先度
    toy::SkeletalMeshComponent* SetupMesh(const std::string& meshPath,
                                          int order = 100);

    // コライダーを生成して mCollider にセットする
    //  - mesh からバウンディングボリュームを自動計算する
    //  - bboxOffset / bboxScale で細かく調整できる（デフォルトは調整なし）
    toy::ColliderComponent* SetupCollider(
        toy::SkeletalMeshComponent* mesh,
        int            flags,
        const Vector3& bboxOffset = Vector3::Zero,
        const Vector3& bboxScale  = Vector3::One);

    // 重力コンポーネントを生成して mGravity にセットする
    //  - 戻り値のポインタで追加カスタマイズ可能
    toy::GravityComponent* SetupGravity();

    // ターゲット候補 / ロックオン時の足元スプライトを設定する
    //  candidateTex : SensorComponent に捕捉されたときのテクスチャ
    //  lockedTex    : ロックオンされたときのテクスチャ（空文字なら非表示のまま）
    //  ※ SetupCollider より後に呼ぶこと
    void SetupTargetSprites(const std::string& candidateTex,
                            const std::string& lockedTex = "");

    // キャラクターの頭上に名前ビルボードを設置する
    //  - 内部で toy::Actor を新たに生成し、デストラクタで自動 Dead 化する
    //  - 毎フレーム自動追従するので Actor::SetPosition を自分で書かなくてよい
    void SetupNameBoard(const std::string& name,
                        const std::string& fontPath,
                        float              yOffset = 4.0f,
                        const Vector3&     color   = Vector3(1.0f, 0.0f, 0.0f));

    //=========================================================================
    // 派生クラスからアクセス可能なコンポーネント参照
    //  Setup* 系を呼んだときだけ非 nullptr になる
    //=========================================================================
    toy::SkeletalMeshComponent* mMesh     = nullptr;
    toy::ColliderComponent*     mCollider = nullptr;
    toy::GravityComponent*      mGravity  = nullptr;

private:
    //=========================================================================
    // 内部処理（毎フレーム UpdateActor から呼ばれる）
    //=========================================================================
    void UpdateTargetSprites();
    void UpdateNameBoard();

    toy::GroundConformSpriteComponent* CreateTargetSprite(const std::string& texPath);

    // ターゲット表示スプライト
    toy::GroundConformSpriteComponent* mCandidateSigne = nullptr;
    toy::GroundConformSpriteComponent* mLockedSigne    = nullptr;

    // 名前ビルボード用別 Actor
    toy::Actor* mNameActor   = nullptr;
    float       mNameYOffset = 4.0f;
};

} // namespace toy::kit
