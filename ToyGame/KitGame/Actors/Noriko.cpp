#include "Noriko.h"

namespace {

//=============================================================================
// Desc（構築レシピ）を作る
//  Prefab（ここでは Creature）は「Desc を渡すとその通りに組み立てられる」
//  という形で作る。Desc はプリミティブ型だけで構成された、ただのデータ
//  （JSON化を見据えた形。設計方針16）。ここで値を書けば書くほど、この
//  キャラの見た目・当たり判定・演出が決まる——Prefab のコード自体
//  （Creature.cpp）はゲーム固有の値を一切知らない。
//
//  フィールドの意味は ToyKit_KitPrefab_KitSignal_Reference.md か、
//  CreatureDesc.h のコメントを参照。ここで使っているものだけ簡単に:
//    - model / scale / yawOffsetDeg : メッシュとその見た目補正
//    - colliderOffset/Scale/Flags   : 当たり判定の形と種別フラグ
//    - displayName                  : 頭上の名前ビルボード（空文字なら非表示）
//    - candidateTexture/lockedTexture : ロックオン候補/ロック中の足元スプライト
//    - enableVision/visionFovDeg/visionMaxDist/visionTargetMask
//        : 視界センサー（Humanoid のロックオン索敵と同じ toy::SensorComponent）。
//          visionTargetMask で「何を見るか」を指定する——ここでは Player の
//          コライダーが持つ toy::C_PLAYER_TEAM を指定し、Player を検知対象にする。
//=============================================================================
toy::kit::CreatureDesc MakeNorikoDesc()
{
    toy::kit::CreatureDesc desc;
    desc.model                     = "Field/noriko.glb";
    desc.scale                     = 3.0f;
    desc.yawOffsetDeg              = 180.0f;
    desc.cancelRootTranslationBone = "Hip";

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.7f, 1.0f, 0.7f);
    desc.colliderFlags  = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                         | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.displayName = "海苔子";
    desc.nameYOffset = 3.0f;

    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    desc.enableVision     = true;
    desc.visionFovDeg     = 100.0f;
    desc.visionMaxDist    = 15.0f;
    desc.visionTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

//=============================================================================
// FleeBehaviorDesc — FleeBehavior（ToyKit 汎用）の構築情報
//  基本は Idle（静止）。ターゲット（Player）を Sensor で視認したら
//  Flee（ターゲットと反対方向へ歩く）に切り替わり、一定距離より離れたら
//  Idle に戻る。ロジック本体は toy::kit::FleeBehavior（ChaseBehavior と同じ
//  位置付けの汎用 IBehavior）。元々は Noriko 専用のローカルクラスだったが、
//  Ninja が2人目の利用者になったため ToyKit 側へ昇格した
//  （「2人目の利用者が現れてから昇格する」というこのプロジェクトの方針。
//  ChaseBehavior と同じ流れ）。
//=============================================================================
toy::kit::FleeBehaviorDesc MakeNorikoFleeDesc()
{
    toy::kit::FleeBehaviorDesc desc;
    desc.loseDistance = 22.0f; // Sensorの検知距離＝visionMaxDist より少し大きめにしてヒステリシスを持たせる
    desc.moveSpeed    = 4.0f;
    desc.idleAnim     = 0;
    desc.fleeAnim     = 1;
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
// MakeNoriko — Factory 関数
//  Agent<TPrefab> のコンストラクタは (Application*, unique_ptr<IBehavior>,
//  Prefabのコンストラクタ引数...) という形（Agent.h 参照）。ここでは
//  Creature(Application*, const CreatureDesc&) 相当の第3引数として
//  MakeNorikoDesc() の戻り値を渡している。
//
//  内部の Prefab（Creature）を直接触りたいときは GetBody() 経由で行う
//  （SetPosition/SetRotation は Prefab の Interface、Agent 自体は持たない）。
//-----------------------------------------------------------------------------
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<toy::kit::FleeBehavior>(MakeNorikoFleeDesc());
    behavior->SetTarget(target);

    auto noriko = std::make_unique<Noriko>(app, std::move(behavior), MakeNorikoDesc());
    noriko->GetBody().SetPosition(position);
    Quaternion rot = Quaternion(Vector3(0.0f, 1.0f, 0.0f), Math::ToRadians(180.0f));
    noriko->GetBody().SetRotation(rot);
    return noriko;
}
