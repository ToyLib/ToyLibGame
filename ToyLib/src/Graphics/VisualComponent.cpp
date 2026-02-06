#include "Graphics/VisualComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/IRenderer.h"
#include "Engine/Render/LightingManager.h"

namespace toy {

VisualComponent::VisualComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : Component(owner)
    , mLayer(layer)          // 描画レイヤー
    , mDrawOrder(drawOrder)  // レイヤー内の描画順
{
    // ------------------------------------------------------------
    // Renderer に登録
    //   VisualComponent を持つインスタンスはすべて Renderer で管理され、
    //   レイヤー＆描画順でソートされて自動的に描画される。
    // ------------------------------------------------------------
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    renderer->AddVisualComp(this);

    // ライティングマネージャーを取得（描画時に使う）
    mLightingManager = renderer->GetLightingManager();

    // デフォルトの頂点配列（スプライト用クアッド）
    //   - Particle, Sprite, Overlay などが共通して使う
    mVertexArray = renderer->GetSpriteQuad();
}

VisualComponent::~VisualComponent()
{
    // ------------------------------------------------------------
    // Renderer から登録解除
    //   インスタンス破棄時にきちんと削除しておかないと、
    //   次フレームの描画ループで不正アクセスになる。
    // ------------------------------------------------------------
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    renderer->RemoveVisualComp(this);
}


} // namespace toy
