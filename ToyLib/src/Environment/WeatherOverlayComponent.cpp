#include "Environment/WeatherOverlayComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Shader.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Render/Renderer.h"
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
    if (!mIsVisible) return;
    if (!mShader || !mVertexArray) return;

    auto* app = GetOwner()->GetApp();
    if (!app) return;

    auto* renderer = app->GetRenderer();
    auto* phys     = app->GetPhysWorld();
    if (!renderer || !phys) return;

    // 最新のスクリーンサイズ
    const float screenW = renderer->GetScreenWidth();
    const float screenH = renderer->GetScreenHeight();

    //==========================
    // 1) レンズフレア可視判定（旧 Draw そのまま）
    //==========================
    float  flareIntensity = 0.0f;
    Vector2 sunUv(0.0f, 0.0f);

    if (mSunDir.y < 0.0f)
    {
        Vector3 camPos      = renderer->GetCameraPosition();
        Vector3 sunWorldPos = camPos - mSunDir * 200.0f;

        ScreenProjectResult sc = renderer->WorldToScreen(sunWorldPos);

        if (sc.visible)
        {
            sunUv.x = sc.screen.x / screenW;
            sunUv.y = 1.0f - sc.screen.y / screenH;

            Vector3 dirCamToSun = Vector3::Normalize(sunWorldPos - camPos);

            const float startOffset = 20.0f;
            Vector3 rayOrigin = camPos + dirCamToSun * startOffset;

            float maxDist = (sunWorldPos - rayOrigin).Length();

            RaycastHit hit;
            bool hitSomething = phys->Raycast(
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

    //==========================
    // 2) RenderItem 化
    //==========================
    RenderItem it;
    it.pass      = RenderPass::World;                 // ★Overlay は UI/Overlay パスへ
    it.layer     = VisualLayer::OverlayScreen;
    it.drawOrder = GetDrawOrder();
    it.type      = RenderItemType::Overlay;
    it.dispatch  = GetDispatch(it.type);

    // 旧 Draw の GL state を RenderItem state に変換（ロジック変更なし）
    it.depthTest  = false;
    it.depthWrite = false;
    it.blend      = BlendMode::Alpha;              // 通常はアルファ

    // フレアがある時だけ加算
    if (flareIntensity > 0.0f)
    {
        it.blend = BlendMode::Additive;
    }

    it.cull      = CullMode::None;

    it.shader.ptr   = mShader.get();
    it.geometry.ptr = mVertexArray.get();
    it.indexCount   = mVertexArray->GetNumIndices();

    // uniforms（旧 Draw のまま）
    it.overlayTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;

    it.overlayRainAmount = mRainAmount;
    it.overlayFogAmount  = mFogAmount;
    it.overlaySnowAmount = mSnowAmount;

    it.overlayResolution = Vector2(screenW, screenH);

    it.overlayFlareIntensity = flareIntensity;
    it.overlaySunPos         = sunUv;
    it.overlayFlareColor     = Vector3(1.0f, 0.9f, 0.7f);

    outQueue.Push(it);
}

/*
void WeatherOverlayComponent::Draw()
{
    if (!mShader || !mVertexArray)
    {
        return;
    }
    
    Renderer* renderer = GetOwner()->GetApp()->GetRenderer();
    PhysWorld* phys    = GetOwner()->GetApp()->GetPhysWorld();
    
    // 最新のスクリーンサイズを取得
    float screenW = renderer->GetScreenWidth();
    float screenH = renderer->GetScreenHeight();

    //==========================
    // 1. レンズフレア可視判定
    //==========================
    float  flareIntensity = 0.0f;
    Vector2 sunUv(0.0f, 0.0f);
    
    if (mSunDir.y < 0.0f)
    {
        Vector3 camPos      = renderer->GetCameraPosition();
        Vector3 sunWorldPos = camPos - mSunDir * 200.0f;
        
        ScreenProjectResult sc = renderer->WorldToScreen(sunWorldPos);
        
        if (sc.visible)
        {
            sunUv.x = sc.screen.x / screenW;
            sunUv.y = 1.0f - sc.screen.y / screenH;
            
            // カメラ → 太陽 方向
            Vector3 dirCamToSun = Vector3::Normalize(sunWorldPos - camPos);
                
            // カメラの少し前から撃つ（めり込み回避用オフセット）
            const float startOffset = 20.0f; // 調整用（5〜50くらいで好みを探る）
            Vector3 rayOrigin = camPos + dirCamToSun * startOffset;
                
            // 太陽までの距離分だけ飛ばせば十分
            float maxDist = (sunWorldPos - rayOrigin).Length();
            
            RaycastHit hit;
            bool hitSomething = phys->Raycast(
                                            rayOrigin,
                                            dirCamToSun,
                                            maxDist,
                                            C_WALL | C_GROUND | C_CEILING|  C_ENEMY_TEAM,
                                            hit
                                            );
                
            if (!hitSomething)
            {
                flareIntensity = 1.0f;
            }
        }
    }

    //==========================
    // 2. フルスクリーン描画設定
    //==========================
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    // ★ フレアがあるときだけ加算合成、それ以外は通常のアルファブレンド
    if (flareIntensity > 0.0f)
    {
        glBlendFunc(GL_ONE, GL_ONE);  // 加算合成
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 通常
    }

    //==========================
    // 3. シェーダ設定
    //==========================
    mShader->SetActive();

    mShader->SetFloatUniform("uTime", SDL_GetTicks() / 1000.0f);
    mShader->SetFloatUniform("uRainAmount", mRainAmount);
    mShader->SetFloatUniform("uFogAmount",  mFogAmount);
    mShader->SetFloatUniform("uSnowAmount", mSnowAmount);

    mShader->SetVector2Uniform("uResolution",
                               Vector2(screenW, screenH));

    // フレア関連
    mShader->SetFloatUniform("uFlareIntensity", flareIntensity);
    mShader->SetVector2Uniform("uSunPos", sunUv);
    mShader->SetVectorUniform("uFlareColor",
                               Vector3(1.0f, 0.9f, 0.7f));

    //==========================
    // 4. フルスクリーン Quad 描画
    //==========================
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES,
                   mVertexArray->GetNumIndices(),
                   GL_UNSIGNED_INT,
                   nullptr);
    renderer->AddDrawCall();

    //==========================
    // 5. 後処理
    //==========================
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
*/
} // namespace toy
