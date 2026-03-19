#include "Environment/SnowFieldComponent.h"

#include "Asset/Material/Texture.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"
#include "Render/RenderItemPayloads.h"

#include <cstdlib>
#include <cmath>

namespace toy {

namespace {

float Rand01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float RandRange(float a, float b)
{
    return a + (b - a) * Rand01();
}

} // namespace

SnowFieldComponent::SnowFieldComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder, layer)
{
    mPipelineName = "UnlitQuad";
    InitializeParticles();
}

void SnowFieldComponent::InitializeParticles()
{
    mParticles.resize(mMaxParticles);

    const Vector3 center = GetSpawnCenter();
    for (auto& p : mParticles)
    {
        RespawnParticle(p, center, true);
    }
}

Vector3 SnowFieldComponent::GetSpawnCenter() const
{
    auto* app = GetOwner()->GetApp();
    if (!app || !app->GetRenderer())
    {
        return GetOwner()->GetWorldTransform().GetTranslation();
    }

    return app->GetRenderer()->GetInvViewMatrix().GetTranslation();
}

void SnowFieldComponent::RespawnParticle(SnowParticle& p, const Vector3& center, bool randomY)
{
    const float halfX = mAreaSize.x * 0.5f;
    const float halfY = mAreaSize.y * 0.5f;
    const float halfZ = mAreaSize.z * 0.5f;

    p.position.x = center.x + RandRange(-halfX, halfX);
    p.position.z = center.z + RandRange(-halfZ, halfZ);

    if (randomY)
    {
        p.position.y = center.y + RandRange(-halfY, halfY);
    }
    else
    {
        p.position.y = center.y + halfY;
    }

    const float fall = RandRange(0.8f, 1.2f) * mFallSpeed;

    p.velocity = Vector3(
        mWind.x + RandRange(-0.15f, 0.15f),
        -fall,
        mWind.z + RandRange(-0.15f, 0.15f)
    );

    p.scale = RandRange(0.7f, 1.3f);
    p.alpha = RandRange(0.6f, 1.0f);
}

void SnowFieldComponent::Update(float deltaTime)
{
    if (!mEnabled)
    {
        return;
    }

    const Vector3 center = GetSpawnCenter();
    const float halfY = mAreaSize.y * 0.5f;

    const int activeCount =
        static_cast<int>(static_cast<float>(mParticles.size()) * mIntensity);

    for (int i = 0; i < activeCount; ++i)
    {
        SnowParticle& p = mParticles[i];

        p.position += p.velocity * deltaTime;

        if (std::fabs(p.position.x - center.x) > mAreaSize.x * 0.6f ||
            std::fabs(p.position.z - center.z) > mAreaSize.z * 0.6f ||
            p.position.y < center.y - halfY - mRespawnBottomMargin)
        {
            RespawnParticle(p, center, false);
        }
    }
}

void SnowFieldComponent::GatherRenderItems(RenderQueue& out)
{
    if (!mIsVisible || !mEnabled || !mTexture)
    {
        return;
    }

    auto* owner    = GetOwner();
    auto* renderer = owner ? owner->GetApp()->GetRenderer() : nullptr;
    if (!renderer)
    {
        return;
    }

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();

    const Vector3 cameraPos = renderer->GetInvViewMatrix().GetTranslation();
    const float ownerScale  = owner->GetScale();

    const int activeCount =
        static_cast<int>(static_cast<float>(mParticles.size()) * mIntensity);

    for (int i = 0; i < activeCount; ++i)
    {
        const SnowParticle& p = mParticles[i];

        //------------------------------------------------------
        // Billboard rotation
        //------------------------------------------------------
        Matrix4 rot = Matrix4::Identity;

        if (mYawOnly)
        {
            Vector3 toCamera = p.position - cameraPos;
            toCamera.y = 0.0f;

            if (toCamera.LengthSq() < 1.0e-6f)
            {
                toCamera = Vector3::UnitZ;
            }
            else
            {
                toCamera.Normalize();
            }

            const float angle = std::atan2(toCamera.x, toCamera.z);
            rot = Matrix4::CreateRotationY(angle);
        }
        else
        {
            // 必要になったら full billboard を追加
            Vector3 toCamera = p.position - cameraPos;
            toCamera.Normalize();

            const float angle = std::atan2(toCamera.x, toCamera.z);
            rot = Matrix4::CreateRotationY(angle);
        }

        //------------------------------------------------------
        // Scale
        // BillboardComponent と同じ考え方:
        // texture size * scale * ownerScale
        //------------------------------------------------------
        const float finalScale = mBaseScale * p.scale * ownerScale;

        const Matrix4 scaleMat = Matrix4::CreateScale(
            mTexture->GetWidth()  * finalScale,
            mTexture->GetHeight() * finalScale,
            1.0f);

        const Matrix4 translate = Matrix4::CreateTranslation(p.position);
        const Matrix4 world     = scaleMat * rot * translate;

        //------------------------------------------------------
        // Payload
        //------------------------------------------------------
        UnlitQuadPayload up{};
        up.useTint = true;
        up.tint    = Vector3(1.0f, 1.0f, 1.0f);
        up.alpha   = p.alpha;

        const uint32_t payloadIndex = out.PushUnlitQuad(up);

        //------------------------------------------------------
        // RenderItem
        //------------------------------------------------------
        RenderItem it{};
        it.type      = RenderItemType::UnlitQuad;
        it.pass      = RenderPass::World;
        it.dispatch  = GetDispatch(it.type);
        it.layer     = mLayer;
        it.drawOrder = mDrawOrder;

        it.pipeline = renderer->GetPipelineHandle(mPipelineName);
        it.viewProj = view * proj;
        it.world    = world;

        it.geometry   = renderer->GetSurfaceQuadHandle();
        it.indexCount = 6;

        it.texture     = renderer->ToHandle(mTexture);
        it.textureUnit = 0;

        it.payloadIndex = payloadIndex;

        it.depthTest  = true;
        it.depthWrite = false;
        it.blend      = BlendMode::Alpha;

        it.cull      = CullMode::Back;
        it.frontFace = FrontFace::CCW;

        out.Push(it);
    }
}

} // namespace toy
