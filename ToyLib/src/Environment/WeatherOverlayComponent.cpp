#include "Environment/WeatherOverlayComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/Shader.h"
#include "Asset/Geometry/VertexArray.h"
#include "Render/IRenderer.h"
#include "Physics/PhysWorld.h"
#include "Utils/MathUtil.h"

namespace toy {

WeatherOverlayComponent::WeatherOverlayComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
    auto renderer   = GetOwner()->GetApp()->GetRenderer();
    mShader         = renderer->GetShader("WeatherOverlay");
    mVertexArray    = renderer->GetFullScreenQuad();
}
void WeatherOverlayComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (!mIsVisible)
    {
        return;
    }
    if (!mShader || !mVertexArray)
    {
        return;
    }

    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    auto* phys     = app->GetPhysWorld();
    if (!renderer || !phys)
    {
        return;
    }

    // 最新のスクリーンサイズ
    const float screenW = renderer->GetScreenWidth();
    const float screenH = renderer->GetScreenHeight();

    //==========================================================
    // 1) レンズフレア可視判定（旧 Draw そのまま）
    //==========================================================
    float   flareIntensity = 0.0f;
    Vector2 sunUv(0.0f, 0.0f);

    if (mSunDir.y < 0.0f)
    {
        const Vector3 camPos      = renderer->GetCameraPosition();
        const Vector3 sunWorldPos = camPos - mSunDir * 200.0f;

        const ScreenProjectResult sc = renderer->WorldToScreen(sunWorldPos);

        if (sc.visible)
        {
            sunUv.x = sc.screen.x / screenW;
            sunUv.y = 1.0f - sc.screen.y / screenH;

            Vector3 dirCamToSun = Vector3::Normalize(sunWorldPos - camPos);

            const float startOffset = 20.0f;
            const Vector3 rayOrigin = camPos + dirCamToSun * startOffset;

            const float maxDist = (sunWorldPos - rayOrigin).Length();

            RaycastHit hit;
            const bool hitSomething = phys->Raycast(
                rayOrigin,
                dirCamToSun,
                maxDist,
                C_WALL | C_GROUND | C_CEILING | C_ENEMY_TEAM,
                hit
            );

            if (!hitSomething)
            {
                flareIntensity = 1.0f;
            }
        }
    }

    //==========================================================
    // 2) Payload（Overlay params）
    //==========================================================
    OverlayPayload op {};
    op.time = static_cast<float>(SDL_GetTicks()) / 1000.0f;

    op.rainAmount = mRainAmount;
    op.fogAmount  = mFogAmount;
    op.snowAmount = mSnowAmount;

    op.resolution = Vector2(screenW, screenH);

    op.flareIntensity = flareIntensity;
    op.sunPos         = sunUv;
    op.flareColor     = Vector3(1.0f, 0.9f, 0.7f);

    const uint32_t payloadIndex = outQueue.PushOverlayPayload(op);

    //==========================================================
    // 3) RenderItem
    //==========================================================
    RenderItem it {};
    it.pass      = RenderPass::World;                 // ※現状の描画パス設計に合わせる
    it.layer     = VisualLayer::OverlayScreen;
    it.drawOrder = GetDrawOrder();

    it.type      = RenderItemType::Overlay;
    it.dispatch  = GetDispatch(it.type);

    // state（旧 Draw 相当）
    it.depthTest  = false;
    it.depthWrite = false;

    it.blend = BlendMode::Alpha;
    if (flareIntensity > 0.0f)
    {
        it.blend = BlendMode::Additive;
    }

    it.cull      = CullMode::None;
    it.frontFace = FrontFace::CCW; // 念のため（未指定でもいい）

    // shader / geometry
    it.shader.ptr   = mShader.get();
    it.geometry.ptr = mVertexArray.get();
    it.indexCount   = mVertexArray->GetNumIndices();

    // payload
    it.payloadIndex = payloadIndex;

    outQueue.Push(it);
}
} // namespace toy
