#include "Graphics/Effect/SceneCaptureComponent.h"

//------------------------------------------------------------------------------
// Engine / Core
//------------------------------------------------------------------------------
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

//------------------------------------------------------------------------------
// Engine / Render
//------------------------------------------------------------------------------
#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderTarget.h"

//------------------------------------------------------------------------------
// Asset
//------------------------------------------------------------------------------
#include "Asset/Material/Texture.h"

//------------------------------------------------------------------------------
// GL
//------------------------------------------------------------------------------
#include "glad/glad.h"

namespace toy {

//==============================================================================
// コンストラクタ
//==============================================================================
SceneCaptureComponent::SceneCaptureComponent(Actor* owner)
    : Component(owner)
    , mView(Matrix4::Identity)
    , mProj(Matrix4::Identity)
{
}

//==============================================================================
// 初期化
//==============================================================================
void SceneCaptureComponent::Init(const Desc& desc)
{
    // 設定コピー
    mDesc = desc;

    // 解像度の最低保証
    mDesc.width  = (mDesc.width  > 0) ? mDesc.width  : 1;
    mDesc.height = (mDesc.height > 0) ? mDesc.height : 1;

    // RenderTarget 作成
    mRT = std::make_shared<RenderTarget>();
    mRT->Create(mDesc.width, mDesc.height);

    // 更新タイマー初期化
    mAcc = 0.0f;
}

//==============================================================================
// 出力
//==============================================================================
std::shared_ptr<Texture> SceneCaptureComponent::GetColorTexture() const
{
    return mRT ? mRT->GetColorTexture() : nullptr;
}

//==============================================================================
// キャプチャ用カメラ設定
//==============================================================================
void SceneCaptureComponent::SetViewProj(const Matrix4& view, const Matrix4& proj)
{
    mView = view;
    mProj = proj;
}

//==============================================================================
// 更新
//==============================================================================
void SceneCaptureComponent::Update(float deltaTime)
{
    // 無効 or 未初期化
    if (!mDesc.enabled || !mRT)
    {
        return;
    }

    // 毎フレーム更新
    if (mDesc.updateHz <= 0.0f)
    {
        Capture();
        return;
    }

    // updateHz ベースの間引き更新
    const float interval = 1.0f / mDesc.updateHz;
    mAcc += deltaTime;
    if (mAcc >= interval)
    {
        mAcc = 0.0f;
        Capture();
    }
}

//==============================================================================
// キャプチャ実行
//==============================================================================
void SceneCaptureComponent::Capture()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    //----------------------------------------------------------------------
    // Actor の Transform からカメラ基底を作成
    //----------------------------------------------------------------------
    const Matrix4 world = GetOwner()->GetWorldTransform();

    // ToyLib の軸系想定：
    //   前方 = +Z, 上 = +Y（ただし視線は -Z を使う）
    Vector3 camPos = world.GetTranslation();
    const Vector3 camFwd = -1.0f * world.GetZAxis(); // -Z が前
    const Vector3 camUp  = world.GetYAxis();

    camPos = camPos - camFwd * 10.0f;
    const Vector3 target = camPos + camFwd * 100.0f;

    mView = Matrix4::CreateLookAt(camPos, target, camUp);

    //----------------------------------------------------------------------
    // Projection（RenderTarget サイズから aspect を構成）
    //----------------------------------------------------------------------
    const float w = static_cast<float>(mRT->GetWidth());
    const float h = static_cast<float>(mRT->GetHeight());

    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mDesc.fov),
        mDesc.width,
        mDesc.height,
        0.1f,
        1000.0f
    );

    //----------------------------------------------------------------------
    // RenderTarget へ描画
    //----------------------------------------------------------------------
    renderer->DrawToRenderTarget(mRT, mView, mProj, mDesc.drawUI);
}

} // namespace toy
