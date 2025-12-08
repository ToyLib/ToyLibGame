#include "HeroActor.h"
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
    float outline = 1.00f;
    JsonHelper::GetBool(json["mesh"], "toon_render", useToon);
    JsonHelper::GetFloat(json["mesh"], "toon_outline", outline);
    mMeshComp->SetToonRender(useToon, outline);

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
    //mCollComp->GetBoundingVolume()->SetVisible(true);
    mCollComp->SetFlags(toy::C_FOOT | toy::C_PLAYER);
    mCollComp->SetDisp(true);

    // --- 移動コンポーネント ---
    //mMoveComp = CreateComponent<toy::FPSMoveComponent>();
    mMoveComp = CreateComponent<toy::DirMoveComponent>();
    
    
    // --- カメラコンポーネント ---
    mCameraComp = CreateComponent<toy::OrbitCameraComponent>();
    //mCameraComp = CreateComponent<toy::FollowCameraComponent>();
    

    mGravComp = CreateComponent<toy::GravityComponent>();
    //SetPosition(Vector3(0,100,0));
    
    
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->SetUseDistanceAttenuation(true);
    
    
}

HeroActor::~HeroActor()
{

}

void HeroActor::UpdateActor(float deltaTime)
{
}

void HeroActor::ActorInput(const toy::InputState& state)
{
    bool inputAttack = false;

    auto animPlayer = mMeshComp->GetAnimPlayer();

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
        }
        else if (state.IsButtonPressed(toy::GameButton::Y))
        {
            animPlayer->PlayOnce(H_Stab, H_Stand);
            inputAttack = true;
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
