#include "Graphics/Sprite/SpriteComponent.h"
#include "Asset/Material/Texture.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Core/Actor.h"
#include "glad/glad.h"

namespace toy {

//==================================================
// SpriteComponent
//   2D スプライト描画コンポーネント
//   ・基本は UI / HUD 用（左上原点論理座標）
//   ・mIsTopLeft=false にすると、中心原点の2Dスプライトとしても使える
//==================================================

SpriteComponent::SpriteComponent(Actor* a, int drawOrder, VisualLayer layer)
: VisualComponent(a, drawOrder, layer)
, mScaleWidth(1.0f)
, mScaleHeight(1.0f)
, mTexWidth(0)
, mTexHeight(0)
, mIsTopLeft(true)
{
    mDrawOrder = drawOrder;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // スプライト用シェーダー
    mShader = renderer->GetShader("Sprite");

    // 起動時の画面サイズ（必要に応じて再取得してもよい）
    mScreenWidth  = renderer->GetScreenWidth();
    mScreenHeight = renderer->GetScreenHeight();
}

SpriteComponent::~SpriteComponent() = default;

//--------------------------------------------------
// テクスチャ設定
// ついでにピクセル幅/高さもキャッシュしておく
//--------------------------------------------------
void SpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    VisualComponent::SetTexture(tex);

    if (tex)
    {
        mTexWidth  = tex->GetWidth();
        mTexHeight = tex->GetHeight();
    }
    else
    {
        mTexWidth  = 0;
        mTexHeight = 0;
    }
}

//--------------------------------------------------
// 描画
//  ・Sprite は 2D なので深度テストを無効化
//  ・Virtual 解像度 → 実解像度へのスケールで等倍表示を調整
//--------------------------------------------------
void SpriteComponent::Draw()
{
    if (!mIsVisible || mTexture == nullptr)
    {
        return;
    }

    //==============================
    // ブレンド/深度設定
    //==============================
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(
        mIsBlendAdd ? GL_ONE : GL_SRC_ALPHA,
        mIsBlendAdd ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA
    );
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    //==============================
    // 画面サイズと Virtual 解像度
    //==============================
    // 物理解像度
    float sw = renderer->GetScreenWidth();
    float sh = renderer->GetScreenHeight();

    // 論理解像度（ゲーム内の想定座標系）
    float vw = renderer->GetVirtualWidth();
    float vh = renderer->GetVirtualHeight();

    // Virtual が未設定なら物理と同一扱い
    if (vw <= 0.0f) vw = sw;
    if (vh <= 0.0f) vh = sh;

    // 論理→物理は「小さい方」に合わせてアスペクト比を維持
    float sx    = sw / vw;
    float sy    = sh / vh;
    float scale = (sx < sy) ? sx : sy;

    //==============================
    // テクスチャサイズと表示サイズ
    //==============================
    float texW   = static_cast<float>(mTexWidth);
    float texH   = static_cast<float>(mTexHeight);
    float width  = texW * mScaleWidth  * scale;
    float height = texH * mScaleHeight * scale;

    //==============================
    // 描画位置の決定
    //
    //  mIsTopLeft == true
    //    Actor.Position を「左上原点の論理座標(ピクセル相当)」として扱う。
    //    UI レイアウト用のモード。
    //
    //  mIsTopLeft == false
    //    Actor.Position を「中心原点前提の座標」として扱う
    //    従来のスプライト/2Dオブジェクト的な使い方。
    //==============================
    Vector3 pos;

    if (mIsTopLeft)
    {
        // 論理座標（左上原点 / 右+ / 下+）
        Vector3 logicalPos = GetOwner()->GetPosition();

        // 論理領域の物理サイズ（scale を掛けた表示範囲）
        float scaledVW = vw * scale;
        float scaledVH = vh * scale;

        // 論理領域の「左上」が、OpenGL の中心原点から見てどこか
        float originX = -scaledVW * 0.5f; // 左端（中心からマイナス）
        float originY =  scaledVH * 0.5f; // 上端（中心からプラス）

        // スプライトの「左上」のワールド座標 T
        float topLeftX = originX + logicalPos.x * scale;
        float topLeftY = originY - logicalPos.y * scale; // 下方向が＋なので反転

        // 板ポリのローカル原点は「中心」なので、
        // 左上Tから (width/2, height/2) だけ中心側にずらして最終位置にする。
        // ※Yは「上が＋」なので height/2 分だけマイナス方向。
        pos.x = topLeftX + width  * 0.5f;
        pos.y = topLeftY - height * 0.5f;
        pos.z = logicalPos.z; // UI用途なら 0.0f 固定でもよい
    }
    else
    {
        // 従来の「中心原点」的な座標として扱う
        pos = GetOwner()->GetPosition();
        pos.x *= scale;
        pos.y *= scale;
        // pos.z はそのまま（2D UIなら 0.0f でもよい）
    }

    //==============================
    // ワールド・ビュー射影行列
    //==============================
    Matrix4 world = Matrix4::CreateScale(width, height, 1.0f);
    world *= Matrix4::CreateTranslation(pos);

    // シンプルな 2D 用 ViewProj（中心原点 / 右+ / 上+）
    Matrix4 viewProj = Matrix4::CreateSimpleViewProj(sw, sh);

    //==============================
    // シェーダー・テクスチャ設定
    //==============================
    mShader->SetActive();
    mShader->SetMatrixUniform("uViewProj",       viewProj);
    mShader->SetMatrixUniform("uWorldTransform", world);

    mTexture->SetActive(0);
    mShader->SetTextureUniform("uTexture", 0);

    // LightingManager が Sprite 用にも何かしら設定している前提
    Matrix4 view = renderer->GetViewMatrix();
    mLightingManager->ApplyToShader(mShader, view);

    //==============================
    // 描画
    //==============================
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    //==============================
    // ステートを戻す
    //==============================
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

} // namespace toy
