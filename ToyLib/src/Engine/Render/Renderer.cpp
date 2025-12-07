#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Graphics/Sprite/SpriteComponent.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Mesh.h"
#include "Graphics/Mesh/MeshComponent.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Graphics/Effect/ParticleComponent.h"
#include "Graphics/Sprite/BillboardComponent.h"
#include "Graphics/VisualComponent.h"
#include "Environment/SkyDomeComponent.h"
#include "Asset/Font/TextFont.h"
#include "Utils/FrustumUtil.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"
#include "glad/glad.h"

#include <algorithm>
#include <string>
#include <iostream>

namespace toy {

//=============================================================
// コンストラクタ／デストラクタ
//=============================================================

// コンストラクタ
Renderer::Renderer()
: mScreenWidth(0.f)
, mScreenHeight(0.f)
, mVirtualWidth(0.f)
, mVirtualHeight(0.f)
, mPerspectiveFOV(45.f)
, mIsDebugMode(false)
, mClearColor(Vector3(0.2f, 0.5f, 0.8f))
, mWireColor(Vector3(1.f, 1.f, 1.f))
, mShadowNear(10.f)
, mShadowFar(100)
, mShadowOrthoWidth(100.f)
, mShadowOrthoHeight(100.f)
, mShadowFBOWidth(4096)
, mShadowFBOHeight(4096)
, mWindow(nullptr)
, mGLContext(nullptr)
, mShaderPath("ToyLib/Shaders/")
, mCntDrawObject(0)
, mSkyDomeComp(nullptr)
, mLightSpaceMatrix(Matrix4::Identity)
, mWindowDisplayScale(1.0f)
{
    // ライティング管理クラス
    mLightingManager = std::make_shared<LightingManager>();

    // Renderer の初期設定（タイトルや解像度など）を外部ファイルから読み込む
    // 例: ToyLib/Settings/Renderer_Settings.json
    LoadSettings("ToyLib/Settings/Renderer_Settings.json");
}

// デストラクタ
Renderer::~Renderer()
{
    // 実処理は Shutdown() 側で行う前提
}


//=============================================================
// 初期化／終了処理
//=============================================================

bool Renderer::Initialize(SDL_Window* window)
{
    // SDLウィンドウを保持
    mWindow = window;

    //---------------------------------------------------------
    // OpenGL コンテキスト属性設定
    //---------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    //---------------------------------------------------------
    // OpenGL コンテキスト生成
    //---------------------------------------------------------
    mGLContext = SDL_GL_CreateContext(mWindow);
    if (!mGLContext)
    {
        std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
        return false;
    }

    //---------------------------------------------------------
    // 垂直同期（VSync）
    //---------------------------------------------------------
    SDL_GL_SetSwapInterval(1);

    //---------------------------------------------------------
    // ウィンドウの「実ピクセルサイズ」を取得（HiDPI 対応）
    //---------------------------------------------------------
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);

    // 描画に使うスクリーンサイズは「実ピクセル」で管理
    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    //---------------------------------------------------------
    // このウィンドウに対する DPI スケール
    //---------------------------------------------------------
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    //---------------------------------------------------------
    // GLAD 初期化
    //---------------------------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return false;
    }

    //---------------------------------------------------------
    // シェーダーのロード
    //---------------------------------------------------------
    if (!LoadShaders())
    {
        return false;
    }

    //---------------------------------------------------------
    // 各種描画用 VAO 準備
    //---------------------------------------------------------
    CreateSpriteVerts();
    CreateFullScreenQuad();

    //---------------------------------------------------------
    // シャドウマッピング初期化
    //---------------------------------------------------------
    if (!InitializeShadowMapping())
    {
        return false;
    }

    //---------------------------------------------------------
    // クリアカラーの初期設定
    //---------------------------------------------------------
    SetClearColor(mClearColor);

    mSkyDomeComp   = nullptr;
    mCntDrawObject = 0;

    //---------------------------------------------------------
    // ビューポート＆射影行列など、サイズ依存の状態をまとめて更新
    //---------------------------------------------------------
    OnWindowResized(pixelW, pixelH);

    std::cout << "[Renderer] GL Init Complete. "
              << "Pixels("  << pixelW   << "x" << pixelH   << ") "
              << "Scale="   << mWindowDisplayScale
              << std::endl;

    return true;
}
// リリース処理
void Renderer::Shutdown()
{
    if (mShadowFBO)
    {
        glDeleteFramebuffers(1, &mShadowFBO);
        mShadowFBO = 0;
    }
    if (mGLContext)
    {
        SDL_GL_DestroyContext(mGLContext);
        mGLContext = nullptr;
    }
}


