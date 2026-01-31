#include "Graphics/Mesh/SkeletalMeshComponent.h"

#include "Engine/Runtime/AnimationPlayer.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderItem.h"
#include "Engine/Render/RenderQueue.h"

namespace toy {

SkeletalMeshComponent::SkeletalMeshComponent(Actor* a, int drawOrder, VisualLayer layer)
    : MeshComponent(a, drawOrder, layer, true)
{
    
}

void SkeletalMeshComponent::Update(float deltaTime)
{
    if (mAnimPlayer)
    {
        mAnimPlayer->Update(deltaTime);
    }
}

void SkeletalMeshComponent::SetAnimID(unsigned int animID, bool /*mode*/)
{
    if (mAnimPlayer)
    {
        mAnimPlayer->Play(animID, true);
    }
}

void SkeletalMeshComponent::SetMesh(std::shared_ptr<Mesh> mesh)
{
    MeshComponent::SetMesh(mesh);

    if (mesh)
    {
        mAnimPlayer = std::make_unique<AnimationPlayer>(mesh);
    }
    else
    {
        mAnimPlayer.reset();
    }
}

void SkeletalMeshComponent::GatherRenderItems(RenderQueue& out)
{
    if (!mIsVisible || !mMesh || !mAnimPlayer) return;

    auto* owner = GetOwner();
    if (!owner) return;

    auto* renderer = owner->GetApp()->GetRenderer();
    if (!renderer) return;

    const auto& mats = mAnimPlayer->GetFinalMatrices();
    if (mats.empty()) return;

    const Matrix4 view     = renderer->GetViewMatrix();
    const Matrix4 proj     = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    //=========================================================
    // 1) 通常メッシュ
    //=========================================================
    for (auto& va : mMesh->GetVertexArray())
    {
        if (!va) continue;

        RenderItem it{};
        it.type      = RenderItemType::SkinnedMesh;
        it.dispatch  = GetDispatch(it.type);
        it.pass      = RenderPass::World;
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

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
        it.shader   = renderer->GetShaderHandle("Skinned");
        it.material = renderer->ToHandle(mMesh->GetMaterial(va->GetTextureID()));

        // transforms
        it.world    = world;
        it.viewProj = viewProj;

        // toon
        it.toon = mIsToon;

        // matrix palette
        it.matrixPalette = mats.data();
        it.paletteCount  = (int)mats.size();

        out.Push(it);

        //=====================================================
        // 2) 輪郭（アウトライン）
        //   - mContourFactor > 1.0f のときだけ
        //   - CW にして「裏面」を描く
        //   - overrideColor で塗りつぶし色にする
        //=====================================================
        if (mContourFactor > 1.0f)
        {
            RenderItem ol = it; // まずコピーして差分だけ変える

            ol.depthTest  = true;
            ol.depthWrite = false;              // お好み：輪郭は深度を書かない方が無難
            ol.blend      = BlendMode::Opaque;  // 輪郭は基本不透明
            ol.cull       = CullMode::Back;     // CWにした上で Back を消す＝表が出る（旧glFrontFace(CW)想定）
            ol.frontFace  = FrontFace::CW;

            // ★スケールアップ
            const Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
            ol.world = scaleOutline * world;

            // ★色上書き（DispatchSkinnedMesh が SetOverrideColor→Bind→解除 する想定）
            ol.overrideColor      = true;
            ol.overrideColorValue = mContourColor;

            out.Push(ol);
        }
    }
}

void SkeletalMeshComponent::GatherShadowItems(RenderQueue& out)
{
    if (!mIsVisible || !mEnableShadow || !mMesh || !mAnimPlayer) return;

    auto* owner = GetOwner();
    if (!owner) return;

    auto* renderer = owner->GetApp()->GetRenderer();
    if (!renderer) return;

    const auto& mats = mAnimPlayer->GetFinalMatrices();
    if (mats.empty()) return;

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    for (auto& va : mMesh->GetVertexArray())
    {
        if (!va) continue;

        RenderItem it{};
        it.type      = RenderItemType::SkinnedMesh;
        it.dispatch  = GetDispatch(it.type);
        it.pass      = RenderPass::Shadow;         // ★Shadowパスへ
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.topology   = PrimitiveTopology::Triangles;
        it.geometry   = GeometryHandle{ va.get() };
        it.indexCount = (int)va->GetNumIndices();

        it.world = world;

        // ★カスケードごとの lightVP は Renderer の Shadow描画側で上書きする方針
        it.lightVP = renderer->GetLightSpaceMatrix(0); // 保険（0を入れておく）

        // palette
        it.matrixPalette = mats.data();
        it.paletteCount  = (int)mats.size();

        // state（深度のみ）
        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = BlendMode::Opaque;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        it.shader = renderer->GetShaderHandle("ShadowSkinned");

        out.Push(it);
    }
}

} // namespace toy
