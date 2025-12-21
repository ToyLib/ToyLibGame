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

    if (mDesc.captureMode == CaptureMode::Fixed)
    {
        BuildMirrorView();
    }
    //----------------------------------------------------------------------
    // Projection
    //----------------------------------------------------------------------
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

void SceneCaptureComponent::BuildFixedView()
{
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


}
//------------------------------------------------------------------------------
// SceneCaptureComponent::BuildMirrorView
// ・メインカメラを鏡平面で反射した View 行列を mView に設定する
// ・「鏡に映る世界」を作るための仮想カメラ
//------------------------------------------------------------------------------
void SceneCaptureComponent::BuildMirrorView()
{
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    auto mainInvView = renderer->GetInvViewMatrix();
    auto mirrorWorld = GetOwner()->GetWorldTransform();
    
    // ------------------------------------------------------------
    // 1) メインカメラ（ワールド）の基底を取り出す
    //    ToyLib: viewは -Z を前として使ってるので forward = -ZAxis
    // ------------------------------------------------------------
    const Vector3 mainPos = mainInvView.GetTranslation();
    const Vector3 mainFwd = mainInvView.GetZAxis(); // カメラ前方
    const Vector3 mainUp  = mainInvView.GetYAxis();

    // ------------------------------------------------------------
    // 2) 鏡平面（ワールド）
    //    鏡面の中心 = mirrorWorld の平行移動
    //    法線 N は「鏡の表（反射面）」方向にしたい
    //    ※ここが逆だと “向きが違う” になりやすい
    // ------------------------------------------------------------
    const Vector3 mirrorPos = mirrorWorld.GetTranslation();

    // 鏡の“表”の法線をまず仮定（あなたの板ポリの前が +Z ならこれ）
    Vector3 N = mirrorWorld.GetZAxis(); // まず +Z を法線とみなす
    N.Normalize();

    // ★重要：N が必ず “メインカメラ側” を向くように反転チェック
    // カメラが鏡の表側にいるなら dot(mainPos - mirrorPos, N) > 0 になるのが自然
    if (Vector3::Dot(mainPos - mirrorPos, N) < 0.0f)
    {
        N = -1.0f * N;
    }

    // ------------------------------------------------------------
    // 3) 平面反射（位置）
    //    P' = P - 2 * dot(P - mirrorPos, N) * N
    // ------------------------------------------------------------
    const float dist = Vector3::Dot(mainPos - mirrorPos, N);
    const Vector3 reflPos = mainPos - 2.0f * dist * N;

    // ------------------------------------------------------------
    // 4) 平面反射（向き）
    //    v' = v - 2 * dot(v, N) * N
    // ------------------------------------------------------------
    auto ReflectVec = [](const Vector3& v, const Vector3& n) -> Vector3
    {
        return v - 2.0f * Vector3::Dot(v, n) * n;
    };

    Vector3 reflFwd = ReflectVec(mainFwd, N);
    Vector3 reflUp  = ReflectVec(mainUp,  N);

    reflFwd.Normalize();
    reflUp.Normalize();

    // ------------------------------------------------------------
    // 5) LookAt（反射カメラ）
    // ------------------------------------------------------------
    const Vector3 target = reflPos + reflFwd * 100.0f;
    mView =  Matrix4::CreateLookAt(reflPos, target, reflUp);


}
} // namespace toy