//=============================================================
// メイン描画パス
//=============================================================

void Renderer::Draw()
{
    // カラーバッファ／デプスバッファ初期化
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 1) ライト視点でのシャドウマップ描画
    RenderShadowMap();
    
    // 2) 通常描画パス
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // スカイドーム（背景）
    DrawSky();

    // レイヤー別描画（奥から順に）
    DrawVisualLayer(VisualLayer::Background2D);
    DrawVisualLayer(VisualLayer::Object3D);
    DrawVisualLayer(VisualLayer::Effect3D);
    DrawVisualLayer(VisualLayer::OverlayScreen);
    DrawVisualLayer(VisualLayer::UI);
    
    // Debug 用カウンタリセット
    // std::cout << "Render 3D Objects Count = " << mCntDrawObject << std::endl;
    mCntDrawObject = 0;
    
    // バッファ入れ替え
    SDL_GL_SwapWindow(mWindow);
}

// スカイドーム描画
void Renderer::DrawSky()
{
    if (!mSkyDomeComp)
        return;

    mSkyDomeComp->Draw();
}


//=============================================================
// VisualComponent 管理
//=============================================================

void Renderer::AddVisualComp(VisualComponent* comp)
{
    // DrawOrder 昇順で挿入
    auto iter = mVisualComps.begin();
    for (; iter != mVisualComps.end(); ++iter)
    {
        if (comp->GetDrawOrder() < (*iter)->GetDrawOrder())
            break;
    }
    mVisualComps.insert(iter, comp);
}

void Renderer::RemoveVisualComp(VisualComponent* comp)
{
    auto iter = std::find(mVisualComps.begin(), mVisualComps.end(), comp);
    if (iter != mVisualComps.end())
        mVisualComps.erase(iter);
}


//=============================================================
// レイヤー描画＆フラスタムカリング
//=============================================================

