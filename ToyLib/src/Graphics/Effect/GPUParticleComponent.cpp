// Graphics/Effect/GPUParticleComponent.cpp
#include "Graphics/Effect/GPUParticleComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Utils/JsonHelper.h"
#include "glad/glad.h"

#include <vector>
#include <cstddef> // offsetof
#include <random>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace toy {

//======================================================================
// 1) GPU データ定義 / 定数
//======================================================================

//--------------------------------------------------------------
// GPU particle layout（Transform Feedback 用 / interleaved）
//
// Update shader が Transform Feedback で “同じ並び” で書き戻す前提。
//  - position (vec3)
//  - velocity (vec3)
//  - life     (float)   ※ life >= lifeMax なら dead とみなす想定
//--------------------------------------------------------------
struct GPUParticle
{
    float px, py, pz;   // position
    float vx, vy, vz;   // velocity
    float life;         // seconds (>= lifeMax => dead)
};
static_assert(sizeof(GPUParticle) == sizeof(float) * 7, "GPUParticle layout must be 7 floats");

//--------------------------------------------------------------
// Quad geometry（板ポリ：SpriteVerts互換 pos3 + normal3 + uv2）
//
// 描画は「この quad を maxParticles 個インスタンシング」する。
// normal は互換のため保持（ここでは未使用でもOK）
//--------------------------------------------------------------
static const float kQuadVerts[4 * 8] =
{
    // x,    y,    z,    nx, ny, nz,    u,   v
    -0.5f,  0.5f, 0.0f,  0,  0,  0,   0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  0,  0,  0,   1.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0,  0,  0,   1.0f, 1.0f,
    -0.5f, -0.5f, 0.0f,  0,  0,  0,   0.0f, 1.0f
};

static const unsigned int kQuadIndices[6] =
{
    2, 1, 0,
    0, 3, 2
};

//======================================================================
// 2) Ctor / Dtor（ライフサイクル）
//======================================================================

GPUParticleComponent::GPUParticleComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder)
    , mInitialized(false)
    , mPingPong(false)
    , mRunning(false)
    , mComponentLifeAcc(0.0f)
    , mTimeAcc(0.0f)
    , mQuadVBO(0)
    , mQuadIBO(0)
    , mParticleVBO_A(0)
    , mParticleVBO_B(0)
    , mUpdateVAO(0)
    , mRenderVAO(0)
{
    // エフェクト枠で扱う（ToyLib の描画レイヤー整理に合わせる）
    mLayer = VisualLayer::Effect3D;

    // シェーダ取得（Renderer 管理）
    //  - ParticleUpdate : GPU 更新（Transform Feedback）
    //  - ParticleGPU    : 描画（インスタンシング）
    auto* r = GetOwner()->GetApp()->GetRenderer();
    mUpdateShader = r->GetShader("ParticleUpdate");
    mRenderShader = r->GetShader("ParticleGPU");

    // 初期状態は停止（Init() で true になる）
    mRunning = false;
}

GPUParticleComponent::~GPUParticleComponent()
{
    // VAO/VBO を確実に解放
    ReleaseGL();
}

//======================================================================
// 3) Public API（外部から呼ぶ機能）
//======================================================================

void GPUParticleComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}

//--------------------------------------------------------------
// Init
//  - Desc で初期化
//  - 値の正規化（sanitize）をここに一本化
//  - maxParticles が変わったら VBO を作り直す
//--------------------------------------------------------------
void GPUParticleComponent::Init(const Desc& desc)
{
    // 容量が変わるなら VBO 再生成が必要
    const bool needRebuild =
        (!mInitialized) ||
        (mDesc.maxParticles != desc.maxParticles);

    mDesc = desc;

    // sanitize（JSON経由もここを通る前提）
    mDesc.maxParticles     = std::max<uint32_t>(1, mDesc.maxParticles);
    mDesc.particleLife     = std::max(0.01f, mDesc.particleLife);
    mDesc.size             = std::max(0.01f, mDesc.size);
    mDesc.spawnRatePerSec  = std::max(0.0f,  mDesc.spawnRatePerSec);
    mDesc.spawnRampSec     = std::max(0.0f,  mDesc.spawnRampSec);
    mDesc.componentLife    = std::max(0.0f,  mDesc.componentLife);

    // mode ごとの “それっぽいプリセット” を適用（必要なら）
    ApplyModePresetIfNeeded();

    // 起動状態へ
    mIsVisible = true;
    mRunning   = true;
    mComponentLifeAcc = 0.0f;
    mTimeAcc = 0.0f;

    // GL リソース再構築が必要ならやり直す
    if (needRebuild)
    {
        ReleaseGL();
        InitIfNeeded(); // quad/VAO 等の遅延初期化
    }

    // 粒状態を初期化（warmStart で見た目の立ち上がりが自然になる）
    InitParticleBuffers(mDesc.warmStart);
}

