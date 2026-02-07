#include "Graphics/Mesh/MeshComponent.h"

#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"

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
    if (!mMesh)
    {
        return nullptr;
    }
    auto& list = mMesh->GetVertexArray();
    if (id < 0 || id >= (int)list.size())
    {
        return nullptr;
    }
    return list[(size_t)id];
}

void MeshComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible || !mMesh)
    {
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    // ワールド行列（旧 Draw と同じ）
    const Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    // 共有：ビュー・プロジェクション
    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    // サブメッシュごとに積む（Materialが違うため）
    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        if (!v)
        {
            continue;
        }

        // ---- 通常描画 ----
        RenderItem it {};
        it.pass      = RenderPass::World;
        it.layer     = mLayer;
        it.drawOrder = mDrawOrder;

        it.type     = RenderItemType::Mesh;
        it.dispatch = GetDispatch(it.type);

        it.topology     = PrimitiveTopology::Triangles;
        it.geometry.ptr = v.get();
        it.indexCount   = static_cast<int>(v->GetNumIndices());

        it.pipeline = renderer->GetPipelineHandle("Mesh");
        it.world    = worldMatrix;
        it.viewProj = viewProj;

        it.toon = mIsToon;

        it.blend      = (mIsBlendAdd ? BlendMode::Additive : BlendMode::Opaque);
        it.depthTest  = true;
        it.depthWrite = true;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        auto mat = mMesh->GetMaterial(v->GetTextureID());
        it.material = renderer->ToHandle(mat);

        queue.Push(it);

        // ---- アウトライン ----
        if (mContourFactor > 1.0f)
        {
            RenderItem o = it;

            const Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
            o.world = scaleOutline * worldMatrix;

            o.frontFace = FrontFace::CW;

            o.overrideColor      = true;
            o.overrideColorValue = mContourColor;

            o.drawOrder = mDrawOrder - 1;

            queue.Push(o);
        }
    }
}
void MeshComponent::GatherShadowItems(RenderQueue& queue)
{
    if (!mIsVisible || !mEnableShadow || !mMesh)
    {
        return;
    }

    auto* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    auto* renderer = owner->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    auto& vaList = mMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        if (!va)
        {
            continue;
        }

        RenderItem it {};
        it.pass      = RenderPass::Shadow;
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.type     = RenderItemType::Mesh;
        it.dispatch = GetDispatch(it.type);

        it.topology     = PrimitiveTopology::Triangles;
        it.geometry.ptr = va.get();
        it.indexCount   = static_cast<int>(va->GetNumIndices());

        it.world = world;

        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = BlendMode::Opaque;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        it.pipeline = renderer->GetPipelineHandle("ShadowMesh");

        queue.Push(it);
    }
}

} // namespace toy
