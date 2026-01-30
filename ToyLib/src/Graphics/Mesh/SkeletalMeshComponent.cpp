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
    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // Skinned専用shaderへ差し替え（新パスでは RenderItem 側で選ぶが、ここも合わせておく）
    mShader       = renderer->GetShader("Skinned");
    mShadowShader = renderer->GetShader("ShadowSkinned");
}

void SkeletalMeshComponent::Draw()
{
    // 旧描画パス廃止
}

void SkeletalMeshComponent::DrawShadow(int /*cascadeIndex*/)
{
    // 旧描画パス廃止
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

void SkeletalMeshComponent::GatherRenderItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mMesh || !mAnimPlayer) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    const auto& mats = mAnimPlayer->GetFinalMatrices();
    if (mats.empty()) return;

    const Matrix4 view     = renderer->GetViewMatrix();
    const Matrix4 proj     = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    for (auto& va : mMesh->GetVertexArray())
    {
        if (!va) continue;

        RenderItem it{};
        it.type      = RenderItemType::SkinnedMesh;
        it.pass      = RenderPass::World;          // ★必須
        it.layer     = GetLayer();                 // ★必須
        it.drawOrder = GetDrawOrder();             // ★必須

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

        // toon flag
        it.toon = mIsToon;

        // ★matrix palette
        it.matrixPalette = mats.data();
        it.paletteCount  = (int)mats.size();

        out.Push(it);
    }
}

void SkeletalMeshComponent::GatherShadowItems(RenderQueueLike& out)
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