void GPUParticleComponent::Start()
{
    mRunning = true;
    mIsVisible = true;
}

void GPUParticleComponent::Stop()
{
    // 更新・描画を止める（GL バッファは保持）
    mRunning = false;
}

void GPUParticleComponent::Reset()
{
    // 粒だけ初期化（容量は変えない）
    if (!mInitialized)
    {
        return;
    }
    InitParticleBuffers(false);
}

//======================================================================
// 4) Update / Draw（ゲームループで呼ばれる）
//======================================================================

void GPUParticleComponent::Update(float deltaTime)
{
    if (!mIsVisible || !mRunning)
    {
        return;
    }
    
    //----------------------------------------------------------
    // component lifetime（コンポーネント寿命）
    // 0 = 無限 / >0 なら時間経過で停止する
    //----------------------------------------------------------
    if (mDesc.componentLife > 0.0f)
    {
        mComponentLifeAcc += deltaTime;
        if (mComponentLifeAcc >= mDesc.componentLife)
        {
            mIsVisible = false;
            mRunning = false;
            return;
        }
    }

    if (!mInitialized)
    {
        return;
    }
    
    // 経過時間（スポーン制御などに使用）
    mTimeAcc += deltaTime;

    // GPU 側で粒更新（Transform Feedback + ping-pong）
    UpdateParticlesGPU(deltaTime);
}

void GPUParticleComponent::Draw()
{
    if (!mIsVisible || !mRunning)
    {
        return;
    }
    if (!mInitialized || !mTexture || !mRenderShader)
    {
        return;
    }
    //----------------------------------------------------------
    // ブレンド設定
    // ・加算合成：火花/発光系に向く
    // ・通常α    ：煙/水など
    //----------------------------------------------------------
    glEnable(GL_BLEND);
    if (mDesc.additiveBlend)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    
    // ToyLib の行列規約に合わせて ViewProj を構築
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    Matrix4 viewProj = view * proj;
    
    //----------------------------------------------------------
    // billboard basis（カメラの右・上を view 行列から抽出）
    // パーティクル quad を常にカメラ正面に向けるために使用
    //----------------------------------------------------------
    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();
    
    // shader uniforms
    mRenderShader->SetActive();
    mRenderShader->SetVectorUniform("uCameraRight", camRight);
    mRenderShader->SetVectorUniform("uCameraUp", camUp);
    mRenderShader->SetMatrixUniform("uViewProj", viewProj);
    mRenderShader->SetFloatUniform("uLifeMax", mDesc.particleLife);
    mRenderShader->SetFloatUniform("uSize", mDesc.size);
    
    // texture bind
    mTexture->SetActive(0);
    mRenderShader->SetTextureUniform("uTexture", 0);
    
    // VAO
    glBindVertexArray(mRenderVAO);
    
    // 現在の粒 VBO をインスタンス属性として束縛（pos, life）
    const unsigned int src = CurrentSrcVBO();
    BindInstanceAttributes(src);
    
    // quad を maxParticles 個描画
    glDrawElementsInstanced(
                            GL_TRIANGLES,
                            6,
                            GL_UNSIGNED_INT,
                            nullptr,
                            (GLsizei)mDesc.maxParticles
                            );
    
    // stats
    renderer->AddDrawCall();
    renderer->AddDrawObject();
    
    // 後続描画への影響を抑える（保険）
    if (mDesc.additiveBlend)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}
