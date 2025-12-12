// Engine/Debug/DebugOverlayActor.cpp
#include "Engine/Debug/DebugOverlayActor.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Debug/DebugStats.h"
#include "Asset/Font/TextFont.h"
#include "Asset/AssetManager.h"
#include "Graphics/Sprite/TextSpriteComponent.h"

#include <SDL3/SDL_scancode.h> // SDL2 なら <SDL2/SDL_scancode.h>

namespace toy {

DebugOverlayActor::DebugOverlayActor(Application* app)
: Actor(app)
, mEnabled(false)
, mWireVisible(false)
, mTextComp(nullptr)
, mSmoothedFPS(0.0f)
, mTextColor(Vector3(0.3f, 1.0f, 0.3f))
{
    SetActorID("DebugOverlay");

    // 左上に固定（SpriteComponent 側で原点が左上になるようにしている前提）
    SetPosition(Vector3::Zero);

    // TextSpriteComponent を作成（UI レイヤー）
    mTextComp = CreateComponent<TextSpriteComponent>(/*drawOrder*/ 1000,
                                                     VisualLayer::UI);

    // 好きなデバッグ用フォントを取得
    auto debugFont = app->mSystemAssetManager->GetFont("Hermit-Bold.otf", 20);
    mTextComp->SetFont(debugFont);

    // 色
    mTextComp->SetColor(mTextColor);
}

void DebugOverlayActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;
    if (mTextComp)
    {
        mTextComp->SetVisible(enabled);
    }
}
void DebugOverlayActor::SetWireVisible(bool visible)
{
    mWireVisible = visible;
    GetApp()->GetRenderer()->SetDebugWireVisible(visible);
}


void DebugOverlayActor::UpdateActor(float deltaTime)
{
    Actor::UpdateActor(deltaTime);

    auto* app   = GetApp();
    auto& stats = app->GetDebugStats();


    
    if (!mEnabled || !mTextComp)
    {
        return;
    }

    // FPS をちょっとだけ平滑化
    if (stats.FPS > 0.0f)
    {
        mSmoothedFPS = (mSmoothedFPS == 0.0f)
                     ? stats.FPS
                     : (mSmoothedFPS * 0.8f + stats.FPS * 0.2f);
    }

    // 表示文字列を組み立て（\n で改行）
    std::string text;
    text += "=== Debug ===\n";
    text += StringUtil::Format("FPS        : <<\n",     mSmoothedFPS);
    text += StringUtil::Format("FrameTime  : << ms\n",  stats.FrameTimeMs);
    text += StringUtil::Format("Actors     : <<\n",     stats.ActorCount);
    text += StringUtil::Format("Colliders  : <<\n",     stats.ColliderCount);
    text += StringUtil::Format("DrawObjects: <<\n",     stats.DrawObjectCount);
    text += StringUtil::Format("DrawCalls  : <<\n",     stats.DrawCallCount);
    text += StringUtil::Format("PhysTime   : << ms\n",  stats.PhysicsTimeMs);
    text += StringUtil::Format("RenderTime : << ms\n",  stats.RenderTimeMs);

    mTextComp->SetText(text);
}

void DebugOverlayActor::ActorInput(const InputState &state)
{
    // F3 で ON/OFF
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_F3) == EPressed)
    {
        SetEnabled(!mEnabled);
    }
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_F2) == EPressed)
    {
        SetWireVisible(!mWireVisible);
    }
    
}

} // namespace toy
