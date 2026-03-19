#include "Environment/WeatherOverlayComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/Geometry/VertexArray.h"
#include "Render/IRenderer.h"
#include "Physics/PhysWorld.h"
#include "Utils/MathUtil.h"

#include <SDL3/SDL.h>

namespace toy {

WeatherOverlayComponent::WeatherOverlayComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    mPipelineName = "WeatherOverlay";
    mVertexArray  = renderer->GetFullScreenQuad();
}

void WeatherOverlayComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (!mIsVisible)
    {
        return;
    }
    if (!mVertexArray)
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

    const float screenW = renderer->GetScreenWidth();
    const float screenH = renderer->GetScreenHeight();
    if (screenW <= 0.0f || screenH <= 0.0f)
    {
        return;
    }

    //==========================================================
    // 1) レンズフレア可視判定
    //    - mFlareIntensity > 0 のときだけ判定
    //    - Overlay は「フレア許可量」しか知らない
    //==========================================================
    float   flareIntensity = 0.0f;
    Vector2 sunUv(0.0f, 0.0f);

    if (mFlareIntensity > 0.001f)
    {
        // 既存ロジックを維持
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
                    flareIntensity = mFlareIntensity;
                }
            }
        }
    }

    //==========================================================
    // 2) Payload
    //==========================================================
    OverlayPayload op{};
    op.time = static_cast<float>(SDL_GetTicks()) / 1000.0f;

    op.rainAmount = mRainAmount;
    op.fogAmount  = mFogAmount;
    op.snowAmount = mSnowAmount;

    op.resolution = Vector2(screenW, screenH);

    op.flareIntensity = flareIntensity;
    op.sunPos         = sunUv;
    op.flareColor     = mFlareColor;

    const uint32_t payloadIndex = outQueue.PushOverlayPayload(op);

    //==========================================================
    // 3) RenderItem
    //==========================================================
    RenderItem it{};
    it.pass      = RenderPass::World;
    it.layer     = VisualLayer::OverlayScreen;
    it.drawOrder = GetDrawOrder();

    it.type     = RenderItemType::Overlay;
    it.dispatch = GetDispatch(it.type);

    it.depthTest  = false;
    it.depthWrite = false;
    it.blend      = BlendMode::Alpha;

    it.cull      = CullMode::None;
    it.frontFace = FrontFace::CCW;

    it.pipeline     = renderer->GetPipelineHandle(mPipelineName);
    it.geometry.ptr = mVertexArray.get();
    it.indexCount   = mVertexArray->GetNumIndices();

    it.payloadIndex = payloadIndex;

    it.blend = BlendMode::Alpha;
    if (flareIntensity > 0.0f)
    {
        it.blend = BlendMode::Additive;
    }
    
    outQueue.Push(it);

}

} // namespace toy