void Renderer::DrawVisualLayer(VisualLayer layer)
{
    bool is3DLayer =
        (layer == VisualLayer::Object3D ||
         layer == VisualLayer::Effect3D);
    
    Frustum frustum;
    if (is3DLayer)
    {
        // View * Projection からフラスタムを生成
        Matrix4 vp = mViewMatrix * mProjectionMatrix;
        frustum = BuildFrustumFromMatrix(vp);
    }
    
    //---------------------------------------------------------
    // レイヤーごとのデプス設定
    //---------------------------------------------------------
    if (layer == VisualLayer::UI || layer == VisualLayer::Background2D)
    {
        // 2D/UI → Zテスト不要、書き込み不要
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    else if (layer == VisualLayer::Effect3D)
    {
        // 3Dエフェクト → Zテストあり／書き込みなし（パーティクルなど）
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    else
    {
        // 通常3D描画
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    
    //---------------------------------------------------------
    // コンポーネント描画ループ
    //---------------------------------------------------------
    for (auto& comp : mVisualComps)
    {
        if (!comp->IsVisible() || comp->GetLayer() != layer)
            continue;
        
        // 3Dレイヤーのみフラスタムカリング
        if (is3DLayer)
        {
            // Actor の BoundingVolumeComponent から AABB を取得
            Actor* owner = comp->GetOwner();
            if (owner)
            {
                auto bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    Cube aabb = bv->GetWorldAABB();

                    // 視錐台外ならスキップ
                    if (!FrustumIntersectsAABB(frustum, aabb))
                    {
                        continue;
                    }
                }
            }
        }
        
        comp->Draw();
        mCntDrawObject++;
    }
    
    // 状態戻し（保険）
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}


//=============================================================
// 共通ジオメトリ（スプライト／フルスクリーン）
//=============================================================

// スプライト用四角ポリゴン（ローカル [-0.5, 0.5] の正方形）
void Renderer::CreateSpriteVerts()
{
    const float vertices[] =
    {
        -0.5f,  0.5f, 0.f, 0.f, 0.f, 0.0f, 0.f, 0.f, // top left
         0.5f,  0.5f, 0.f, 0.f, 0.f, 0.0f, 1.f, 0.f, // top right
         0.5f, -0.5f, 0.f, 0.f, 0.f, 0.0f, 1.f, 1.f, // bottom right
        -0.5f, -0.5f, 0.f, 0.f, 0.f, 0.0f, 0.f, 1.f  // bottom left
    };
    
    const unsigned int indices[] =
    {
        2, 1, 0,
        0, 3, 2
    };

    mSpriteVerts = std::make_shared<VertexArray>(
        (float*)vertices, 4,
        (unsigned int*)indices, 6
    );
}

// フルスクリーンクアッド（PostEffect, 天候オーバーレイなど）
void Renderer::CreateFullScreenQuad()
{
    float quadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    unsigned int quadIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // 2D 頂点のみの簡易 VAO（isScreenQuad = true の想定）
    mFullScreenQuad = std::make_shared<VertexArray>(
        quadVerts, 4, quadIndices, 6, true
    );
}


//=============================================================
// データ解放
//=============================================================

void Renderer::UnloadData()
{
    // VisualComponent の登録だけをクリア
    // 実際の Mesh/Texture などのリソースは AssetManager 側で管理する想定
    mVisualComps.clear();
}

//=============================================================
// ウィンドウサイズ変更時
//=============================================================

void Renderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0) return;

    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    glViewport(0, 0, pixelW, pixelH);

    // DPI スケールもここで取り直しておくと、モニタ跨ぎ時も安全
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    // ここは Matrix4::CreatePerspectiveFOV のシグネチャに合わせて
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        0.1f,
        10000.f
    );

}

//=============================================================
// UI / Virtual 解像度関連
//=============================================================

void Renderer::SetVirtualResolution(float w, float h)
{
    mVirtualWidth  = w;
    mVirtualHeight = h;
}

// UIScaleInfo を計算して返す
UIScaleInfo Renderer::GetUIScaleInfo() const
{
    UIScaleInfo info{};

    // 物理解像度（必ずピクセルベース）
    info.screenW = mScreenWidth;
    info.screenH = mScreenHeight;

    // Virtual が未設定なら「物理＝論理」とみなす
    info.virtualW = (mVirtualWidth  > 0.0f) ? mVirtualWidth  : mScreenWidth;
    info.virtualH = (mVirtualHeight > 0.0f) ? mVirtualHeight : mScreenHeight;

    if (info.virtualW <= 0.0f) info.virtualW = 1.0f;
    if (info.virtualH <= 0.0f) info.virtualH = 1.0f;

    info.scaleX = info.screenW / info.virtualW;
    info.scaleY = info.screenH / info.virtualH;

    // レターボックス前提の共通スケール
    info.scale = (info.scaleX < info.scaleY) ? info.scaleX : info.scaleY;

    return info;
}

//=============================================================
// シャドウマッピング
//=============================================================

