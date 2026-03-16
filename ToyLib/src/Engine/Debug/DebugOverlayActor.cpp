#include "Engine/Debug/DebugOverlayActor.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Debug/DebugStats.h"
#include "Asset/Font/TextFont.h"
#include "Asset/AssetManager.h"
#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Graphics/Sprite/SpriteComponent.h"
#include "Asset/Material/Texture.h"
#include "Render/RenderBackendState.h"
#include "Render/IRenderer.h"
#include "Utils/StringUtil.h"

namespace toy {

DebugOverlayActor::DebugOverlayActor(Application* app)
    : Actor(app)
{
    SetActorID("DebugOverlay");

    SetPosition(Vector3::Zero);

    mTextComp = CreateComponent<TextSpriteComponent>(1000, VisualLayer::UI);

    auto debugFont = app->mSystemAssetManager->GetFont("Hermit-Bold.otf", 20);
    mTextComp->SetFont(debugFont);
    mTextComp->SetColor(mTextColor);

    auto white = app->mSystemAssetManager->GetWhite1x1Texture();
    mBgSprite = CreateComponent<SpriteComponent>(999, VisualLayer::UI);
    mBgSprite->SetTexture(white);
    mBgSprite->SetColor(Vector3(0.2f, 0.2f, 0.2f));
    mBgSprite->SetAlpha(0.6f);

    if (RenderBackendState::Get().IsVK())
    {
        mBackendName = "Vulkan";
    }
    else
    {
        mBackendName = "OpenGL";
    }

    SetEnabled(false);
}

const std::string& DebugOverlayActor::GetBackendName() const
{
    return mBackendName;
}

void DebugOverlayActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;

    if (mTextComp)
    {
        mTextComp->SetVisible(enabled);
    }
    if (mBgSprite)
    {
        mBgSprite->SetVisible(enabled);
    }

    if (enabled)
    {
        mRefreshAccum = mRefreshInterval;
        RefreshOverlayText();
    }
}

void DebugOverlayActor::SetWireVisible(bool visible)
{
    mWireVisible = visible;
    GetApp()->SetVisibleDebuWire(mWireVisible);
}

void DebugOverlayActor::RefreshOverlayText()
{
    auto* app   = GetApp();
    auto& stats = app->GetDebugStats();

    if (!mEnabled || !mTextComp || !mBgSprite)
    {
        return;
    }

    std::string text;
    text.clear();
    text.reserve(512);
    
    std::string deviceName = GetApp()->GetRenderer()->GetDeviceName();

    text += "=== Debug ===\n";
    text += StringUtil::Format("<<\n",                    deviceName);
    text += StringUtil::Format("<<\n",                    GetBackendName());
    text += "-------------\n";
    text += StringUtil::Format("FPS          : <<\n",     mSmoothedFPS);
    text += StringUtil::Format("DeltaTime    : << ms\n",  stats.DeltaTimeMs);
    text += StringUtil::Format("Actors       : <<\n",     stats.ActorCount);
    text += StringUtil::Format("Colliders    : <<\n",     stats.ColliderCount);
    text += StringUtil::Format("DrawCalls    : <<\n",     stats.DrawCallCount);
    text += StringUtil::Format("RTTCalls     : <<\n",     stats.OffDrawCallCount);
    text += StringUtil::Format("RenderTime   : << ms\n",  stats.RenderTimeMs);
    text += StringUtil::Format("UpdateTTL    : << ms\n",  stats.UpdateTotalTimeMs);
    text += "-------------\n";
    text += StringUtil::Format("UpdateGame   : << ms\n",  stats.UpdateGameTimeMs);
    text += StringUtil::Format("ActorUpdate  : << ms\n",  stats.ActorUpdateTimeMs);
    text += StringUtil::Format("ActorFinalize: << ms\n",  stats.ActorFinalizeTimeMs);
    text += StringUtil::Format("SoundTime    : << ms\n",  stats.SoundTimeMs);
    text += StringUtil::Format("TimeOfDay    : << ms\n",  stats.TimeOfDayTimeMs);
    text += StringUtil::Format("DebugStats   : << ms\n",  stats.DebugStatsTimeMs);
    text += StringUtil::Format("PhysTime     : << ms\n",  stats.PhysicsTimeMs);
    text += "-------------\n";
    text += StringUtil::Format("Resolution   : << x <<",  stats.ScreenW, stats.ScreenH);

    mTextComp->SetText(text);

    auto tex = mTextComp->GetTexture();
    if (!tex)
    {
        return;
    }

    const float pad = 6.0f;
    const float w = static_cast<float>(tex->GetWidth());
    const float h = static_cast<float>(tex->GetHeight());

    mBgSprite->SetScale(w + pad * 2.0f, h + pad * 2.0f);
}

void DebugOverlayActor::UpdateActor(float deltaTime)
{
    if (!mEnabled || !mTextComp || !mBgSprite)
    {
        return;
    }

    auto& stats = GetApp()->GetDebugStats();

    if (stats.FPS > 0.0f)
    {
        mSmoothedFPS = (mSmoothedFPS == 0.0f)
                     ? stats.FPS
                     : (mSmoothedFPS * 0.8f + stats.FPS * 0.2f);
    }

    mRefreshAccum += deltaTime;
    if (mRefreshAccum >= mRefreshInterval)
    {
        mRefreshAccum = 0.0f;
        RefreshOverlayText();
    }
}

void DebugOverlayActor::ActorInput(const InputState& state)
{
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_F3) == EPressed)
    {
        SetEnabled(!mEnabled);
    }

    if (state.Keyboard.GetKeyState(SDL_SCANCODE_F4) == EPressed)
    {
        SetWireVisible(!mWireVisible);
    }
}

} // namespace toy
