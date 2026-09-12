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
//    - enableSensor/sensorFovDeg/sensorMaxDist/sensorTargetMask
//        : 視界センサー（Humanoid のロックオン索敵と同じ toy::SensorComponent）。
//          sensorTargetMask で「何を見るか」を指定する——ここでは Player の
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

    desc.enableSensor     = true;
    desc.sensorFovDeg     = 100.0f;
    desc.sensorMaxDist    = 15.0f;
    desc.sensorTargetMask = toy::C_PLAYER_TEAM;

    return desc;
}

//=============================================================================
// FleeBehavior — IBehavior の実例
//  基本は Idle（静止）。ターゲット（Player）を Sensor で視認したら
//  Flee（ターゲットと反対方向へ歩く）に切り替わり、
//  一定距離より離れたら Idle に戻る。
//
//  検知（Idle→Flee）に Prefab::HasSensorHit() を使い、見失い（Flee→Idle）は
//  単純な距離判定にしているのは意図的：Flee 中は反対方向を向いて歩く
//  ので、前方視野の Sensor では追ってくる相手がすぐ視野外＝
//  HasSensorHit()==false になってしまい、見失い判定に使うと
//  Idle⇄Flee を毎フレーム往復してしまう。「見つけるのは視界、
//  諦めるのは距離」という非対称な組み合わせが実用上ちょうどよい。
//
//  IBehavior は OnStart/OnUpdate/OnInput/OnCollision の4つのフックを持つ
//  （どれも既定は空実装。使うものだけ override すればよい）。Noriko は
//  入力もぶつかり判定への反応も不要なので、ここでは OnStart/OnUpdate の
//  2つだけを使っている。
//
//  Behavior は Prefab（Creature/Humanoid等）の型を知らなくてよい設計に
//  なっている——ここで触っているのは Prefab の共通 Interface
//  （PlayAnimationBlend/GetPosition/HasSensorHit 等）と MovementUtil の
//  free function だけで、Creature 固有のメソッドは一切呼んでいない。
//  そのため、このクラスをそのまま Humanoid にも使い回せる（Prefab& を
//  受け取る関数として書けているのがポイント）。
//
//  今のところ Noriko 専用（他の Creature/Humanoid でも使うようになったら
//  ToyKit 側の汎用 Behavior に昇格する。ChaseBehavior と同じ流れ。
//  「2人目の利用者が現れてから昇格する」というこのプロジェクトの方針）。
//=============================================================================
class FleeBehavior : public toy::kit::IBehavior
{
public:
    // 「視界に入ったら」の相手（Player の Prefab）を設定する。
    // ChaseBehavior::SetTarget と同じく、位置だけを Prefab Interface 越しに
    // 参照する（Prefab の具体型を問わない）。
    void SetTarget(toy::kit::Prefab* target) { mTarget = target; }

    // Agent 生成直後に一度だけ呼ばれる。ここで KitStateMachine の状態と
    // 遷移条件を登録し、初期状態へ入る（＝ゲームループの外で1回だけ行う
    // セットアップ）。
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = &body;

        // KitStateMachine<State>::Register(状態, 毎フレーム呼ばれる関数)
        //  - mFSM.IsEnterFrame() : その状態に入った最初のフレームだけ true
        //    （アニメーションブレンドなど「入った瞬間に1回だけやりたいこと」
        //    はここに書く。毎フレーム書くと毎フレームやり直しになる）
        //  - mFSM.To(state)      : 指定した状態へ遷移する
        mFSM.Register(State::Idle,
            [this](float)
            {
                if (mFSM.IsEnterFrame())
                {
                    mBody->PlayAnimationBlend(0, kAnimBlendSec);

                    // Idle に入った瞬間、Player の方を向く（見失って落ち着く動作）。
                    // FaceDirectionXZ と組み合わせる向き（DirectionAwayFromPointXZの逆）を使う
                    // ——FaceTowardPointXZ とは符号規約が違うので、こちらは使わない。
                    if (HasTarget())
                    {
                        const Vector3 dir = toy::kit::DirectionTowardPointXZ(*mBody, mTarget->GetPosition());
                        toy::kit::FaceDirectionXZ(*mBody, dir);
                    }
                }

                // 検知: Sensor（視界）に Player が入ったかどうか
                if (HasTarget() && mBody->HasSensorHit())
                {
                    mFSM.To(State::Flee);
                }
            });

        mFSM.Register(State::Flee,
            [this](float dt)
            {
                if (mFSM.IsEnterFrame()) mBody->PlayAnimationBlend(1, kAnimBlendSec);

                if (HasTarget())
                {
                    // ToTarget（ChaseBehavior/FollowBehavior）の逆＝ターゲットの
                    // 反対方向を向いて進む
                    const Vector3 dir = toy::kit::DirectionAwayFromPointXZ(*mBody, mTarget->GetPosition());
                    toy::kit::FaceDirectionXZ(*mBody, dir);
                    toy::kit::MoveInDirectionXZ(*mBody, dir, mMoveSpeed, dt);
                }

                // 見失い: こちらは距離だけで判定（Flee中は背を向けているので
                // Sensor では判定しない。理由はクラスコメント参照）
                if (!HasTarget() || toy::kit::GetDistanceXZ(*mBody, mTarget->GetPosition()) > mLoseDistance)
                {
                    mFSM.To(State::Idle);
                }
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
    enum class State { Idle, Flee };

    bool HasTarget() const { return mTarget != nullptr; }

    static constexpr float kAnimBlendSec = 0.3f;

    toy::kit::Prefab*                mBody   = nullptr;
    toy::kit::Prefab*                mTarget = nullptr;
    toy::kit::KitStateMachine<State> mFSM;

    // Flee中にこの距離より離れたら見失ってIdleに戻る（Sensorの検知距離
    // ＝MakeNorikoDesc の sensorMaxDist より少し大きめにしてヒステリシスを持たせる）
    float mLoseDistance = 22.0f;
    float mMoveSpeed     = 4.0f;
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
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position, toy::kit::Prefab* target)
{
    auto behavior = std::make_unique<FleeBehavior>();
    behavior->SetTarget(target);

    auto noriko = std::make_unique<Noriko>(app, std::move(behavior), MakeNorikoDesc());
    noriko->GetBody().SetPosition(position);
    Quaternion rot = Quaternion(Vector3(0.0f, 1.0f, 0.0f), Math::ToRadians(180.0f));
    noriko->GetBody().SetRotation(rot);
    return noriko;
}
