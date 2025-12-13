// ParticleComponent.cpp
#include "Graphics/Effect/ParticleComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Geometry/VertexArray.h"
#include <random>

namespace toy {

//======================================================================
// コンストラクタ
//======================================================================
ParticleComponent::ParticleComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder)
    , mTexture(nullptr)
    , mDrawOrder(drawOrder)
    , mIsBlendAdd(true)
    , mNumParts(0)
    , mLifeTime(0.0f)
    , mTotalLife(0.0f)
    , mPartLifecycle(0.0f)
    , mPartSize(0.0f)
    , mPartSpeed(2.0f)
    , mParticleMode(P_SPARK)
{
    // 3D エフェクト扱い（ライト・深度あり）
    mLayer = VisualLayer::Effect3D;

    // パーティクル用シェーダ
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Particle");
}

ParticleComponent::~ParticleComponent()
{
    mParts.clear();
}

//======================================================================
// テクスチャ設定
//======================================================================
void ParticleComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = tex;
}

//======================================================================
// パーティクル生成（必要数だけ配列を確保）
//======================================================================
void ParticleComponent::CreateParticles(
    Vector3 pos,
    unsigned int num,
    float life,
    float partLife,
    float size,
    ParticleMode mode
)
{
    mPosition       = pos;
    mIsVisible      = true;
    mNumParts       = num;
    mLifeTime       = 0.0f;
    mTotalLife      = life;
    mPartLifecycle  = partLife;
    mPartSize       = size;
    mParticleMode   = mode;

    mParts.resize(mNumParts);
}

//======================================================================
// パーティクル 1 個生成
// - ランダムな方向に初期化する
// - 空いているスロットだけ再利用
//======================================================================
void ParticleComponent::GenerateParts()
{
    std::random_device rnd;

    for (int i = 0; i < mNumParts; i++)
    {
        if (mParts[i].isVisible)
        {
            continue;   // 生存中なら skip
        }
        
        // ランダム方向（雑だが軽量）
        float x = (float)(rnd() % (int)mPartSpeed);
        float y = (float)(rnd() % (int)mPartSpeed);
        float z = (float)(rnd() % (int)mPartSpeed);
        if (rand() % 2) x *= -1;
        if (rand() % 2) y *= -1;
        if (rand() % 2) z *= -1;

        mParts[i].pos       = mPosition;
        mParts[i].dir       = Vector3(x, y, z);
        mParts[i].isVisible = true;
        mParts[i].lifeTime  = 0.0f;
        mParts[i].size      = mPartSize;
        break;
    }
}

//======================================================================
// Update
// - パーティクル寿命管理
// - モードごとの挙動（上昇/落下）
// - ランダムに新規生成
//======================================================================
void ParticleComponent::Update(float deltaTime)
{
    // コンポーネント寿命
    mLifeTime += deltaTime;
    if (mLifeTime > mTotalLife)
    {
        mIsVisible = false;
    }

    // 各パーティクル更新
    for (int i = 0; i < mNumParts; i++)
    {
        if (mParts[i].isVisible)
        {
            // モード別上下方向の変化
            if (mParticleMode == P_WATER)
                mParts[i].dir.y -= 0.04f;   // 落下
            else if (mParticleMode == P_SMOKE)
                mParts[i].dir.y += 0.04f;   // 上昇

            // 位置更新
            mParts[i].lifeTime += deltaTime;
            mParts[i].pos += mParts[i].dir * deltaTime;

            // 寿命超えたら非表示
            if (mParts[i].lifeTime > mPartLifecycle)
                mParts[i].isVisible = false;
        }
    }

    // ランダムに新規生成（負荷軽減の簡易実装）
    if (rand() % 2 == 0)
    {
        GenerateParts();
    }
}

//======================================================================
// Draw（フルビルボード描画）
// - 加算／アルファブレンド切り替え
// - uPosition をパーティクルごとに更新して 6 ポリゴン描画
//======================================================================
void ParticleComponent::Draw()
{
    if (!mIsVisible || mTexture == nullptr)
    {
        return;
    }

    //------------------------------
    // ブレンド設定
    //------------------------------
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE); // 加算
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 通常
    }

    //------------------------------
    // ビルボード用ワールド行列
    //------------------------------
    Matrix4 mat = GetOwner()->GetWorldTransform();
    Matrix4 invView = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix();

    // カメラの向きだけ利用し、位置はパーティクルに合わせる
    invView.mat[3][0] = mat.mat[3][0];
    invView.mat[3][1] = mat.mat[3][1];
    invView.mat[3][2] = mat.mat[3][2];

    Matrix4 scaleMat = Matrix4::CreateScale(mPartSize, mPartSize, 1);
    Matrix4 world = scaleMat *
                    Matrix4::CreateScale(GetOwner()->GetScale()) *
                    invView;

    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();

    //------------------------------
    // シェーダ設定
    //------------------------------
    mShader->SetActive();
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", world);

    mTexture->SetActive(0);
    mShader->SetTextureUniform("uTexture", 0);

    //------------------------------
    // パーティクルを 1 つずつ描画
    //------------------------------
    mVertexArray->SetActive();
    for (int i = 0; i < mNumParts; i++)
    {
        if (mParts[i].isVisible)
        {
            // 位置だけ更新して 6 ポリゴン描画
            mShader->SetVectorUniform("uPosition", mParts[i].pos);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            renderer->AddDrawCall();
        }
    }

    //------------------------------
    // ブレンド戻す
    //------------------------------
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    renderer->AddDrawObject();
}

} // namespace toy
