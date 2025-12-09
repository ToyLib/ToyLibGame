#include "Environment/WeatherOverlayComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Shader.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Render/Renderer.h"
#include "Utils/MathUtil.h"

namespace toy {

WeatherOverlayComponent::WeatherOverlayComponent(Actor* a, int drawOrder, VisualLayer layer)
: VisualComponent(a, drawOrder, layer)
, mRainAmount(0.0f)
, mFogAmount(0.0f)
, mSnowAmount(0.0f)
{
    //------ 必要リソースを取得 ------
    auto renderer   = GetOwner()->GetApp()->GetRenderer();
    mShader         = renderer->GetShader("WeatherOverlay");  // 雨/霧/雪 用シェーダ
    mVertexArray    = renderer->GetFullScreenQuad();          // フルスクリーン四角形
    mScreenWidth    = renderer->GetScreenWidth();
    mScreenHeight   = renderer->GetScreenHeight();
}

void WeatherOverlayComponent::Draw()
{
    if (!mShader || !mVertexArray) return;

    //======================================================================
    // フルスクリーンオーバーレイ描画のための典型的な OpenGL 設定
    // ・深度テスト無効（画面全体に描く）
    // ・深度書き込み無効
    // ・アルファブレンド有効（霧や雨粒を透明合成する）
    //======================================================================
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //------ シェーダー有効化 ------
    mShader->SetActive();

    //------ 天候の強さ（WeatherManager から設定される値） ------
    mShader->SetFloatUniform("uTime",        SDL_GetTicks() / 1000.0f);
    mShader->SetFloatUniform("uRainAmount",  mRainAmount);   // 雨（0〜1）
    mShader->SetFloatUniform("uFogAmount",   mFogAmount);    // 霧（0〜1）
    mShader->SetFloatUniform("uSnowAmount",  mSnowAmount);   // 雪（0〜1）

    //------ 画面解像度（スクリーンスペースエフェクト用） ------
    mShader->SetVector2Uniform("uResolution",
                               Vector2(mScreenWidth, mScreenHeight));

    //------ フルスクリーン四角形を描画 ------
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES,
                   mVertexArray->GetNumIndices(),
                   GL_UNSIGNED_INT,
                   nullptr);

    //------ OpenGL ステート復帰 ------
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace toy
