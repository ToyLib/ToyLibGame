#include "KitPrefab/Prefab.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"
#include "Physics/SensorComponent.h"
#include "Graphics/Sprite/GroundConformSpriteComponent.h"
#include "Graphics/Billboard/TextBillboardComponent.h"
#include "Audio/SoundComponent.h"

namespace toy::kit {

namespace detail {

//=============================================================================
// PrefabActorAdapter
//  Prefab を toy::Actor の更新ループに接続するための内部実装専用アダプタ。
//  ゲーム側・Prefab派生クラスの外からは見えない（Prefab.cpp 内に閉じる）。
//=============================================================================
class PrefabActorAdapter : public toy::Actor
{
public:
    PrefabActorAdapter(toy::Application* app, Prefab* owner)
        : toy::Actor(app)
        , mOwner(owner)
    {}

    void UpdateActor(float deltaTime) override
    {
        if (mOwner) mOwner->TickFromActor(deltaTime);
    }

    // Prefab が破棄されるときに呼ぶ。
    // Actor 自体は Dead マーク後も同フレーム中に Update() され得るため、
    // 参照先が既に破棄されたあとの UpdateActor() 呼び出しに備えて
    // 逆参照を確実に断つ。
    void ClearOwner() { mOwner = nullptr; }

private:
    Prefab* mOwner = nullptr;
};

} // namespace detail

Prefab::Prefab(toy::Application* app)
    : mApp(app)
{
    mActor = mApp->CreateActor<detail::PrefabActorAdapter>(this);
}

Prefab::~Prefab()
{
    if (mNameActor)
    {
        mNameActor->SetState(toy::Actor::State::Dead);
    }

    if (mActor)
    {
        // Actor 側からの逆参照を先に断ってから Dead マークする
        static_cast<detail::PrefabActorAdapter*>(mActor)->ClearOwner();
        mActor->DestroyActor();
    }
}

void Prefab::SetPosition(const Vector3& pos)
{
    mActor->SetPosition(pos);
}

const Vector3& Prefab::GetPosition() const
{
    return mActor->GetPosition();
}

void Prefab::SetRotation(const Quaternion& rot)
{
    mActor->SetRotation(rot);
}

const Quaternion& Prefab::GetRotation() const
{
    return mActor->GetRotation();
}

const Matrix4& Prefab::GetWorldTransform() const
{
    return mActor->GetWorldTransform();
}

Vector3 Prefab::GetForward() const
{
    return mActor->GetForward();
}

void Prefab::SetScale(float scale)
{
    mActor->SetScale(scale);
}

float Prefab::GetScale() const
{
    return mActor->GetScale();
}

void Prefab::TickFromActor(float deltaTime)
{
    DetectCollisionEvents();
    DetectGroundedEvent();
    UpdateNameBoard();
    UpdateTargetSprites();
    OnUpdate(deltaTime);
}

void Prefab::DetectCollisionEvents()
{
    if (!mCollider) return;

    for (auto* other : mCollider->GetTargetColliders())
    {
        mOnCollision.Emit(CollisionEvent{ other });
    }
}

void Prefab::DetectGroundedEvent()
{
    if (!mGravity) return;

    const bool grounded = mGravity->IsGrounded();
    if (grounded != mWasGrounded)
    {
        mWasGrounded = grounded;
        mOnGrounded.Emit(GroundedEvent{ grounded });
    }
}

//=============================================================================
// 名前ビルボード
//=============================================================================
void Prefab::SetupNameBoard(const std::string& name, const std::string& fontPath,
                            float yOffset, const Vector3& color)
{
    mNameYOffset = yOffset;

    // ビルボードを本体の Actor に持たせるとスケールの影響を受けるため、
    // 独立した Actor に持たせる
    mNameActor = mApp->CreateActor<toy::Actor>();

    auto* board = mNameActor->CreateComponent<toy::TextBillboardComponent>(101);
    auto  font  = mApp->GetAssetManager()->GetFont(fontPath, 40);
    board->SetFont(font);
    board->SetFormat(name);
    board->SetScale(0.01f);
    board->SetColor(color);
}

void Prefab::UpdateNameBoard()
{
    if (!mNameActor) return;

    const Vector3& pos = GetPosition();
    mNameActor->SetPosition(Vector3(pos.x, pos.y + mNameYOffset, pos.z));
}

//=============================================================================
// ターゲット表示スプライト
//=============================================================================
void Prefab::SetupTargetSprites(const std::string& candidateTex, const std::string& lockedTex)
{
    if (!candidateTex.empty())
    {
        mCandidateSigne = CreateTargetSprite(candidateTex);
    }
    if (!lockedTex.empty())
    {
        mLockedSigne = CreateTargetSprite(lockedTex);
    }
}

toy::GroundConformSpriteComponent* Prefab::CreateTargetSprite(const std::string& texPath)
{
    auto* sprite = mActor->CreateComponent<toy::GroundConformSpriteComponent>();
    sprite->SetTexture(mApp->GetAssetManager()->GetTexture(texPath));
    sprite->SetSize(5, 5);
    sprite->SetBlendAdd(false);
    sprite->SetAlpha(1.0f);
    sprite->SetGroundLift(0.2f);
    sprite->SetGridDiv(4);
    sprite->SetMaxDeltaFromCenter(0.6f);
    sprite->SetVisible(false);
    return sprite;
}

void Prefab::UpdateTargetSprites()
{
    if (!mCollider) return;

    const auto state = mCollider->GetTargetState();

    if (mCandidateSigne)
    {
        mCandidateSigne->SetVisible(state == toy::TargetState::Candidate);
    }
    if (mLockedSigne)
    {
        mLockedSigne->SetVisible(state == toy::TargetState::Locked);
    }
}

//=============================================================================
// アンビエントサウンド / 常時表示テキスト
//=============================================================================
void Prefab::SetupAmbientSound(const std::string& soundPath, float volume, bool loop)
{
    if (soundPath.empty()) return;

    auto* sound = mActor->CreateComponent<toy::SoundComponent>();
    sound->SetSound(soundPath);
    sound->SetVolume(volume);
    sound->SetLoop(loop);
    sound->Enable3DSound(true);
    sound->Play();
}

void Prefab::SetupSpeechText(const std::string& text, const std::string& fontPath, const Vector3& color)
{
    if (text.empty()) return;

    auto* board = mActor->CreateComponent<toy::TextBillboardComponent>(500);
    board->SetFont(mApp->GetAssetManager()->GetFont(fontPath, 50));
    board->SetColor(color);
    board->SetText(text);
    board->SetScale(0.01f);
}

//=============================================================================
// 視界センサー
//=============================================================================
void Prefab::SetupSensor(float fovDeg, float maxDist, uint32_t targetMask, bool requireLOS)
{
    toy::SensorComponent::Desc desc;
    desc.fovRad     = Math::ToRadians(fovDeg);
    desc.maxDist    = maxDist;
    desc.targetMask = targetMask;
    desc.requireLOS = requireLOS;

    mSensor = mActor->CreateComponent<toy::SensorComponent>(desc);
}

bool Prefab::HasSensorHit() const
{
    return mSensor && !mSensor->GetHits().empty();
}

} // namespace toy::kit
