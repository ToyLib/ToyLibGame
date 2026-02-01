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

#include <cmath>

namespace toy {

//==============================================================================
// ctor
//==============================================================================
SceneCaptureComponent::SceneCaptureComponent(Actor* owner)
    : Component(owner)
{
}

//==============================================================================
// Init
//==============================================================================
void SceneCaptureComponent::Init(const Desc& desc)
{
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
// Output
//==============================================================================
std::shared_ptr<Texture> SceneCaptureComponent::GetColorTexture() const
{
    return mRT ? mRT->GetColorTexture() : nullptr;
}

//==============================================================================
// SetViewProj
//==============================================================================
void SceneCaptureComponent::SetViewProj(const Matrix4& view, const Matrix4& proj)
{
    mView = view;
    mProj = proj;
}

//==============================================================================
// Update
//==============================================================================
void SceneCaptureComponent::Update(float deltaTime)
{
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

    // updateHz ベースの間引き
    const float interval = 1.0f / mDesc.updateHz;
    mAcc += deltaTime;

    if (mAcc >= interval)
    {
        // 取りこぼし防止：interval分だけ進める
        // （必要なければ mAcc=0 でもOK）
        mAcc = std::fmod(mAcc, interval);
        Capture();
    }
}

//==============================================================================
// Capture
//==============================================================================
void SceneCaptureComponent::Capture()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    if (mCaptureMode == CaptureMode::Fixed)
    {
        BuildFixedView();
    }
    else
    {
        BuildMirrorView();
    }
    
    // Renderer にリクエスト
    SceneCaptureRequest req;
    req.rt     = mRT;
    req.view   = mView;
    req.proj   = mProj;
    req.drawUI = mDesc.drawUI;

    renderer->RequestSceneCapture(req);
}

//==============================================================================
// BuildFixedView
//==============================================================================
void SceneCaptureComponent::BuildFixedView()
{
    GetOwner()->ComputeWorldTransform();
    const Matrix4 world = GetOwner()->GetWorldTransform();

    // ToyLib軸：前方=+Z, 上=+Y（視線は -Z を使う運用）
    Vector3 camPos = world.GetTranslation();
    const Vector3 camFwd = world.GetZAxis();
    const Vector3 camUp  = world.GetYAxis();

    const Vector3 target = camPos - camFwd * 100.0f;

    mView = Matrix4::CreateLookAt(camPos, target, camUp);

    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mDesc.fov),
        mDesc.width,
        mDesc.height,
        0.1f,
        1000.0f
    );
}

//==============================================================================
// BuildMirrorView
//==============================================================================
void SceneCaptureComponent::BuildMirrorView()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }
    
    GetOwner()->ComputeWorldTransform();

    const Matrix4 mainInvView = renderer->GetInvViewMatrix();
    const Matrix4 mirrorW     = GetOwner()->GetWorldTransform();

    const Vector3 mainPos   = mainInvView.GetTranslation();
    const Vector3 mirrorPos = mirrorW.GetTranslation();

    // 鏡面法線（板ポリ表が +Z なら ZAxis が法線）
    Vector3 N = mirrorW.GetZAxis();
    N.Normalize();

    // 反射位置
    const float d = Vector3::Dot(mainPos - mirrorPos, N);
    const Vector3 reflPos = mainPos - 2.0f * d * N;

    // up は鏡のY軸を採用
    Vector3 up = mirrorW.GetYAxis();
    up.Normalize();

    // 鏡中心へ LookAt（「鏡に映る」カメラ）
    mView = Matrix4::CreateLookAt(reflPos, mirrorPos, up);

    // distance / near
    const float distance = (mirrorPos - reflPos).Length();

    float nearZ = distance * 0.95f;
    nearZ = (nearZ > 0.01f) ? nearZ : 0.01f;

    // 鏡サイズ（まずは高さ基準で FOV を決める）
    const float actorScale = GetOwner()->GetScale(); // ToyLib が uniform scale 前提の想定
    const float mirrorHsize =
        mirrorW.GetYAxis().Length() * mSurfaceInfo.scHeight * actorScale;

    const float sizeY = (mirrorHsize > 1e-4f) ? mirrorHsize : 1e-4f;
    const float distSafe = (distance > 1e-4f) ? distance : 1e-4f;

    // fov = 2*atan(Size/(2*distance))
    const float fovRad = 2.0f * std::atan(sizeY / (2.0f * distSafe));
    const float fovDeg = Math::ToDegrees(fovRad);

    const float farZ = 1000.0f;

    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(fovDeg),
        mDesc.width,
        mDesc.height,
        nearZ,
        farZ
    );
}

} // namespace toy
