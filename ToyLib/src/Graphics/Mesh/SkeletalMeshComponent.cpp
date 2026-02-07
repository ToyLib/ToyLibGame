#include "Graphics/Mesh/SkeletalMeshComponent.h"

#include "Engine/Runtime/AnimationPlayer.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"

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

void SkeletalMeshComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible || !mMesh || !mAnimPlayer)
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

    const auto& mats = mAnimPlayer->GetFinalMatrices();
    if (mats.empty())
    {
        return;
    }

    const Matrix4 view     = renderer->GetViewMatrix();
    const Matrix4 proj     = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    //=========================================================
    // World pass
    //=========================================================
    for (auto& va : mMesh->GetVertexArray())
    {
        if (!va)
        {
            continue;
        }

        RenderItem it {};
        it.type      = RenderItemType::SkinnedMesh;
        it.dispatch  = GetDispatch(it.type);

        it.pass      = RenderPass::World;
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.topology     = PrimitiveTopology::Triangles;
        it.geometry.ptr = va.get();
        it.indexCount   = static_cast<int>(va->GetNumIndices());

        // states
        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = (mIsBlendAdd ? BlendMode::Additive : BlendMode::Opaque);
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        // shader/material
        it.pipeline = renderer->GetPipelineHandle("Skinned");
        it.material = renderer->ToHandle(mMesh->GetMaterial(va->GetTextureID()));

        // transforms
        it.world    = world;
        it.viewProj = viewProj;

        // toon
        it.toon = mIsToon;

        // matrix palette
        it.matrixPalette = mats.data();
        it.paletteCount  = mats.size();

        queue.Push(it);

        //=====================================================
        // Outline
        //=====================================================
        if (mContourFactor > 1.0f)
        {
            RenderItem ol = it;

            ol.depthTest  = true;
            ol.depthWrite = false;
            ol.blend      = BlendMode::Opaque;

            // glFrontFace(GL_CW) 相当
            ol.cull      = CullMode::Back;
            ol.frontFace = FrontFace::CW;

            const Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
            ol.world = scaleOutline * world;

            ol.overrideColor      = true;
            ol.overrideColorValue = mContourColor;

            // 必要なら通常より先に描く（任意）
            // ol.drawOrder = GetDrawOrder() - 1;

            queue.Push(ol);
        }
    }
}

void SkeletalMeshComponent::GatherShadowItems(RenderQueue& queue)
{
    if (!mIsVisible || !mEnableShadow || !mMesh || !mAnimPlayer)
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

    const auto& mats = mAnimPlayer->GetFinalMatrices();
    if (mats.empty())
    {
        return;
    }

    const Matrix4 world =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        owner->GetRenderWorldTransform();

    for (auto& va : mMesh->GetVertexArray())
    {
        if (!va)
        {
            continue;
        }

        RenderItem it {};
        it.type      = RenderItemType::SkinnedMesh;
        it.dispatch  = GetDispatch(it.type);

        it.pass      = RenderPass::Shadow;
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.topology     = PrimitiveTopology::Triangles;
        it.geometry.ptr = va.get();
        it.indexCount   = static_cast<int>(va->GetNumIndices());

        it.world = world;

        // palette（ShadowSkinned 用）
        it.matrixPalette = mats.data();
        it.paletteCount  = mats.size();

        // state（深度のみ）
        it.depthTest  = true;
        it.depthWrite = true;
        it.blend      = BlendMode::Opaque;
        it.cull       = CullMode::Back;
        it.frontFace  = FrontFace::CCW;

        it.pipeline = renderer->GetPipelineHandle("ShadowSkinned");

        queue.Push(it);
    }
}
} // namespace toy