//======================================================================
// 5) GL 初期化 / 解放（リソース管理）
//======================================================================

//--------------------------------------------------------------
// InitIfNeeded
// まだ GL リソースが無い場合だけ作成する（遅延初期化）
//--------------------------------------------------------------
void GPUParticleComponent::InitIfNeeded()
{
    if (mInitialized)
    {
        return;
    }
    
    InitQuadGeometry();
    InitUpdateVAO();
    InitRenderVAO();

    mInitialized = true;
    mPingPong = false;
}

//--------------------------------------------------------------
// ReleaseGL
// VAO/VBO を安全に削除し、状態を未初期化に戻す
//--------------------------------------------------------------
void GPUParticleComponent::ReleaseGL()
{
    if (mUpdateVAO)
    {
        glDeleteVertexArrays(1, &mUpdateVAO);
        mUpdateVAO = 0;
    }
    if (mRenderVAO)
    {
        glDeleteVertexArrays(1, &mRenderVAO);
        mRenderVAO = 0;
    }

    if (mQuadVBO)
    {
        glDeleteBuffers(1, &mQuadVBO);
        mQuadVBO = 0;
    }
    if (mQuadIBO)
    {
        glDeleteBuffers(1, &mQuadIBO);
        mQuadIBO = 0;
    }

    if (mParticleVBO_A)
    {
        glDeleteBuffers(1, &mParticleVBO_A);
        mParticleVBO_A = 0;
    }
    if (mParticleVBO_B)
    {
        glDeleteBuffers(1, &mParticleVBO_B);
        mParticleVBO_B = 0;
    }

    mInitialized = false;
    mPingPong = false;
}

//--------------------------------------------------------------
// Quad buffers（描画用の板ポリ）
//--------------------------------------------------------------
void GPUParticleComponent::InitQuadGeometry()
{
    glGenBuffers(1, &mQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

    glGenBuffers(1, &mQuadIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mQuadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kQuadIndices), kQuadIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

//--------------------------------------------------------------
// Particle ping-pong buffers（粒データ VBO A/B）
//--------------------------------------------------------------
void GPUParticleComponent::InitParticleBuffers(bool warmStart)
{
    InitIfNeeded();

    const uint32_t N = mDesc.maxParticles;
    std::vector<GPUParticle> init(N);

    // warm start:
    //   life を (i/N)*lifeMax に分散して「一斉に生まれる塊感」を避ける
    //   速度にも微小ノイズを入れて立ち上がりを自然にする
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> u11(-1.0f, 1.0f);

    for (uint32_t i = 0; i < N; ++i)
    {
        auto& p = init[i];

        p.px = p.py = p.pz = 0.0f;
        p.vx = p.vy = p.vz = 0.0f;

        if (warmStart)
        {
            const float t = (float)i / (float)N;
            p.life = t * mDesc.particleLife;

            // tiny velocity noise (so update spreads immediately)
            p.vx = u11(rng) * 0.05f;
            p.vy = u11(rng) * 0.05f;
            p.vz = u11(rng) * 0.05f;
        }
        else
        {
            // dead（life > lifeMax）
            p.life = mDesc.particleLife + 1.0f;
        }
    }

    if (!mParticleVBO_A)
    {
        glGenBuffers(1, &mParticleVBO_A);
    }
    if (!mParticleVBO_B)
    {
        glGenBuffers(1, &mParticleVBO_B);
    }

    // A/B 両方へ同じ初期状態を投入（初回 ping-pong がどちらでも成立）
    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_A);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_B);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mPingPong = false;
}

//--------------------------------------------------------------
// Update VAO
// ここでは VAO を作るだけ。
// 実際の attribute pointer は毎フレーム srcVBO に合わせて束縛する。
//--------------------------------------------------------------
void GPUParticleComponent::InitUpdateVAO()
{
    glGenVertexArrays(1, &mUpdateVAO);
    glBindVertexArray(mUpdateVAO);
    glBindVertexArray(0);
}

