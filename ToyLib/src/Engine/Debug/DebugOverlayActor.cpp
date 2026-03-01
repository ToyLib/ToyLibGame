// Engine/Debug/DebugOverlayActor.cpp
#include "Engine/Debug/DebugOverlayActor.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Debug/DebugStats.h"
#include "Asset/Font/TextFont.h"
#include "Asset/AssetManager.h"
#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Asset/Material/Texture.h"
#include "Render/RenderBackendState.h"
#include <iostream>

#include <SDL3/SDL_scancode.h> // SDL2 なら <SDL2/SDL_scancode.h>

namespace toy {

DebugOverlayActor::DebugOverlayActor(Application* app)
    : Actor(app)
{
    SetActorID("DebugOverlay");

    // 左上に固定（SpriteComponent 側で原点が左上になるようにしている前提）
    SetPosition(Vector3::Zero);

    // TextSpriteComponent を作成（UI レイヤー）
    mTextComp = CreateComponent<TextSpriteComponent>(1000, VisualLayer::UI);

    // デバッグ用フォントを取得
    auto debugFont = app->mSystemAssetManager->GetFont("Hermit-Bold.otf", 20);
    mTextComp->SetFont(debugFont);
    // 色
    mTextComp->SetColor(mTextColor);
    
    // 背景用スプライト
    auto white = app->mSystemAssetManager->GetWhite1x1Texture();
    mBgSprite = CreateComponent<SpriteComponent>(999, VisualLayer::UI);
    mBgSprite->SetTexture(white);
    mBgSprite->SetColor(Vector3(0.2f, 0.2f, 0.2f)); // グレー
    mBgSprite->SetAlpha(0.6f);                      // 半透明

}

void DebugOverlayActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;
    if (mTextComp)
    {
        mTextComp->SetVisible(enabled);
        mBgSprite->SetVisible(enabled);
    }
}
void DebugOverlayActor::SetWireVisible(bool visible)
{
    mWireVisible = visible;
    GetApp()->SetVisibleDebuWire(mWireVisible);
}


void DebugOverlayActor::UpdateActor(float deltaTime)
{
    //Actor::UpdateActor(deltaTime);

    auto* app   = GetApp();
    auto& stats = app->GetDebugStats();

    if (!mEnabled || !mTextComp || !mBgSprite)
    {
        return;
    }
    
    std::string backend = "";
    if (RenderBackendState::Get().IsGL())
    {
        backend = "OpenGL";
    }
    if (RenderBackendState::Get().IsVK())
    {
        backend = "Vulkan";
    }

    // 表示文字列を組み立て（\n で改行）
    std::string text;
    text += "=== Debug ===\n";
    text += StringUtil::Format("Renderer   : <<\n",     backend);
    text += StringUtil::Format("FPS        : <<\n",     mSmoothedFPS);
    text += StringUtil::Format("FrameTime  : << ms\n",  stats.FrameTimeMs);
    text += StringUtil::Format("Actors     : <<\n",     stats.ActorCount);
    text += StringUtil::Format("Colliders  : <<\n",     stats.ColliderCount);
    text += StringUtil::Format("DrawCalls  : <<\n",     stats.DrawCallCount);
    text += StringUtil::Format("RTTCalls   : <<\n",     stats.OffDrawCallCount);
    text += StringUtil::Format("PhysTime   : << ms\n",  stats.PhysicsTimeMs);
    text += StringUtil::Format("RenderTime : << ms\n",  stats.RenderTimeMs);
    text += StringUtil::Format("Resolution : << x <<",  stats.ScreenW, stats.ScreenH);

    mTextComp->SetText(text);
    
    
    auto tex = mTextComp->GetTexture();
    if (!tex)
    {
        return;
    }
    
    const float pad = 6.0f;
    float w = static_cast<float>(tex->GetWidth());
    float h = static_cast<float>(tex->GetHeight());

    
    // 1x1背景なら「倍率=ピクセル」でも意図通り
    mBgSprite->SetScale(w + pad * 2.0f, h + pad * 2.0f);

    // FPS をちょっとだけ平滑化
    if (stats.FPS > 0.0f)
    {
        mSmoothedFPS = (mSmoothedFPS == 0.0f)
                     ? stats.FPS
                     : (mSmoothedFPS * 0.8f + stats.FPS * 0.2f);
    }

}

void DebugOverlayActor::ActorInput(const InputState &state)
{
    // F3 で ON/OFF
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
