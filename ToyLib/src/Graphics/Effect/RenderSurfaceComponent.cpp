#include "Graphics/Effect/RenderSurfaceComponent.h"

//------------------------------------------------------------------------------
// Engine / Core
//------------------------------------------------------------------------------
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

//------------------------------------------------------------------------------
// Engine / Render
//------------------------------------------------------------------------------
#include "Engine/Render/IRenderer.h"
#include "Engine/Render/Shader.h"

//------------------------------------------------------------------------------
// Asset
//------------------------------------------------------------------------------
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

//------------------------------------------------------------------------------
// GL
//------------------------------------------------------------------------------
#include "glad/glad.h"

namespace toy {

//==============================================================================
// コンストラクタ
//==============================================================================
RenderSurfaceComponent::RenderSurfaceComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder, VisualLayer::Object3D)
{
    // 置物の板ポリ：ビルボードなし、通常 3D と同じ扱い
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    mVertexArray   = renderer->GetSurfaceQuad();
    mShader        = renderer->GetShader("RenderSurface");
}

//==============================================================================
// 描画
//==============================================================================
void RenderSurfaceComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!IsVisible() || !mTexture)
    {
        return;
    }

    auto* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    auto* app = owner->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    //====================
    // Surface payload
    //====================
    SurfacePayload sp {};
    sp.opacity = mOpacity;
    sp.tint    = mTint;
    sp.flipX   = mFlipX;
    sp.flipY   = mFlipY;
    sp.scanlineStrength = mScanlineStrength;

    int mode = 0; // Plain fallback
    switch (mMode)
    {
        case SurfaceMode::Plain:   mode = 0; break;
        case SurfaceMode::Monitor: mode = 1; break;
        case SurfaceMode::Mirror:  mode = 2; break;
        case SurfaceMode::Water:   mode = 3; break;
        default:                   mode = 0; break;
    }
    sp.mode = mode;

    // 時間（関数名 typo っぽいので元を尊重）
    sp.time = app->GetTimeSconds();

    const uint32_t payloadIndex = queue.PushSurfacePayload(sp);

    //====================
    // RenderItem
    //====================
    RenderItem it {};
    it.type = RenderItemType::Surface;
    it.pass = RenderPass::World;

    // layer / order（Surface がどこに属するかは今の挙動踏襲）
    it.layer     = GetLayer();
    it.drawOrder = GetDrawOrder();

    // Geometry / Shader
    it.geometry    = renderer->GetSurfaceQuadHandle();
    it.indexCount  = 6;   // ★ 必須（SurfaceQuad は EBO 6 前提）
    it.vertexCount = 0;

    // できれば Handle 化したいが、今は最小変更で維持
    it.shader.ptr = mShader.get();

    // Transform
    owner->ComputeWorldTransform();

    Matrix4 sc = Matrix4::CreateScale(mScaleX, mScaleY, 1.0f);
    it.world   = sc * owner->GetWorldTransform();

    it.viewProj = renderer->GetViewProjMatrix();

    // Texture
    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    // State
    it.depthTest  = true;
    it.depthWrite = true;
    it.cull       = CullMode::Back;
    it.frontFace  = FrontFace::CCW;
    it.blend      = BlendMode::Alpha;

    // Payload
    it.payloadIndex = payloadIndex;

    // Dispatch
    it.dispatch = GetDispatch(it.type);

    queue.Push(it);
}
} // namespace toy
