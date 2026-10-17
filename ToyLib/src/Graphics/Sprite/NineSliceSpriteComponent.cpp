#include "Graphics/Sprite/NineSliceSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"
#include "Asset/Material/Texture.h"

#include <algorithm>

namespace toy {

NineSliceSpriteComponent::NineSliceSpriteComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
}

void NineSliceSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    VisualComponent::SetTexture(tex);

    mTexWidth  = tex ? tex->GetWidth()  : 0;
    mTexHeight = tex ? tex->GetHeight() : 0;
}

void NineSliceSpriteComponent::SetBorder(int left, int top, int right, int bottom)
{
    mBorder[0] = std::max(left,   0);
    mBorder[1] = std::max(top,    0);
    mBorder[2] = std::max(right,  0);
    mBorder[3] = std::max(bottom, 0);
}

void NineSliceSpriteComponent::GatherRenderItems(RenderQueue& out)
{
    if (!mIsVisible || !mTexture || mTexWidth <= 0 || mTexHeight <= 0)
        return;
    if (mSize.x <= 0.0f || mSize.y <= 0.0f)
        return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    const UIScaleInfo ui = renderer->GetUIScaleInfo();
    const float sw    = ui.screenW;
    const float sh    = ui.screenH;
    const float scale = ui.scale;

    const float texW = static_cast<float>(mTexWidth);
    const float texH = static_cast<float>(mTexHeight);

    //----------------------------------------------------------
    // 分割位置（描画側：論理 px / テクスチャ側：UV）
    //----------------------------------------------------------
    float dl = mBorder[0] * mCornerScale;
    float dt = mBorder[1] * mCornerScale;
    float dr = mBorder[2] * mCornerScale;
    float db = mBorder[3] * mCornerScale;

    // 描画サイズが角より小さいときは、角を比率を保って縮める
    if (dl + dr > mSize.x && dl + dr > 0.0f)
    {
        const float k = mSize.x / (dl + dr);
        dl *= k;
        dr *= k;
    }
    if (dt + db > mSize.y && dt + db > 0.0f)
    {
        const float k = mSize.y / (dt + db);
        dt *= k;
        db *= k;
    }

    const float xs[4] = { 0.0f, dl, mSize.x - dr, mSize.x };
    const float ys[4] = { 0.0f, dt, mSize.y - db, mSize.y };

    const float us[4] = { 0.0f, mBorder[0] / texW, 1.0f - mBorder[2] / texW, 1.0f };
    const float vs[4] = { 0.0f, mBorder[1] / texH, 1.0f - mBorder[3] / texH, 1.0f };

    const Vector3 base     = GetOwner()->GetPosition() + mOffset;
    const Matrix4 viewProj = Matrix4::CreateSimpleViewProj(sw, sh);

    //----------------------------------------------------------
    // 3x3 の各ピースを Sprite として積む
    //----------------------------------------------------------
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            const float w = xs[col + 1] - xs[col];
            const float h = ys[row + 1] - ys[row];
            if (w <= 0.0f || h <= 0.0f)
                continue;

            // 論理座標 → 画面中心原点（SpriteComponent の左上固定と同じ計算）
            const float width  = w * scale;
            const float height = h * scale;
            const float px = ui.offsetX + (base.x + xs[col]) * scale;
            const float py = ui.offsetY + (base.y + ys[row]) * scale;

            Vector3 pos;
            pos.x = (px + width  * 0.5f) - sw * 0.5f;
            pos.y = sh * 0.5f - (py + height * 0.5f);   // 上＋へ反転
            pos.z = base.z;

            Matrix4 world = Matrix4::CreateScale(width, height, 1.0f);
            world *= Matrix4::CreateTranslation(pos);

            SpritePayload sp;
            sp.color    = mColor;
            sp.alpha    = mAlpha * mAlphaMultiplier;
            sp.uvOffset = Vector2(us[col], vs[row]);
            sp.uvScale  = Vector2(us[col + 1] - us[col], vs[row + 1] - vs[row]);
            const uint32_t payloadIdx = out.PushSpritePayload(sp);

            RenderItem it;
            it.type         = RenderItemType::Sprite;
            it.dispatch     = GetDispatch(it.type);
            it.layer        = GetLayer();
            it.drawOrder    = GetDrawOrder();
            it.payloadIndex = payloadIdx;

            it.depthTest  = false;
            it.depthWrite = false;
            it.cull       = CullMode::Back;
            it.frontFace  = FrontFace::CCW;
            it.blend      = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;

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
    }
}

} // namespace toy
