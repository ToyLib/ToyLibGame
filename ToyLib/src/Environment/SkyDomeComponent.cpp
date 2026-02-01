// Environment/SkyDomeComponent.cpp
#include "Environment/SkyDomeComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"

namespace toy {

SkyDomeComponent::SkyDomeComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder, layer)
{
    // Sky は最背面なので専用レイヤーがあるならそれを使う
    // 無ければ Effect3D や Object3D でも可
    mLayer = VisualLayer::Sky; // ★ VisualLayer::Sky が無いなら追加推奨
    
    // Renderer に「スカイドームとして登録」
    // → Renderer側の描画パイプラインで SkyDomeComponent が呼ばれるようになる
        // スカイドーム描画用の基本シェーダ取得
    // 派生クラスがここに Uniform を詰めて描画する
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("SkyDome");
    
    auto light = GetOwner()->GetApp()->GetRenderer()->GetLightingManager();
    SetLightingManager(light);
    
}
SkyDomeComponent::~SkyDomeComponent() = default;

void SkyDomeComponent::SetSkyGeometry(std::unique_ptr<VertexArray> vao)
{
    mSkyVAO = std::move(vao);
}

void SkyDomeComponent::SetSkyShader(std::shared_ptr<Shader> shader)
{
    mShader = std::move(shader);
}

void SkyDomeComponent::Update(float /*deltaTime*/)
{
    // ベースはロジック無し（派生が更新）
}

void SkyDomeComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (!mIsVisible)
    {
        return;
    }

    if (!mSkyVAO || !mShader)
    {
        return;
    }
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }
    // カメラ位置に追従させる
    const Matrix4 invView = renderer->GetInvViewMatrix();
    const Vector3 camPos  = invView.GetTranslation();

    const Matrix4 world =
        Matrix4::CreateScale(mSkyScale) *
        Matrix4::CreateTranslation(camPos);

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    RenderItem it;
    it.pass      = RenderPass::World;
    it.layer     = mLayer;
    it.drawOrder = GetDrawOrder();
    it.type      = RenderItemType::SkyDome;
    it.dispatch  = GetDispatch(it.type);

    // Sky の定番ステート
    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = BlendMode::Opaque;
    it.cull       = CullMode::Front;      // ★ 内側を描く
    it.frontFace  = FrontFace::CCW;

    it.shader.ptr  = mShader.get();
    it.geometry.ptr = mSkyVAO.get();

    it.world    = world;
    it.viewProj = viewProj;

    it.topology   = PrimitiveTopology::Triangles;
    it.indexCount = mSkyVAO->GetNumIndices();

    // SkyDome の中身（uniform）は派生で埋める
    // ベースは何も入れない（安全なデフォルト）
    outQueue.Push(it);
}

} // namespace toy