//--------------------------------------------------------------
// Render VAO
// quad の固定 attribute を束縛しておく。
// 粒データ（instance attrs）は毎フレーム CurrentSrcVBO に合わせて束縛。
//--------------------------------------------------------------
void GPUParticleComponent::InitRenderVAO()
{
    glGenVertexArrays(1, &mRenderVAO);
    glBindVertexArray(mRenderVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mQuadIBO);

    const GLsizei stride = (GLsizei)(sizeof(float) * 8);

    // location 0: aQuadPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // location 1: aUV (vec2) at float[6]
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));

    glBindVertexArray(0);
}

//======================================================================
// 6) GPU 更新（Transform Feedback + ping-pong）
//======================================================================

void GPUParticleComponent::UpdateParticlesGPU(float dt)
{
    const unsigned int src = CurrentSrcVBO();
    const unsigned int dst = CurrentDstVBO();

    //----------------------------------------------------------
    // emitter world pos：
    // Actor ローカルの emitterOffset を Actor のワールド行列で変換
    //----------------------------------------------------------
    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 emitterWorld = Vector3::Transform(mDesc.emitterOffset, actorWorld);

    // Update 用 VAO を使用し、srcVBO を attribute に束縛
    glBindVertexArray(mUpdateVAO);
    BindUpdateAttributes(src);

    // update shader uniforms
    mUpdateShader->SetActive();
    mUpdateShader->SetFloatUniform("uDeltaTime", dt);
    mUpdateShader->SetFloatUniform("uTime", mTimeAcc);

    mUpdateShader->SetFloatUniform("uLifeMax", mDesc.particleLife);
    mUpdateShader->SetVectorUniform("uEmitterPos", emitterWorld);

    mUpdateShader->SetIntUniform("uMode", (int)mDesc.mode);
    mUpdateShader->SetFloatUniform("uGravity", mDesc.gravity);
    mUpdateShader->SetFloatUniform("uLift", mDesc.lift);
    mUpdateShader->SetFloatUniform("uSpread", mDesc.spread);

    mUpdateShader->SetFloatUniform("uSpawnRate", mDesc.spawnRatePerSec);
    mUpdateShader->SetFloatUniform("uSpawnRampSec", mDesc.spawnRampSec);

    //----------------------------------------------------------
    // Transform Feedback 実行
    // ・描画は不要なので Rasterizer discard
    // ・出力先を dstVBO にして glDrawArrays で更新を走らせる
    //----------------------------------------------------------
    glEnable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, dst);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mDesc.maxParticles);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glDisable(GL_RASTERIZER_DISCARD);

    glBindVertexArray(0);

    // 次フレームは src/dst を入れ替える
    mPingPong = !mPingPong;
}

//======================================================================
// 7) Attribute bind（VBO → shader attribute の束縛）
//======================================================================

//--------------------------------------------------------------
// Update attributes
// 0:pos, 1:vel, 2:life
//--------------------------------------------------------------
void GPUParticleComponent::BindUpdateAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);

    const GLsizei stride = (GLsizei)sizeof(GPUParticle);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, px));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, vx));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, life));
}

//--------------------------------------------------------------
// Render instance attributes
// 3:pos, 4:life
//
// quad の頂点属性(0,1)は固定で、粒ごとの値は instance divisor=1。
//--------------------------------------------------------------
void GPUParticleComponent::BindInstanceAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);

    const GLsizei stride = (GLsizei)sizeof(GPUParticle);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, px));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, life));
    glVertexAttribDivisor(4, 1);
}

//======================================================================
// 8) Helpers（ping-pong / mode preset / parse）
//======================================================================

unsigned int GPUParticleComponent::CurrentSrcVBO() const
{
    return mPingPong ? mParticleVBO_B : mParticleVBO_A;
}

unsigned int GPUParticleComponent::CurrentDstVBO() const
{
    return mPingPong ? mParticleVBO_A : mParticleVBO_B;
}