// シャドウマップ用 FBO 初期化
bool Renderer::InitializeShadowMapping()
{
    // シャドウマップ用 FBO 作成
    glGenFramebuffers(1, &mShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
    
    // シャドウ用テクスチャ生成（深度テクスチャ）
    mShadowMapTexture = std::make_shared<Texture>();
    mShadowMapTexture->CreateShadowMap(mShadowFBOWidth, mShadowFBOHeight);
    
    // FBO に深度テクスチャをアタッチ
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        mShadowMapTexture->GetTextureID(),
        0
    );
    
    // カラーバッファ無し（深度のみ）
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // 完成チェック
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Shadow framebuffer is not complete!" << std::endl;
        return false;
    }
    
    // FBOのバインド解除
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// シャドウマップのレンダリング
void Renderer::RenderShadowMap()
{
    // 太陽がほぼ消えている時はシャドウをスキップ
    float sunIntensity = mLightingManager->GetSunIntensity();
    if (sunIntensity <= 0.01f)
        return;
    
    //---------------------------------------------------------
    // シャドウ FBO バインド
    //---------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
    glViewport(0, 0,
               (GLsizei)mShadowFBOWidth,
               (GLsizei)mShadowFBOHeight);

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    //---------------------------------------------------------
    // ライト視点行列を構築
    //   - カメラの前方方向の少し先を中心にライトカメラを置く
    //   - Ortho + LookAt の組み合わせ
    //---------------------------------------------------------
    Vector3 camCenter = mInvView.GetTranslation() + mInvView.GetZAxis() * 30.0f;
    Vector3 lightDir  = mLightingManager->GetLightDirection();
    Vector3 lightPos  = camCenter - lightDir * 50.0f;
    
    Matrix4 lightView = Matrix4::CreateLookAt(
        lightPos,
        camCenter,
        Vector3::UnitY
    );

    Matrix4 lightProj = Matrix4::CreateOrtho(
        mShadowOrthoWidth,
        mShadowOrthoHeight,
        mShadowNear,
        mShadowFar
    );
    
    // OpenGL では通常 Projection * View を使うが、
    // ここでは view * proj の形で扱っている（フラスタム生成と対応）
    Matrix4 lightVP = lightView * lightProj;
    mLightSpaceMatrix = lightVP;
    
    // ライト側フラスタム（影用）を作成
    Frustum shadowFrustum = BuildFrustumFromMatrix(lightVP);
    
    //---------------------------------------------------------
    // 影描画ループ
    //---------------------------------------------------------
    for (auto& visual : mVisualComps)
    {
        if (!visual->GetEnableShadow() || !visual->IsVisible())
            continue;
        
        // ライト側フラスタムカリング
        Actor* owner = visual->GetOwner();
        if (owner)
        {
            auto bv = owner->GetComponent<BoundingVolumeComponent>();
            if (bv)
            {
                Cube aabb = bv->GetWorldAABB();
                // 必要なら aabb.Expand(…) 等で少し余裕を持たせる
                
                if (!FrustumIntersectsAABB(shadowFrustum, aabb))
                    continue;
            }
        }
        
        // 影用描画（VisualComponent 側でシャドウシェーダーを使う）
        visual->DrawShadow();
    }
    
    //---------------------------------------------------------
    // 元のフレームバッファとビューポートに戻す
    //---------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0,
               (GLsizei)mScreenWidth,
               (GLsizei)mScreenHeight);
}


//=============================================================
// その他ユーティリティ
//=============================================================

void Renderer::RegisterSkyDome(SkyDomeComponent* sky)
{
    mSkyDomeComp = sky;
    if (mSkyDomeComp)
    {
        // SkyDome にライティング情報を共有
        mSkyDomeComp->SetLightingManager(mLightingManager);
    }
}

// クリアカラー変更
void Renderer::SetClearColor(const Vector3& color)
{
    mClearColor = color;
    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
}


//=============================================================
// シェーダーロード
//=============================================================

