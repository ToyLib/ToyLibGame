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
    // DrawPass移行のためリターン
    return;
    
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
    // 画面サイズと Virtual 解像度（Renderer に計算させる）
    //==============================
    UIScaleInfo ui = renderer->GetUIScaleInfo();

    float sw    = ui.screenW;    // 物理解像度（ピクセル）
    float sh    = ui.screenH;
    //float vw    = ui.virtualW;   // 論理解像度
    //float vh    = ui.virtualH;
    float scale = ui.scale;      // 論理→物理の共通スケール

    //==============================
    // テクスチャサイズと表示サイズ（ピクセルベース）
    //==============================
    float texW   = static_cast<float>(mTexWidth);
    float texH   = static_cast<float>(mTexHeight);
    float width  = texW * mScaleWidth  * scale;
    float height = texH * mScaleHeight * scale;

    //==============================
    // 描画位置の決定
    //==============================
    Vector3 pos;

    if (mIsTopLeft)
    {
        // 論理座標（左上原点 / 右+ / 下+）
        Vector3 logicalPos = GetOwner()->GetPosition() + mOffset;

        // -----------------------------
        // 1. 論理座標 → 画面ピクセル (左上原点)
        // -----------------------------
        // 論理 (0,0) が「黒帯込みの画面左上」に来るようにする
        float px = ui.offsetX + logicalPos.x * scale;
        float py = ui.offsetY + logicalPos.y * scale;

        // Sprite のローカル原点は「中心」なので、
        // 論理位置は「左上」とみなして中心にずらす
        float cx = px + width  * 0.5f;
        float cy = py + height * 0.5f;

        // -----------------------------
        // 2. 画面ピクセル → ワールド座標（中心原点 / 右+ / 上+）
        // -----------------------------
        // SimpleViewProj(sw, sh) が
        //   X: [-sw/2, +sw/2]
        //   Y: [-sh/2, +sh/2] (上が＋)
        // を前提とした行列だとすると：
        pos.x = cx - sw * 0.5f;
        pos.y = sh * 0.5f - cy;   // 画面上が＋になるよう反転
        pos.z = logicalPos.z;     // UI用途なら 0 でもOK
    }
    else
    {
        // 論理座標（左上原点 / 右+ / 下+）
        Vector3 logicalPos = GetOwner()->GetPosition() + mOffset;

        // -----------------------------
        // 1. 論理座標 → 画面ピクセル (左上原点)
        // -----------------------------
        // 論理 (0,0) が「黒帯込みの画面左上」に来るようにする
        float px = ui.offsetX + logicalPos.x * scale;
        float py = ui.offsetY + logicalPos.y * scale;

        // -----------------------------
        // 2. 画面ピクセル → ワールド座標（中心原点 / 右+ / 上+）
        // -----------------------------
        // SimpleViewProj(sw, sh) が
        //   X: [-sw/2, +sw/2]
        //   Y: [-sh/2, +sh/2] (上が＋)
        // を前提とした行列だとすると：
        pos.x = px - sw * 0.5f;
        pos.y = sh * 0.5f - py;   // 画面上が＋になるよう反転
        pos.z = logicalPos.z;     // UI用途なら 0 でもOK
        /*
        // 従来の「中心原点」座標（レターボックス無視で中央基準）
        pos = GetOwner()->GetPosition();
        pos.x *= scale;
        pos.y *= scale;
        // pos.z はそのまま
         */
    }

    //==============================
    // ワールド・ビュー射影行列
    //==============================
    Matrix4 world = Matrix4::CreateScale(width, height, 1.0f);
    world *= Matrix4::CreateTranslation(pos);

    // 2D 用の ViewProj（中心原点 / 右+ / 上+）
    Matrix4 viewProj = Matrix4::CreateSimpleViewProj(sw, sh);

    //==============================
    // シェーダー・テクスチャ設定
    //==============================
    mShader->SetActive();
    mShader->SetMatrixUniform("uViewProj",       viewProj);
    mShader->SetMatrixUniform("uWorldTransform", world);
    mShader->SetVectorUniform("uSpriteColor",    mColor);
    mShader->SetFloatUniform("uSpriteAlpha",     mAlpha);

    mTexture->SetActive(0);
    mShader->SetTextureUniform("uTexture", 0);

    Matrix4 view = renderer->GetViewMatrix();
    mLightingManager->ApplyToShader(mShader, view);

    //==============================
    // 描画
    //==============================
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    renderer->AddDrawCall();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    renderer->AddDrawObject();
}


#include "Engine/Render/RenderItem.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"


void SpriteComponent::GatherRenderItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mTexture) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // UIスケール
    UIScaleInfo ui = renderer->GetUIScaleInfo();
    const float sw    = ui.screenW;
    const float sh    = ui.screenH;
    const float scale = ui.scale;

    // 表示サイズ（ピクセル）
    const float texW   = static_cast<float>(mTexWidth);
    const float texH   = static_cast<float>(mTexHeight);
    const float width  = texW * mScaleWidth  * scale;
    const float height = texH * mScaleHeight * scale;

    // 位置（あなたの既存ロジックそのまま）
    Vector3 pos;
    {
        Vector3 logicalPos = GetOwner()->GetPosition() + mOffset;

        const float px = ui.offsetX + logicalPos.x * scale;
        const float py = ui.offsetY + logicalPos.y * scale;

        if (mIsTopLeft)
        {
            const float cx = px + width  * 0.5f;
            const float cy = py + height * 0.5f;
            pos.x = cx - sw * 0.5f;
            pos.y = sh * 0.5f - cy;
            pos.z = logicalPos.z;
        }
        else
        {
            pos.x = px - sw * 0.5f;
            pos.y = sh * 0.5f - py;
            pos.z = logicalPos.z;
        }
    }

    // 行列
    Matrix4 world = Matrix4::CreateScale(width, height, 1.0f);
    world *= Matrix4::CreateTranslation(pos);

    Matrix4 viewProj = Matrix4::CreateSimpleViewProj(sw, sh);

    // RenderItem 詰める
    RenderItem it;
    it.type  = RenderItemType::Sprite;
    it.pass  = RenderPass::UI;          // ★UIパスに寄せるなら
    it.layer = GetLayer();

    it.cull      = CullMode::None;      // ★UIはカリング不要
    it.frontFace = FrontFace::CCW;      // ★任意
    it.blend     = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;
    it.depthTest  = false;
    it.depthWrite = false;
    it.pass      = RenderPass::Main;       // UIだけならUIパスにしてもOK
    it.layer     = GetLayer();             // UI/Overlayなど
    it.drawOrder = GetDrawOrder();

    it.topology  = PrimitiveTopology::Triangles;
    it.geometry  = renderer->GetSpriteQuadHandle(); // ★Rendererが返す共通ジオメトリ
    it.indexCount = 6;

    it.blend     = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;
    it.depthTest  = false;
    it.depthWrite = false;

    it.shader    = renderer->GetShaderHandle("Sprite"); // ★暫定でOK（pass選択にしてもよい）

    it.world     = world;
    it.viewProj  = viewProj;

    it.texture   = renderer->ToHandle(mTexture); // or mTexture->GetHandle() 的なもの
    it.textureUnit = 0;

    it.color     = mColor;
    it.alpha     = mAlpha;

    out.Push(it);
}
} // namespace toy

