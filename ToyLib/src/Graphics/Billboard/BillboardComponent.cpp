#include "Graphics/Billboard/BillboardComponent.h"

#include "Asset/Material/Texture.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"

#include <cmath>

namespace toy {

BillboardComponent::BillboardComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
    // Shader / Geometry は Renderer 側で選ぶのでここでは持たない（新パス）
}

//----------------------------------------------------------------------
// GatherRenderItems
//  - Billboard を RenderQueue に積む（新パス）
//----------------------------------------------------------------------
void BillboardComponent::GatherRenderItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mTexture) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer) return;

    // カメラ行列
    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();

    // Actor ワールド位置（元コードに合わせる）
    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 pos = actorWorld.GetTranslation();

    // カメラ位置（invView から）
    const Vector3 cameraPos = renderer->GetInvViewMatrix().GetTranslation();

    // 水平 billboard（元コード通り）
    Vector3 toCamera = pos - cameraPos;
    toCamera.y = 0.0f;

    if (toCamera.LengthSq() < 1.0e-6f)
    {
        toCamera = Vector3::UnitZ;
    }
    else
    {
        toCamera.Normalize();
    }

    const float angle = std::atan2f(toCamera.x, toCamera.z);
    const Matrix4 rotY = Matrix4::CreateRotationY(angle);

    // スケール（元コード：mScale * ownerScale、かつ texture size を掛ける）
    const float scale = mScale * GetOwner()->GetScale();

    const Matrix4 scaleMat = Matrix4::CreateScale(
        mTexture->GetWidth()  * scale,
        mTexture->GetHeight() * scale,
        1.0f
    );

    const Matrix4 translate = Matrix4::CreateTranslation(pos);

    // ★元コードの world 掛け順を維持
    const Matrix4 world = scaleMat * rotY * translate;

    // （元コードにあった副作用：Owner 回転反映）
    // これが必要なら残す（挙動合わせ優先なら残してOK）
    {
        Quaternion q = Quaternion(Vector3::UnitY, angle);
        GetOwner()->SetRotation(q);
    }

    RenderItem it{};
    it.type = RenderItemType::Billboard;                 // 「Meshとして出す」を維持
    it.shader = renderer->GetShaderHandle("Mesh");  // Phong
    it.viewProj = view * proj;
    it.world    = world;

    // ★Rendererの共通Quadを使う（設計維持）
    it.geometry = renderer->GetSpriteQuadHandle();  // すでにある想定
    it.indexCount = 6;

    // ★Materialが無いなら texture を直接渡す（最小変更）
    it.texture = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    // state（元 Draw の想定に近い）
    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;

    // 板ポリは片面カリングで消えやすいので、元挙動に近い“見える”を優先
    it.cull      = CullMode::Front;
    it.frontFace = FrontFace::CW;

    out.Push(it);
}
} // namespace toy
