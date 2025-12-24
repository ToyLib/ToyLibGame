#include "HeroActor.h"
#include "MagicActor.h"
#include "HealMagicActor.h"
#include "ToyLib.h"
#include <iostream>


HeroActor::HeroActor(toy::Application* a)
: Actor(a)
, mAnimID(H_Stand)
, mMovable(true)
{
    // --- JSON読み込み ---
    std::ifstream file("ToyGame/Settings/HeroActor.json");
    nlohmann::json json;
    file >> json;

    // --- スケルタルメッシュ ---
    mMeshComp = CreateComponent<toy::SkeletalMeshComponent>();
    std::string meshPath;
    if (json.contains("mesh") && json["mesh"].contains("file"))
    {
        JsonHelper::GetString(json["mesh"], "file", meshPath);
    }
    mMeshComp->SetMesh(a->GetAssetManager()->GetMesh(meshPath));
    //mMeshComp->SetAnimID(mAnimID, PLAY_CYCLIC);

    bool useToon = false;
    float contour = 1.00f;
    JsonHelper::GetBool(json["mesh"], "toon_render", useToon);
    JsonHelper::GetFloat(json["mesh"], "contour_factor", contour);
    mMeshComp->SetToonRender(useToon);
    mMeshComp->SetContourFactor(contour);

    // --- Transform設定 ---
    Vector3 pos;
    JsonHelper::GetVector3(json, "position", pos);
    SetPosition(pos);
    Quaternion q;
    JsonHelper::GetQuaternionFromEuler(json, "rotation_deg", q);
    SetRotation(q);
    float scale = 1.0f;
    JsonHelper::GetFloat(json, "scale", scale);
    SetScale(scale);

    // --- コライダー ---
    mCollComp = CreateComponent<toy::ColliderComponent>();
    mCollComp->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh(meshPath)->GetVertexArray());
    Vector3 vOffset;
    JsonHelper::GetVector3(json["collider"], "bounding_box_offset", vOffset);
    Vector3 vScale;
    JsonHelper::GetVector3(json["collider"], "bounding_box_scale", vScale);
    mCollComp->GetBoundingVolume()->AdjustBoundingBox(vOffset, vScale);
    mCollComp->SetFlags(toy::C_FOOT | toy::C_PLAYER_TEAM);
    mCollComp->SetEnabled(true);

    
    

    // --- 移動コンポーネント ---
    //mMoveComp = CreateComponent<toy::FPSMoveComponent>();
    mMoveComp = CreateComponent<toy::DirMoveComponent>();
    
    
    // --- カメラコンポーネント ---
    //mCameraComp = CreateComponent<toy::FollowCameraComponent>();
    mCameraComp = CreateComponent<toy::OrbitCameraComponent>();
    
    // --- 重力コンポーネント ---
    mGravComp = CreateComponent<toy::GravityComponent>();
    mGravComp->SetEnableGroundPose(false);
    
    // --- センサーコンポーネント ---
    mSensor= CreateComponent<toy::SensorComponent>();
    
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);
    
    
    mMagic = GetApp()->CreateActor<MagicActor>();
    mHeal = GetApp()->CreateActor<HealMagicActor>();
    
    // ターゲットスプライト
    mTargetActor = GetApp()-> CreateActor<toy::Actor>();
    mTarget = mTargetActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::UI);
    mTarget->SetTexture(GetApp()->GetAssetManager()->GetTexture("target_scope.png"));
    mTarget->SetBlendAdd(false);
    mTarget->SetIsTopLeft(true);
    
    
    
}

HeroActor::~HeroActor()
{

}

void HeroActor::UpdateActor(float deltaTime)
{
    mTarget->SetVisible(false);

    auto hits = mSensor->GetHits();
    if (hits.size() > 0)
    {
        auto bb = hits[0].collider->GetBoundingVolume()->GetWorldAABB();
        auto v = (bb.max + bb.min) * 0.5f;

        auto scInfo = GetApp()->GetRenderer()->WorldToScreen(v);
        if (scInfo.visible)
        {
            mTargetActor->SetPosition(Vector3(scInfo.screen.x, scInfo.screen.y, 0));
            mTarget->SetVisible(true);
			std::cout << "Target Screen Pos: " << scInfo.screen.x << ", " << scInfo.screen.y << std::endl;
        }
    }
    
    
}

void HeroActor::ActorInput(const toy::InputState& state)
{
    bool inputAttack = false;

    auto animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    // --- 移動可能状態 ---
    if (mMovable)
    {
        // 攻撃入力（入力優先度付きで判定）
        if (state.IsButtonPressed(toy::GameButton::B))
        {
            animPlayer->PlayOnce(H_Slash, H_Stand);
            inputAttack = true;
        }
        else if (state.IsButtonPressed(toy::GameButton::X))
        {
            animPlayer->PlayOnce(H_Spin, H_Stand);
            inputAttack = true;
            mHeal->Spawn(GetPosition());
        }
        else if (state.IsButtonPressed(toy::GameButton::Y))
        {
            animPlayer->PlayOnce(H_Stab, H_Stand);
            inputAttack = true;
            mMagic->Spawn(GetPosition(), GetForward());
        }

        if (inputAttack)
        {
            mMovable = false; // 攻撃中はロック
        }
        else
        {
            // ジャンプ（移動ロックしない）
            if (state.IsButtonPressed(toy::GameButton::A))
            {
                mGravComp->Jump();
                animPlayer->PlayOnce(H_Jump, H_Stand);
            }

            // --- 状態に応じた通常アニメ切り替え ---
            if (mGravComp->GetVelocityY() != 0.0f)
            {
                animPlayer->Play(H_Jump); // ジャンプ中も移動OK
            }
            else if (mMoveComp->GetForwardSpeed() == 0.0f &&
                     mMoveComp->GetAngularSpeed() == 0.0f &&
                     mMoveComp->GetRightSpeed() == 0.0f)
            {
                animPlayer->Play(H_Stand);
                mSound->Stop();

            }
            else
            {
                animPlayer->Play(H_Run);
                if (!mSound->IsPlaying())
                {
                    mSound->Play();
                }
            }
        }
        if (mGravComp->GetVelocityY() != 0.0f)
        {
            mSound->Stop();
        }
    }
    else
    {
        // 攻撃終了したら解除（ループアニメ中も解除）
        if (animPlayer->IsLooping() || animPlayer->IsFinished())
        {
            mMovable = true;
        }
    }

    // 最後にMoveComponentへ反映
    mMoveComp->SetIsMovable(mMovable);
}
