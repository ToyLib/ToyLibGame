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
    , mCaptureMode(CaptureMode::Fixed)
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
//    if (mAcc >= interval)
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

    if (mCaptureMode == CaptureMode::Fixed)
    {
        BuildFixedView();
    }
    else
    {
        BuildMirrorView();
    }


    
    
    //----------------------------------------------------------------------
    // Renderへリクエスト
    //----------------------------------------------------------------------
    SceneCaptureRequest req;
    req.rt     = mRT;
    req.view   = mView;
    req.proj   = mProj;
    req.drawUI = mDesc.drawUI;


    renderer->RequestSceneCapture(req);
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
    const Vector3 camFwd = world.GetZAxis();
    const Vector3 camUp  = world.GetYAxis();

//    camPos = camPos - camFwd * 10.0f;
    const Vector3 target = camPos - camFwd * 100.0f;

    mView = Matrix4::CreateLookAt(camPos, target, camUp);
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

}
//------------------------------------------------------------------------------
// SceneCaptureComponent::BuildMirrorView
// ・メインカメラを鏡平面で反射した View 行列を mView に設定する
// ・「鏡に映る世界」を作るための仮想カメラ
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// SceneCaptureComponent::BuildMirrorView
// Qiita 記事の式ベースで「鏡に映る世界」の View を作る
// ・mView を更新する（Projection/FOV/near は Capture() 側で反映）
//------------------------------------------------------------------------------
void SceneCaptureComponent::BuildMirrorView()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer) return;

    //========================
    // 0) 取得
    //========================
    const Matrix4 mainInvView = renderer->GetInvViewMatrix();
    const Matrix4 mirrorW     = GetOwner()->GetWorldTransform();

    const Vector3 mainPos   = mainInvView.GetTranslation();  // メインカメラ位置（ワールド）
    const Vector3 mirrorPos = mirrorW.GetTranslation();      // 鏡中心（ワールド）

    //========================
    // 1) 鏡面法線 N（ワールド）
    //========================
    // 板ポリの「表」が +Z なら、ワールド法線は +ZAxis。
    // （Actor を X軸90度回して水面にしても、ワールドZ軸は回転後の法線になる）
    Vector3 N = mirrorW.GetZAxis();
    N.Normalize();

    //========================
    // 2) 反射カメラ位置（面対称）
    //   P' = P - 2 * dot(P - mirrorPos, N) * N
    //========================
    const float d = Vector3::Dot(mainPos - mirrorPos, N);
    const Vector3 reflPos = mainPos - 2.0f * d * N;

    //========================
    // 3) 反射カメラの向き：鏡中心へ LookAt
    //========================
    // Up は「鏡の上方向」を使うのが安定（記事は LookAt だけ）
    Vector3 up = mirrorW.GetYAxis();
    up.Normalize();

    mView = Matrix4::CreateLookAt(reflPos, mirrorPos, up);

    //========================
    // 4) distance（記事の distance）
    //   distance = |mirrorPos - reflPos|
    //========================
    const float distance = (mirrorPos - reflPos).Length();

    //========================
    // 5) near（記事の nearClipPlane = distance * 0.9）
    //========================
    float nearZ = distance * 0.95f;
    nearZ = (nearZ > 0.01f) ? nearZ : 0.01f;

    //========================
    // 6) 鏡面サイズ Size（記事の Size）
    //========================
    // ToyLibのQuadはローカル [-0.5..0.5] の 1x1。
    // WorldTransform の X/Y 軸ベクトル長が、そのまま「ワールド上の幅/高さ」になってる想定。
    float actorScale = GetOwner()->GetScale();
    //const float mirrorWsize = mirrorW.GetXAxis().Length() * mSurfaceInfo.scWidth * actorScale; // 幅（ワールド）
    const float mirrorHsize = mirrorW.GetYAxis().Length() * mSurfaceInfo.scHeight * actorScale;; // 高さ（ワールド）

    // 記事は 1つの Size を使ってたので、まずは「高さ」を基準に縦FOVを作るのが自然
    // （横FOVは aspect で決まる）
    const float sizeY = (mirrorHsize > 1e-4f) ? mirrorHsize : 1e-4f;

    //========================
    // 7) FOV（記事の式）
    //   fov = 2 * atan( Size / (2*distance) )
    //========================
    float fovRad = 2.0f * std::atan( sizeY / (2.0f * (distance > 1e-4f ? distance : 1e-4f)) );
    float fovDeg = Math::ToDegrees(fovRad);

    //========================
    // 8) Projection（ここで作る）
    //========================
    // 既存の mDesc.width/height を使って aspect を合わせる
    const float farZ = 1000.0f; // 必要なら desc に持たせてOK
    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(fovDeg),
        mDesc.width,
        mDesc.height,
        nearZ,
        farZ
    );
}

} // namespace toy
