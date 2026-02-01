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
    else if (mCaptureMode == CaptureMode::Mirror)
    {
        BuildMirrorView();
    }
    else
    {
        BuildWaterView();
    }
    
    // Renderer にリクエスト
    SceneCaptureRequest req;
    req.rt     = mRT;
    req.view   = mView;
    req.proj   = mProj;
    req.drawUI = mDesc.drawUI;
    if (mCaptureMode == CaptureMode::Water)
    {
        req.drawSky = true;
        req.drawUI = false;
        req.drawWorld = false;
        req.drawOverlay = false;
    }

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

    //========================================================
    // (A) ここは元の挙動に戻す：鏡中心を見る
    //     → FOV計算が自然にハマる
    //========================================================
    Vector3 viewDir = mirrorPos - reflPos;
    const float vlen = viewDir.Length();
    if (vlen < 1e-5f)
    {
        // 万一同一点なら固定値で
        viewDir = Vector3(0.0f, 0.0f, -1.0f);
    }
    else
    {
        viewDir = viewDir * (1.0f / vlen); // /=なし
    }

    //========================================================
    // (B) Up を「壊れないように」自動選択
    //     - まずは従来：鏡のY軸
    //     - 平行に近ければ WorldUp
    //     - それもダメなら 鏡のX軸（最後の保険）
    //========================================================
    auto MakeSafeUp = [](const Vector3& desiredUp, const Vector3& fwdUnit) -> Vector3
    {
        Vector3 up = desiredUp - Vector3::Dot(desiredUp, fwdUnit) * fwdUnit; // 直交化
        float len = up.Length();
        if (len < 1e-4f)
        {
            return Vector3::UnitY; // さらに外側で差し替える
        }
        up = up * (1.0f / len);
        return up;
    };

    Vector3 upCand = mirrorW.GetYAxis();
    upCand.Normalize();

    Vector3 up = MakeSafeUp(upCand, viewDir);

    // desiredUp が forward と平行すぎると MakeSafeUp が UnitY を返す可能性があるので
    // ここで候補を切り替える
    const float dot0 = std::fabs(Vector3::Dot(up, viewDir));
    if (dot0 > 0.999f)
    {
        // WorldUp を試す
        up = MakeSafeUp(Vector3::UnitY, viewDir);
        const float dot1 = std::fabs(Vector3::Dot(up, viewDir));
        if (dot1 > 0.999f)
        {
            // 最後に鏡X軸
            Vector3 xCand = mirrorW.GetXAxis();
            xCand.Normalize();
            up = MakeSafeUp(xCand, viewDir);
        }
    }

    // 鏡中心へ LookAt（元の「自然に反射する」挙動）
    mView = Matrix4::CreateLookAt(reflPos, mirrorPos, up);

    //========================================================
    // Proj：元の計算をそのまま
    //========================================================
    const float distance = (mirrorPos - reflPos).Length();

    float nearZ = distance * 0.95f;
    nearZ = (nearZ > 0.01f) ? nearZ : 0.01f;

    const float actorScale = GetOwner()->GetScale(); // uniform scale 想定
    const float mirrorHsize =
        mirrorW.GetYAxis().Length() * mSurfaceInfo.scHeight * actorScale;

    const float sizeY    = (mirrorHsize > 1e-4f) ? mirrorHsize : 1e-4f;
    const float distSafe = (distance    > 1e-4f) ? distance    : 1e-4f;

    const float fovRad = 2.0f * std::atan(sizeY / (2.0f * distSafe));
    const float fovDeg = Math::ToDegrees(fovRad);

    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(fovDeg),
        mDesc.width,
        mDesc.height,
        nearZ,
        1000.0f
    );
}

//==============================================================================
// BuildWaterView
//==============================================================================
void SceneCaptureComponent::BuildWaterView()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    //============================================================
    // メインカメラ情報取得
    //============================================================
    const Matrix4 mainInvView = renderer->GetInvViewMatrix();
    const Matrix4 mainView    = renderer->GetViewMatrix();

    const Vector3 mainPos = mainInvView.GetTranslation();

    // メインカメラの forward / up（ToyLib: 前方 +Z、視線は -Z 運用）
    Vector3 mainFwd = mainView.GetZAxis() * -1.0f;
    Vector3 mainUp  = mainView.GetYAxis();

    // 正規化
    float len = mainFwd.Length();
    if (len > 1e-6f) mainFwd = mainFwd * (1.0f / len);
    else             mainFwd = Vector3(0, 0, -1);

    len = mainUp.Length();
    if (len > 1e-6f) mainUp = mainUp * (1.0f / len);
    else             mainUp = Vector3::UnitY;

    //============================================================
    // 水面法線（板ポリの +Z を表とする）
    //============================================================
    GetOwner()->ComputeWorldTransform();
    const Matrix4 waterW = GetOwner()->GetWorldTransform();

    Vector3 N = waterW.GetZAxis();
    len = N.Length();
    if (len > 1e-6f) N = N * (1.0f / len);
    else             N = Vector3::UnitY;

    //============================================================
    // 視線と Up を反射
    //  R = V - 2 * dot(V, N) * N
    //============================================================
    Vector3 reflFwd = mainFwd - 2.0f * Vector3::Dot(mainFwd, N) * N;
    Vector3 reflUp  = mainUp  - 2.0f * Vector3::Dot(mainUp,  N) * N;

    // 正規化
    len = reflFwd.Length();
    if (len > 1e-6f) reflFwd = reflFwd * (1.0f / len);
    else             reflFwd = Vector3(0, 0, -1);

    // Up が Forward と平行になる事故防止
    reflUp = reflUp - Vector3::Dot(reflUp, reflFwd) * reflFwd;
    len = reflUp.Length();
    if (len > 1e-6f)
    {
        reflUp = reflUp * (1.0f / len);
    }
    else
    {
        reflUp = Vector3::UnitY;
    }

    //============================================================
    // View 行列
    // 位置は「メインカメラ位置のまま」
    //============================================================
    const Vector3 target = mainPos + reflFwd * 100.0f;

    mView = Matrix4::CreateLookAt(
        mainPos,
        target,
        reflUp
    );

    //============================================================
    // Projection（通常カメラと同等でOK）
    //============================================================
    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mDesc.fov),
        mDesc.width,
        mDesc.height,
        0.1f,
        1000.0f
    );
}
} // namespace toy
