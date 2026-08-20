#include "ShiroActor.h"

ShiroActor::ShiroActor(toy::Application* a)
    : toy::kit::KitNpcActor(a)
{
    //------------------------------------------------------------------
    // メッシュ
    //------------------------------------------------------------------
    SetupMesh("Enemy/Shiro.glb");

    auto mesh = GetApp()->GetAssetManager()->GetMesh("Enemy/Shiro.glb");
    mesh->SetCancelRootTranslation(true);
    mesh->SetCancelNodeName("Hip");
    
    mMesh->SetToonRender(true);
    mMesh->SetContourColor(Vector3(0.3f, 0.3f, 0.35f));
    mMesh->SetLocalScale(3.0f);
    mMesh->SetLocalPositon(Vector3(0.0f, 0.0f, 0.8f));

    //------------------------------------------------------------------
    // コライダー / 重力
    //------------------------------------------------------------------
    SetupCollider(mMesh,
                  toy::C_GROUND | toy::C_WALL | toy::C_FOOT |
                  toy::C_HURTBOX | toy::C_ENEMY_TEAM,
                  Vector3(0.0f, 0.0f, 0.0f),
                  Vector3(0.5f, 1.0f, 0.3f));
    SetupGravity();

    //------------------------------------------------------------------
    // ターゲット表示 / 名前ビルボード
    //------------------------------------------------------------------
    SetupTargetSprites("UI/candidate.png", "UI/lockon.png");
    SetupNameBoard("SHIRO", "Font/rounded-mplus-1c-bold.ttf", 4.0f);

    //------------------------------------------------------------------
    // Shiro 固有パラメータ（Wolf より広く、やや遅め）
    //------------------------------------------------------------------
    mDetectRange = 40.0f;
    mMoveSpeed   = 6.0f;

    //------------------------------------------------------------------
    // ステートマシン
    //  Idle : 索敵して Chase へ
    //  Chase: 追跡。見失ったら Idle へ（ヒステリシス x1.5）
    //------------------------------------------------------------------
    mFSM.Register(ShiroState::Idle,
        /* onUpdate */ [&](float) {
            if (HasTarget() && GetDistanceToTarget() < mDetectRange)
                mFSM.To(ShiroState::Chase);
        },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_IDLE); }
    );
    mFSM.Register(ShiroState::Chase,
        /* onUpdate */ [&](float dt) {
            MoveTowardTarget(mMoveSpeed, dt);
            LookAtTarget();
            if (!HasTarget() || GetDistanceToTarget() > mDetectRange * 1.5f)
                mFSM.To(ShiroState::Idle);
        },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_RUN); }
    );
    mFSM.Start(ShiroState::Idle);
}

void ShiroActor::UpdateCharacter(float deltaTime)
{
    mFSM.Update(deltaTime);
}
