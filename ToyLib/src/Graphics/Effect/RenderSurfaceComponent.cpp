#include "Graphics/Effect/RenderSurfaceComponent.h"

//------------------------------------------------------------------------------
// Engine / Core
//------------------------------------------------------------------------------
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

//------------------------------------------------------------------------------
// Engine / Render
//------------------------------------------------------------------------------
#include "Engine/Render/Renderer.h"
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

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }
    
    RenderItem it;
    it.type = RenderItemType::Surface;
    it.pass = RenderPass::World;

    //====================
    // Geometry / Shader
    //====================
    it.geometry = renderer->GetSurfaceQuadHandle();
    it.indexCount  = 6;   // ★ 必須（SurfaceQuad は EBO 6 前提）
    it.vertexCount = 0;
    it.shader.ptr   = mShader.get();

    //====================
    // Transform
    //====================
    GetOwner()->ComputeWorldTransform();

    Matrix4 sc = Matrix4::CreateScale(mScaleX, mScaleY, 1.0f);
    it.world   = sc * GetOwner()->GetWorldTransform();

    it.viewProj = renderer->GetViewProjMatrix();

    //====================
    // Texture
    //====================
    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    //====================
    // State
    //====================
    it.depthTest  = true;
    it.depthWrite = true;
    it.cull       = CullMode::Back;
    it.blend      = BlendMode::Opaque;

    //====================
    // Surface params
    //====================
    it.surfaceOpacity = mOpacity;
    it.surfaceTint    = mTint;
    it.surfaceFlipX   = mFlipX;
    it.surfaceFlipY   = mFlipY;
    int mode = 3; // Plain は fallback（= 3 など “0..2以外”）
    switch (mMode)
    {
        case SurfaceMode::Plain:
            mode = 3;
            break; // fallback
        case SurfaceMode::Monitor:
            mode = 0;
            break;
        case SurfaceMode::Mirror:
            mode = 1;
            break;
        case SurfaceMode::Water:
            mode = 2;
            break;
    }
    it.surfaceMode = mode;

    // 時間
    it.time = GetOwner()->GetApp()->GetTimeSconds();

    //====================
    // Dispatch
    //====================
    it.dispatch = GetDispatch(it.type);

    queue.Push(it);
}

} // namespace toy
