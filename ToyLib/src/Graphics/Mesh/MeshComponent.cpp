#include "Graphics/Mesh/MeshComponent.h"

#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderItem.h"
#include "Engine/Render/RenderQueue.h"

namespace toy {

MeshComponent::MeshComponent(Actor* a, int drawOrder, VisualLayer layer, bool isSkeletal)
    : VisualComponent(a, drawOrder, layer)
    , mIsSkeletal(isSkeletal)
{
    mIsVisible    = true;
    mLayer        = layer;
    mEnableShadow = true;
}


std::shared_ptr<VertexArray> MeshComponent::GetVertexArray(int id) const
{
    if (!mMesh) return nullptr;
    auto& list = mMesh->GetVertexArray();
    if (id < 0 || id >= (int)list.size()) return nullptr;
    return list[(size_t)id];
}

void MeshComponent::GatherRenderItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mMesh) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // viewProj（ToyLib流：view * proj）
    const Matrix4 view     = renderer->GetViewMatrix();
    const Matrix4 proj     = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    // world（既存Draw合成）
    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    // submesh ごとに積む
    auto& vaList = mMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        if (!va) continue;

        RenderItem it{};
        it.type      = RenderItemType::Mesh;
        it.pass      = RenderPass::World;          // ★必須
        it.layer     = GetLayer();                 // ★必須（UI/Overlay/Object3D etc）
        it.drawOrder = GetDrawOrder();             // ★必須（ソート用）

        it.topology   = PrimitiveTopology::Triangles;
        it.geometry   = GeometryHandle{ va.get() };
        it.indexCount = (int)va->GetNumIndices();

        // states
        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = mIsBlendAdd ? BlendMode::Additive : BlendMode::Opaque;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        // shader/material
        it.shader   = renderer->GetShaderHandle("Mesh");
        it.material = renderer->ToHandle(mMesh->GetMaterial(va->GetTextureID()));

        // transforms
        it.world   = world;
        it.viewProj= viewProj;

        // toon flag only
        it.toon = mIsToon;

        out.Push(it);
    }
}

void MeshComponent::GatherShadowItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mEnableShadow || !mMesh) return;

    auto* owner = GetOwner();
    if (!owner) return;

    auto* renderer = owner->GetApp()->GetRenderer();
    if (!renderer) return;

    // world
    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    // submesh ごとに積む
    auto& vaList = mMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        if (!va) continue;

        RenderItem it{};
        it.type      = RenderItemType::Mesh;       // Shadow専用Typeが無いならこれでOK
        it.pass      = RenderPass::Shadow;         // ★Shadowパスへ
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.topology   = PrimitiveTopology::Triangles;
        it.geometry   = GeometryHandle{ va.get() };
        it.indexCount = (int)va->GetNumIndices();

        // ★カスケードごとの lightVP は Renderer の Shadow描画側で上書きする方針
        it.lightVP = renderer->GetLightSpaceMatrix(0); // 保険（0を入れておく）

        it.world = world;

        // states（深度のみ）
        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = BlendMode::Opaque;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        it.shader = renderer->GetShaderHandle("ShadowMesh");

        out.Push(it);
    }
}

} // namespace toy
