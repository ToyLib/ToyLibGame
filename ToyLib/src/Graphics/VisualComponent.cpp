#include "Graphics/VisualComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"

namespace toy {

VisualComponent::VisualComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : Component(owner)
    , mTexture(nullptr)
    , mIsVisible(true)       // デフォルトで可視
    , mIsBlendAdd(false)     // 通常ブレンド
    , mLayer(layer)          // 描画レイヤー
    , mDrawOrder(drawOrder)  // レイヤー内の描画順
    , mEnableShadow(false)   // 影を描かない（必要に応じて有効化）
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
    mVertexArray = renderer->GetSpriteVerts();
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