//--------------------------------------------------------------
// ApplyModePresetIfNeeded
// mode ごとの “最低限それっぽい” 値に寄せる。
// ※本来は「ユーザーが明示指定したか」を判定できると理想だが、
//   今は軽量優先で毎回ざっくり合わせる方針。
//--------------------------------------------------------------
void GPUParticleComponent::ApplyModePresetIfNeeded()
{
    if (mDesc.mode == ParticleMode::Water)
    {
        // 水系は重力で落とす（未設定なら 9.8）
        if (mDesc.gravity <= 0.0f)
        {
            mDesc.gravity = 9.8f;
        }
        mDesc.lift = 0.0f;
    }
    else if (mDesc.mode == ParticleMode::Smoke)
    {
        // 煙は浮かせる（lift 未設定なら 2.0）
        mDesc.gravity = 0.0f;
        if (mDesc.lift <= 0.0f)
        {
            mDesc.lift = 2.0f;
        }
    }
    else // Spark
    {
        mDesc.gravity = 0.0f;
        mDesc.lift = 0.0f;
    }
}

// mode string -> enum
GPUParticleComponent::ParticleMode GPUParticleComponent::ParseModeString(const std::string& s)
{
    if (s == "Water" || s == "water")
    {
        return ParticleMode::Water;
    }
    if (s == "Smoke" || s == "smoke")
    {
        return ParticleMode::Smoke;
    }
    return ParticleMode::Spark;
}

//======================================================================
// 9) File I/O（InitFromFile：差分定義 → Init() に一本化）
//======================================================================

//--------------------------------------------------------------
// InitFromFile（JsonHelper 流）
//
// 方針：
// ・Desc はデフォルト初期値から開始
// ・JSON に存在するキーだけ上書き（差分定義）
// ・最後は必ず Init(desc) を呼んで sanitize 等を一本化する
//--------------------------------------------------------------
bool GPUParticleComponent::InitFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open GPUParticle file: "
                  << filePath << std::endl;
        return false;
    }

    nlohmann::json data;
    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    //--------------------------------------------------
    // ★ 常に「Desc の初期値」からスタートする（差分定義）
    //--------------------------------------------------
    Desc desc;

    // mode（文字列指定）
    if (data.contains("mode"))
    {
        std::string modeStr;
        JsonHelper::GetString(data, "mode", modeStr);
        desc.mode = ParseModeString(modeStr);
    }

    // numeric params（キーが無ければ初期値のまま）
    // ※ maxParticles は uint32 だが JsonHelper の都合で int 経由にしている
    JsonHelper::GetInt  (data, "maxParticles",      reinterpret_cast<int&>(desc.maxParticles));
    JsonHelper::GetFloat(data, "componentLife",     desc.componentLife);
    JsonHelper::GetFloat(data, "particleLife",      desc.particleLife);
    JsonHelper::GetFloat(data, "size",              desc.size);
    JsonHelper::GetFloat(data, "spawnRatePerSec",   desc.spawnRatePerSec);
    JsonHelper::GetFloat(data, "spawnRampSec",      desc.spawnRampSec);
    JsonHelper::GetFloat(data, "spread",            desc.spread);
    JsonHelper::GetFloat(data, "gravity",           desc.gravity);
    JsonHelper::GetFloat(data, "lift",              desc.lift);

    // bools
    JsonHelper::GetBool(data, "additiveBlend", desc.additiveBlend);
    JsonHelper::GetBool(data, "warmStart",     desc.warmStart);

    // emitterOffset : [x,y,z]
    if (data.contains("emitterOffset") && data["emitterOffset"].is_array())
    {
        const auto& a = data["emitterOffset"];
        if (a.size() >= 3)
        {
            desc.emitterOffset.x = a[0].get<float>();
            desc.emitterOffset.y = a[1].get<float>();
            desc.emitterOffset.z = a[2].get<float>();
        }
    }

    //--------------------------------------------------
    // Init() に一本化（sanitize / rebuild / warmStart 等）
    //--------------------------------------------------
    Init(desc);

    std::cerr << "Loaded GPUParticle settings from "
              << filePath << std::endl;

    return true;
}

} // namespace toy
