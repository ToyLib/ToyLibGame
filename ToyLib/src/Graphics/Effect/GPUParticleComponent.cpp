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

//==============================================================
// GPU particle layout (Transform Feedback interleaved)
//   vec3 pos + vec3 vel + float life
//==============================================================
struct GPUParticle
{
    float px, py, pz;   // position
    float vx, vy, vz;   // velocity
    float life;         // seconds (>= lifeMax => dead)
};
static_assert(sizeof(GPUParticle) == sizeof(float) * 7, "GPUParticle layout must be 7 floats");

//==============================================================
// Quad geometry (SpriteVerts互換: pos3 + normal3 + uv2)
//==============================================================
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

//==============================================================
// ctor / dtor
//==============================================================
GPUParticleComponent::GPUParticleComponent(Actor* owner, int drawOrder)
: VisualComponent(owner, drawOrder)
{
    mLayer = VisualLayer::Effect3D;

    auto* r = GetOwner()->GetApp()->GetRenderer();
    mUpdateShader = r->GetShader("ParticleUpdate");
    mRenderShader = r->GetShader("ParticleGPU");

    mRunning = false;
}

GPUParticleComponent::~GPUParticleComponent()
{
    ReleaseGL();
}

//==============================================================
// New API
//==============================================================
void GPUParticleComponent::Init(const Desc& desc)
{
    const bool needRebuild =
        (!mInitialized) ||
        (mDesc.maxParticles != desc.maxParticles);

    mDesc = desc;

    // sanitize（ここに一本化）
    mDesc.maxParticles   = std::max<uint32_t>(1, mDesc.maxParticles);
    mDesc.particleLife   = std::max(0.01f, mDesc.particleLife);
    mDesc.size           = std::max(0.01f, mDesc.size);
    mDesc.spawnRatePerSec= std::max(0.0f,  mDesc.spawnRatePerSec);
    mDesc.spawnRampSec   = std::max(0.0f,  mDesc.spawnRampSec);
    mDesc.componentLife  = std::max(0.0f,  mDesc.componentLife);

    ApplyModePresetIfNeeded();

    mIsVisible = true;
    mRunning   = true;
    mComponentLifeAcc = 0.0f;
    mTimeAcc = 0.0f;

    if (needRebuild)
    {
        ReleaseGL();
        InitIfNeeded();
    }

    InitParticleBuffers(mDesc.warmStart);
}

void GPUParticleComponent::Start()
{
    mRunning = true;
    mIsVisible = true;
}

void GPUParticleComponent::Stop()
{
    mRunning = false;
}

void GPUParticleComponent::Reset()
{
    if (!mInitialized) return;
    InitParticleBuffers(false);
}

//==============================================================
// Update / Draw
//==============================================================
void GPUParticleComponent::Update(float deltaTime)
{
    if (!mIsVisible || !mRunning) return;

    // component lifetime
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

    if (!mInitialized) return;

    mTimeAcc += deltaTime;
    UpdateParticlesGPU(deltaTime);
}

void GPUParticleComponent::Draw()
{
    if (!mIsVisible || !mRunning) return;
    if (!mInitialized || !mTexture || !mRenderShader) return;

    glEnable(GL_BLEND);
    if (mDesc.additiveBlend)
        glBlendFunc(GL_ONE, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    Matrix4 viewProj = view * proj;

    // billboard basis from view matrix
    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();

    mRenderShader->SetActive();
    mRenderShader->SetVectorUniform("uCameraRight", camRight);
    mRenderShader->SetVectorUniform("uCameraUp", camUp);
    mRenderShader->SetMatrixUniform("uViewProj", viewProj);

    mRenderShader->SetFloatUniform("uLifeMax", mDesc.particleLife);
    mRenderShader->SetFloatUniform("uSize", mDesc.size);

    mTexture->SetActive(0);
    mRenderShader->SetTextureUniform("uTexture", 0);

    glBindVertexArray(mRenderVAO);

    const unsigned int src = CurrentSrcVBO();
    BindInstanceAttributes(src);

    glDrawElementsInstanced(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr,
        (GLsizei)mDesc.maxParticles
    );

    renderer->AddDrawCall();
    renderer->AddDrawObject();

    if (mDesc.additiveBlend)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==============================================================
// init / release
//==============================================================
void GPUParticleComponent::InitIfNeeded()
{
    if (mInitialized) return;

    InitQuadGeometry();
    InitUpdateVAO();
    InitRenderVAO();

    mInitialized = true;
    mPingPong = false;
}

void GPUParticleComponent::ReleaseGL()
{
    if (mUpdateVAO) { glDeleteVertexArrays(1, &mUpdateVAO); mUpdateVAO = 0; }
    if (mRenderVAO) { glDeleteVertexArrays(1, &mRenderVAO); mRenderVAO = 0; }

    if (mQuadVBO) { glDeleteBuffers(1, &mQuadVBO); mQuadVBO = 0; }
    if (mQuadIBO) { glDeleteBuffers(1, &mQuadIBO); mQuadIBO = 0; }

    if (mParticleVBO_A) { glDeleteBuffers(1, &mParticleVBO_A); mParticleVBO_A = 0; }
    if (mParticleVBO_B) { glDeleteBuffers(1, &mParticleVBO_B); mParticleVBO_B = 0; }

    mInitialized = false;
    mPingPong = false;
}

//--------------------------------------------------------------
// Quad buffers
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
// Particle ping-pong buffers
//--------------------------------------------------------------
void GPUParticleComponent::InitParticleBuffers(bool warmStart)
{
    InitIfNeeded();

    const uint32_t N = mDesc.maxParticles;
    std::vector<GPUParticle> init(N);

    // warm start: life をずらして塊を回避
    //   life = (i/N)*lifeMax で初期分散、速度も軽く乱数
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
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
            // dead
            p.life = mDesc.particleLife + 1.0f;
        }
    }

    if (!mParticleVBO_A) glGenBuffers(1, &mParticleVBO_A);
    if (!mParticleVBO_B) glGenBuffers(1, &mParticleVBO_B);

    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_A);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_B);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mPingPong = false;
}