bool Renderer::LoadShaders()
{
    std::string vShaderName;
    std::string fShaderName;
    
    //---------------------------------------------------------
    // 天気オーバーレイ用シェーダー
    //---------------------------------------------------------
    vShaderName = mShaderPath + "WeatherScreen.vert";
    fShaderName = mShaderPath + "WeatherScreen.frag";
    mShaders["WeatherOverlay"] = std::make_shared<Shader>();
    if (!mShaders["WeatherOverlay"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // メッシュ用 Phong シェーダー
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Phong.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Mesh"] = std::make_shared<Shader>();
    if (!mShaders["Mesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スキンメッシュ用（頂点のみ差し替え）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Skinned.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Skinned"] = std::make_shared<Shader>();
    if (!mShaders["Skinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スプライト用
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Sprite.vert";
    fShaderName = mShaderPath + "Sprite.frag";
    mShaders["Sprite"] = std::make_shared<Shader>();
    if (!mShaders["Sprite"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    // 2D用の固定 ViewProj をセット
    Matrix4 viewProj = Matrix4::CreateSimpleViewProj(mScreenWidth, mScreenHeight);
    mShaders["Sprite"]->SetMatrixUniform("uViewProj", viewProj);

    //---------------------------------------------------------
    // ビルボード
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Billboard.vert";
    fShaderName = mShaderPath + "Billboard.frag";
    mShaders["Billboard"] = std::make_shared<Shader>();
    if (!mShaders["Billboard"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // パーティクル
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Billboard.vert";
    fShaderName = mShaderPath + "Particle.frag";
    mShaders["Particle"] = std::make_shared<Shader>();
    if (!mShaders["Particle"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // ソリッドカラー（ワイヤーフレーム／デバッグ用など）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "BasicMesh.vert";
    fShaderName = mShaderPath + "SolidColor.frag";
    mShaders["Solid"] = std::make_shared<Shader>();
    if (!mShaders["Solid"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // シャドウマップ（スキンメッシュ）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ShadowMapping_Skinned.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowSkinned"] = std::make_shared<Shader>();
    if (!mShaders["ShadowSkinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // シャドウマップ（通常メッシュ）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ShadowMapping_Mesh.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowMesh"] = std::make_shared<Shader>();
    if (!mShaders["ShadowMesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スカイドーム（時間帯・天候ベースの空）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "WeatherDome.vert";
    fShaderName = mShaderPath + "WeatherDome.frag";
    mShaders["SkyDome"] = std::make_shared<Shader>();
    if (!mShaders["SkyDome"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // デフォルトのビュー／プロジェクション行列
    //---------------------------------------------------------
    mViewMatrix = Matrix4::CreateLookAt(
        Vector3(0, 0.5f, -3),
        Vector3(0, 0, 10),
        Vector3::UnitY
    );
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        1.0f,
        2000.0f
    );
    
    return true;
}


//=============================================================
// テキスト → テクスチャ生成（SDL3_ttf）
//=============================================================

std::shared_ptr<Texture> Renderer::CreateTextTexture(
    const std::string& text,
    const Vector3& color,
    std::shared_ptr<TextFont> font)
{
    if (!font || !font->IsValid())
    {
        std::cerr << "[Renderer] CreateTextTexture: invalid font" << std::endl;
        return nullptr;
    }

    if (text.empty())
    {
        return nullptr;
    }

    TTF_Font* nativeFont = font->GetNativeFont();

    SDL_Color sdlColor;
    sdlColor.r = static_cast<Uint8>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    sdlColor.g = static_cast<Uint8>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    sdlColor.b = static_cast<Uint8>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
    sdlColor.a = 255;

    // SDL3_ttf: TTF_RenderText_Blended( font, text, length, color )
    SDL_Surface* surface = TTF_RenderText_Blended(
        nativeFont,
        text.c_str(),
        text.size(),   // 長さを渡す（0 ではない）
        sdlColor
    );

    if (!surface)
    {
        std::cerr << "[Renderer] TTF_RenderText_Blended failed: "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }

    // OpenGL に渡しやすい RGBA8888 に変換
    SDL_Surface* conv = SDL_ConvertSurface(
        surface,
        SDL_PIXELFORMAT_RGBA32   // SDL3 推奨の 32bit RGBA
    );
    SDL_DestroySurface(surface);

    if (!conv)
    {
        std::cerr << "[Renderer] SDL_ConvertSurface failed: "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }

    const int   width  = conv->w;
    const int   height = conv->h;
    const void* pixels = conv->pixels;

    auto tex = std::make_shared<Texture>();
    if (!tex->CreateFromPixels(pixels, width, height, /*hasAlpha=*/true))
    {
        SDL_DestroySurface(conv);
        std::cerr << "[Renderer] CreateFromPixels failed" << std::endl;
        return nullptr;
    }

    SDL_DestroySurface(conv);
    return tex;
}

} // namespace toy
