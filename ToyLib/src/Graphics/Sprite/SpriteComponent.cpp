#include "Graphics/Sprite/SpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"
#include "Asset/Material/Texture.h"

namespace toy {

SpriteComponent::SpriteComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
    // ここでは Renderer / Shader / VAO などには触れない
    // 必要な情報は GatherRenderItems で Renderer から取得する
}


void SpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    VisualComponent::SetTexture(tex);

    if (tex)
    {
        mTexWidth  = tex->GetWidth();
        mTexHeight = tex->GetHeight();
    }
    else
    {
        mTexWidth  = 0;
        mTexHeight = 0;
    }
}

void SpriteComponent::GatherRenderItems(RenderQueue& out)
{
    if (!mIsVisible || !mTexture)
        return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // UIスケール
    const UIScaleInfo ui = renderer->GetUIScaleInfo();
    const float sw    = ui.screenW;
    const float sh    = ui.screenH;
    const float scale = ui.scale;

    // 表示サイズ（ピクセル）
    const float texW   = static_cast<float>(mTexWidth);
    const float texH   = static_cast<float>(mTexHeight);
    const float width  = texW * mScaleWidth  * scale;
    const float height = texH * mScaleHeight * scale;

    // 位置（既存ロジック踏襲）
    Vector3 pos;
    {
        const Vector3 logicalPos = GetOwner()->GetPosition() + mOffset;

        const float px = ui.offsetX + logicalPos.x * scale;
        const float py = ui.offsetY + logicalPos.y * scale;

        if (mIsTopLeft)
        {
            const float cx = px + width  * 0.5f;
            const float cy = py + height * 0.5f;

            pos.x = cx - sw * 0.5f;
            pos.y = sh * 0.5f - cy;   // 上＋へ反転
            pos.z = logicalPos.z;
        }
        else
        {
            pos.x = px - sw * 0.5f;
            pos.y = sh * 0.5f - py;
            pos.z = logicalPos.z;
        }
    }

    // 行列（2D: simple view/proj）
    Matrix4 world = Matrix4::CreateScale(width, height, 1.0f);
    world *= Matrix4::CreateTranslation(pos);

    const Matrix4 viewProj = Matrix4::CreateSimpleViewProj(sw, sh);

    // ----------------------------
    // Payload（Sprite 専用）を先に積む
    // ----------------------------
    SpritePayload sp;
    sp.color = mColor;
    sp.alpha = mAlpha;
    const uint32_t payloadIdx = out.PushSpritePayload(sp);

    // ----------------------------
    // RenderItem
    // ----------------------------
    RenderItem it;
    it.type        = RenderItemType::Sprite;
    it.dispatch    = GetDispatch(it.type);
    it.layer       = GetLayer();
    it.drawOrder   = GetDrawOrder();
    it.payloadIndex = payloadIdx;

    // UIスプライトの基本state
    it.depthTest  = false;
    it.depthWrite = false;
    it.cull       = CullMode::Back;
    it.frontFace  = FrontFace::CCW;
    it.blend      = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;

    // geometry/shader/params
    it.geometry   = renderer->GetSpriteQuadHandle();
    it.indexCount = 6;

    it.pipeline = renderer->GetPipelineHandle("Sprite");
    it.world    = world;
    it.viewProj = viewProj;

    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;
    
    it.pass = RenderPass::UI;


    out.Push(it);
}

} // namespace toy