//--------------------------------------------------------------
// Update VAO (attrs 0,1,2 from src particle VBO; bound per frame)
//--------------------------------------------------------------
void GPUParticleComponent::InitUpdateVAO()
{
    glGenVertexArrays(1, &mUpdateVAO);
    glBindVertexArray(mUpdateVAO);
    glBindVertexArray(0);
}

//--------------------------------------------------------------
// Render VAO (quad attrs 0,1 fixed; instance attrs 3,4 bound per frame)
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

//==============================================================
// GPU update (Transform Feedback)
//==============================================================
void GPUParticleComponent::UpdateParticlesGPU(float dt)
{
    const unsigned int src = CurrentSrcVBO();
    const unsigned int dst = CurrentDstVBO();

    // emitter world = Transform(localOffset, ActorWorld)
    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 emitterWorld = Vector3::Transform(mDesc.emitterOffset, actorWorld);

    glBindVertexArray(mUpdateVAO);
    BindUpdateAttributes(src);

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

    glEnable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, dst);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mDesc.maxParticles);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glDisable(GL_RASTERIZER_DISCARD);

    glBindVertexArray(0);

    mPingPong = !mPingPong;
}

//--------------------------------------------------------------
// bind update attributes (0:pos, 1:vel, 2:life)
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
// bind instance attributes for render (3:pos, 4:life)
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

//==============================================================
// helpers
//==============================================================
unsigned int GPUParticleComponent::CurrentSrcVBO() const
{
    return mPingPong ? mParticleVBO_B : mParticleVBO_A;
}

unsigned int GPUParticleComponent::CurrentDstVBO() const
{
    return mPingPong ? mParticleVBO_A : mParticleVBO_B;
}

void GPUParticleComponent::ApplyModePresetIfNeeded()
{
    // mode default params (only when user didn't explicitly override after Init)
    // (軽量: Initのたびに一旦“それっぽい値”に合わせる)
    if (mDesc.mode == ParticleMode::Water)
    {
        if (mDesc.gravity <= 0.0f) mDesc.gravity = 9.8f;
        mDesc.lift = 0.0f;
    }
    else if (mDesc.mode == ParticleMode::Smoke)
    {
        mDesc.gravity = 0.0f;
        if (mDesc.lift <= 0.0f) mDesc.lift = 2.0f;
    }
    else // Spark
    {
        mDesc.gravity = 0.0f;
        mDesc.lift = 0.0f;
    }
}

//------------------------------------------------------------
// mode string -> enum
//------------------------------------------------------------
GPUParticleComponent::ParticleMode GPUParticleComponent::ParseModeString(const std::string& s)
{
    if (s == "Water" || s == "water") return ParticleMode::Water;
    if (s == "Smoke" || s == "smoke") return ParticleMode::Smoke;
    return ParticleMode::Spark;
}

//------------------------------------------------------------
// InitFromFile (JsonHelper流)
//------------------------------------------------------------
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
    // ★ 常に初期値からスタート
    //--------------------------------------------------
    Desc desc;

    // mode
    if (data.contains("mode"))
    {
        std::string modeStr;
        JsonHelper::GetString(data, "mode", modeStr);
        desc.mode = ParseModeString(modeStr);
    }

    // numeric params
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

    // emitterOffset
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
    // Init() に一本化
    //--------------------------------------------------
    Init(desc);

    std::cerr << "Loaded GPUParticle settings from "
              << filePath << std::endl;

    return true;
}

void GPUParticleComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}
} // namespace toy
