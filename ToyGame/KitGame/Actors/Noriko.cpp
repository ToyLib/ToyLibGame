#include "Noriko.h"
#include "KitPrefab/MovementUtil.h"

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

    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    return desc;
}

//=============================================================================
// IdleWalkBehavior — IBehavior の実例
//  Idle（静止）→Walk（ランダムな方向へ移動）→Idle をタイマーで切り替える
//  最小限の振る舞い。Walkに入るたびに新しいランダム方向を選び直す。
//
//  IBehavior は OnStart/OnUpdate/OnInput/OnCollision の4つのフックを持つ
//  （どれも既定は空実装。使うものだけ override すればよい）。Noriko は
//  入力もぶつかり判定への反応も不要なので、ここでは OnStart/OnUpdate の
//  2つだけを使っている。
//
//  Behavior は Prefab（Creature/Humanoid等）の型を知らなくてよい設計に
//  なっている——ここで触っているのは Prefab の共通 Interface
//  （PlayAnimationBlend/SetPosition/SetRotation 等）だけで、Creature 固有の
//  メソッドは一切呼んでいない。そのため、このクラスをそのまま
//  Humanoid にも使い回せる（Prefab& を受け取る関数として書けているのが
//  ポイント）。
//
//  今のところ Noriko 専用（他の Creature/Humanoid でも使うようになったら
//  ToyKit 側の汎用 Behavior に昇格する。ChaseBehavior と同じ流れ。
//  「2人目の利用者が現れてから昇格する」というこのプロジェクトの方針）。
//=============================================================================
class IdleWalkBehavior : public toy::kit::IBehavior
{
public:
    // Agent 生成直後に一度だけ呼ばれる。ここで KitStateMachine の状態と
    // 遷移条件を登録し、初期状態へ入る（＝ゲームループの外で1回だけ行う
    // セットアップ）。
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = &body;

        // KitStateMachine<State>::Register(状態, 毎フレーム呼ばれる関数)
        //  - mFSM.IsEnterFrame() : その状態に入った最初のフレームだけ true
        //    （アニメーションブレンドや方向決定など「入った瞬間に1回だけ
        //    やりたいこと」はここに書く。毎フレーム書くと毎フレーム
        //    やり直しになってしまう）
        //  - mFSM.GetTimer()     : その状態に入ってからの経過秒数
        //  - mFSM.To(state)      : 指定した状態へ遷移する
        mFSM.Register(State::Idle,
            [this](float)
            {
                if (mFSM.IsEnterFrame()) mBody->PlayAnimationBlend(0, kAnimBlendSec);
                if (mFSM.GetTimer() > 10.0f) mFSM.To(State::Walk);
            });

        mFSM.Register(State::Walk,
            [this](float dt)
            {
                if (mFSM.IsEnterFrame())
                {
                    mBody->PlayAnimationBlend(1, kAnimBlendSec);
                    PickRandomDirection();
                }
                MoveForward(dt);
                if (mFSM.GetTimer() > 5.0f) mFSM.To(State::Idle);
            });

        mFSM.Start(State::Idle);
    }

    // 毎フレーム、Agent<Creature>::Update(dt) → この OnUpdate という順で
    // 呼ばれる（Scene 側は Agent の Update しか呼んでおらず、Behavior の
    // 中身は知らない）。
    void OnUpdate(toy::kit::Prefab& /*body*/, float deltaTime) override
    {
        mFSM.Update(deltaTime);
    }

private:
    enum class State { Idle, Walk };

    static constexpr float kAnimBlendSec = 0.3f;

    // XZ平面でランダムな方向を選び、その方向を向く（Random Walk）。
    // 実際の計算は KitPrefab/MovementUtil.h の free function に委譲している
    // ——Walk/Run（速度の違い）・Random/ToTarget（向かう先の決め方の違い）
    // で共有できるよう切り出された移動ユーティリティ（ChaseBehavior/
    // Stan の FollowBehavior も同じものを使う）。
    void PickRandomDirection()
    {
        mMoveDir = toy::kit::PickRandomDirectionXZ();
        toy::kit::FaceDirectionXZ(*mBody, mMoveDir);
    }

    // Y は重力（GravityComponent）に任せ、XZ だけ現在の向きへ進める
    void MoveForward(float dt)
    {
        toy::kit::MoveInDirectionXZ(*mBody, mMoveDir, mMoveSpeed, dt);
    }

    toy::kit::Prefab*                mBody    = nullptr;
    toy::kit::KitStateMachine<State> mFSM;
    Vector3                          mMoveDir  = Vector3::UnitZ;
    float                            mMoveSpeed = 1.0f;
};

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
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position)
{
    auto noriko = std::make_unique<Noriko>(app, std::make_unique<IdleWalkBehavior>(), MakeNorikoDesc());
    noriko->GetBody().SetPosition(position);
    Quaternion rot = Quaternion(Vector3(0.0f, 1.0f, 0.0f), Math::ToRadians(180.0f));
    noriko->GetBody().SetRotation(rot);
    return noriko;
}
