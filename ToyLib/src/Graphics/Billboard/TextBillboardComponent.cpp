// Graphics/Billboard/TextBillboardComponent.cpp
#include "Graphics/Billboard/TextBillboardComponent.h"

#include "Asset/Font/TextFont.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"

namespace toy {

//==============================================================
// ctor / dtor
//==============================================================
TextBillboardComponent::TextBillboardComponent(Actor* owner,
                                               int drawOrder)
: BillboardComponent(owner, drawOrder)
{
    // Shader は Unlit（文字はライティング不要）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Unlit");
}

TextBillboardComponent::~TextBillboardComponent()
{
}

//==============================================================
// Text / Color / Font
//==============================================================
void TextBillboardComponent::SetText(const std::string& text)
{
    if (mText == text)
        return;

    mText = text;
    mIsDirty = true;
}

void TextBillboardComponent::SetColor(const Vector3& color)
{
    mColor = color;
    mIsDirty = true;
}

void TextBillboardComponent::SetFont(std::shared_ptr<TextFont> font)
{
    if (mFont == font)
        return;

    mFont = std::move(font);
    mIsDirty = true;
}

void TextBillboardComponent::Refresh()
{
    mIsDirty = true;
}

//==============================================================
// RenderQueue 用 Gather
//==============================================================
void TextBillboardComponent::GatherRenderItems(RenderQueue& out)
{
    // Dirty 解決
    if (mIsDirty)
    {
        UpdateTexture();
    }

    if (!mFont || mText.empty() || !mTexture)
        return;

    auto* app = GetOwner()->GetApp();
    if (!app)
        return;

    auto* renderer = app->GetRenderer();
    if (!renderer)
        return;

    RenderItem it{};
    it.type      = RenderItemType::Billboard;
    it.pass      = RenderPass::World;
    it.layer     = GetLayer();          // Effect3D
    it.drawOrder = GetDrawOrder();

    // geometry（共通板ポリ）
    it.geometry   = renderer->GetSpriteQuadHandle();
    it.indexCount = 6;

    // shader
    it.shader = renderer->GetShaderHandle("Unlit");

    // texture
    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    //----------------------------------------------------------
    // Billboard 向き計算（Y固定・XZ回転）
    //----------------------------------------------------------
    Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    Vector3 pos = actorWorld.GetTranslation();

    Matrix4 invView = renderer->GetInvViewMatrix();
    Vector3 camPos  = invView.GetTranslation();

    Vector3 toCamera = pos - camPos;
    toCamera.y = 0.0f;

    if (toCamera.LengthSq() < 1.0e-6f)
        toCamera = Vector3::UnitZ;
    else
        toCamera.Normalize();

    float angle = atan2f(toCamera.x, toCamera.z);

    float scale = GetScale() * GetOwner()->GetScale();

    Matrix4 world =
        Matrix4::CreateScale(
            mTexture->GetWidth()  * scale,
            mTexture->GetHeight() * scale,
            1.0f
        ) *
        Matrix4::CreateRotationY(angle) *
        Matrix4::CreateTranslation(pos);

    it.world    = world;
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();

    //----------------------------------------------------------
    // Render State（旧 Draw() 挙動寄せ）
    //----------------------------------------------------------
    it.depthTest  = true;
    it.depthWrite = false;                 // ★重要：透過
    it.blend      = BlendMode::Alpha;
    it.cull       = CullMode::None;
    it.frontFace  = FrontFace::CCW;

    out.Push(it);
}

//==============================================================
// Texture 更新
//==============================================================
void TextBillboardComponent::UpdateTexture()
{
    mIsDirty = false;

    if (mText.empty() || !mFont || !mFont->IsValid())
    {
        SetTexture(nullptr);
        return;
    }

    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        SetTexture(nullptr);
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        SetTexture(nullptr);
        return;
    }

    auto tex = renderer->CreateTextTexture(mText, mColor, mFont);
    if (!tex)
    {
        SetTexture(nullptr);
        return;
    }

    SetTexture(tex);
}

} // namespace toy
