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

void MeshComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible) return;
    if (!mMesh) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer) return;

    // ワールド行列（旧 Draw と同じ）
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    // 共有：ビュー・プロジェクション
    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();

    // サブメッシュごとに積む（Materialが違うため）
    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        if (!v) continue;

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

        it.shader   = renderer->GetShaderHandle("Mesh");   // ★旧と同じ Mesh shader
        it.world    = worldMatrix;
        it.viewProj = view * proj;

        it.toon = mIsToon;

        it.blend      = (mIsBlendAdd ? BlendMode::Additive : BlendMode::Opaque);
        it.depthTest  = true;
        it.depthWrite = true;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        // Material（旧：v->GetTextureID()で取ってたのと同じ）
        auto mat = mMesh->GetMaterial(v->GetTextureID());
        it.material = renderer->ToHandle(mat);

        queue.Push(it);

        // ---- アウトライン（旧 Draw の輪郭処理そのまま） ----
        if (mContourFactor > 1.0f)
        {
            RenderItem o = it;

            // わずかにスケールアップしたワールド行列（旧：scaleOutline * worldMatrix）
            Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
            o.world = scaleOutline * worldMatrix;

            // glFrontFace(GL_CW) と同義（裏面だけ残す）
            o.frontFace = FrontFace::CW;

            // 色を Material 上書き（旧：mat->SetOverrideColor(true, mContourColor)）
            o.overrideColor      = true;
            o.overrideColorValue = mContourColor;

            // 通常より先に描く（必要なら）
            o.drawOrder = mDrawOrder - 1;

            queue.Push(o);
        }
    }
}
void MeshComponent::GatherShadowItems(RenderQueue& out)
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
        it.dispatch = GetDispatch(it.type);
        
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
