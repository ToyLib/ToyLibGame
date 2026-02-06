#include "Graphics/Effect/ParticleComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/IRenderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"

#include "Asset/Material/Texture.h"
#include "Utils/JsonHelper.h"

#include "glad/glad.h"

#include <vector>
#include <cstddef>
#include <random>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace toy {

//======================================================================
// particle data
//======================================================================

struct Particle
{
    float px, py, pz;
    float vx, vy, vz;
    float life;
};

static_assert(sizeof(Particle) == sizeof(float) * 7, "Particle layout");

// quad geometry (pos3 + normal3 + uv2)
static const float kQuadVerts[4 * 8] =
{
    -0.5f,  0.5f, 0.0f,  0,0,0,  0.0f,0.0f,
     0.5f,  0.5f, 0.0f,  0,0,0,  1.0f,0.0f,
     0.5f, -0.5f, 0.0f,  0,0,0,  1.0f,1.0f,
    -0.5f, -0.5f, 0.0f,  0,0,0,  0.0f,1.0f
};

static const unsigned int kQuadIndices[6] =
{
    2,1,0,
    0,3,2
};

//======================================================================
// ctor / dtor
//======================================================================

ParticleComponent::ParticleComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder)
{
    mLayer = VisualLayer::Effect3D;

    if (auto* r = GetOwner()->GetApp()->GetRenderer())
    {
        mUpdateShader = r->GetShader("ParticleUpdate");
        mRenderShader = r->GetShader("Particle");
    }

    mRunning = false;
}

ParticleComponent::~ParticleComponent()
{
    ReleaseGL();
}

//======================================================================
// Public API
//======================================================================

void ParticleComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}

void ParticleComponent::Init(const Desc& desc)
{
    const bool needRebuild =
        (!mInitialized) || (mDesc.maxParticles != desc.maxParticles);

    mDesc = desc;

    mDesc.maxParticles    = std::max<uint32_t>(1, mDesc.maxParticles);
    mDesc.particleLife    = std::max(0.01f, mDesc.particleLife);
    mDesc.size            = std::max(0.01f, mDesc.size);
    mDesc.spawnRatePerSec = std::max(0.0f,  mDesc.spawnRatePerSec);
    mDesc.spawnRampSec    = std::max(0.0f,  mDesc.spawnRampSec);

    ApplyModePresetIfNeeded();

    mIsVisible = true;
    mRunning   = true;
    mTimeAcc   = 0.0f;
    mComponentLifeAcc = 0.0f;

    if (needRebuild)
    {
        ReleaseGL();
        InitIfNeeded();
    }

    InitParticleBuffers(mDesc.warmStart);
}

bool ParticleComponent::InitFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    nlohmann::json data;
    file >> data;

    Desc desc;

    if (data.contains("mode"))
    {
        std::string modeStr;
        JsonHelper::GetString(data, "mode", modeStr);
        desc.mode = ParseModeString(modeStr);
    }

    JsonHelper::GetInt  (data, "maxParticles",    reinterpret_cast<int&>(desc.maxParticles));
    JsonHelper::GetFloat(data, "componentLife",   desc.componentLife);
    JsonHelper::GetFloat(data, "particleLife",    desc.particleLife);
    JsonHelper::GetFloat(data, "size",            desc.size);
    JsonHelper::GetFloat(data, "spawnRatePerSec", desc.spawnRatePerSec);
    JsonHelper::GetFloat(data, "spawnRampSec",    desc.spawnRampSec);
    JsonHelper::GetFloat(data, "spread",          desc.spread);
    JsonHelper::GetFloat(data, "gravity",         desc.gravity);
    JsonHelper::GetFloat(data, "lift",            desc.lift);

    JsonHelper::GetBool(data, "additiveBlend", desc.additiveBlend);
    JsonHelper::GetBool(data, "warmStart",     desc.warmStart);

    Init(desc);
    return true;
}

void ParticleComponent::Start()
{
    mRunning   = true;
    mIsVisible = true;
}

void ParticleComponent::Stop()
{
    mRunning = false;
}

void ParticleComponent::Reset()
{
    mPendingHardReset = true;
    mSkipDrawFrames   = 2;
    mRunning          = false;
    mIsVisible        = false;
}

//======================================================================
// Update
//======================================================================

void ParticleComponent::Update(float deltaTime)
{
    if (mPendingHardReset)
    {
        mPendingHardReset = false;

        if (mInitialized)
        {
            InitParticleBuffers(false);
            mTimeAcc = 0.0f;
            mComponentLifeAcc = 0.0f;
        }

        mSkipDrawFrames = std::max(mSkipDrawFrames, 1);
        return;
    }

    if (!mIsVisible || !mRunning || !mInitialized)
        return;

    // ★ componentLife（元仕様どおり）
    if (mDesc.componentLife > 0.0f)
    {
        mComponentLifeAcc += deltaTime;
        if (mComponentLifeAcc >= mDesc.componentLife)
        {
            mIsVisible = false;
            mRunning   = false;
            return;
        }
    }

    mTimeAcc += deltaTime;
    UpdateParticlesGPU(deltaTime);
}


//======================================================================
// RenderQueue
//======================================================================
void ParticleComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (mSkipDrawFrames > 0)
    {
        --mSkipDrawFrames;
        return;
    }
    if (!mIsVisible || !mRunning)
    {
        return;
    }
    if (!mInitialized || !mTexture || !mRenderShader)
    {
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    // camera right / up
    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();

    // ★instance attribute 更新（既存挙動維持）
    glBindVertexArray(mRenderVAO);
    BindInstanceAttributes(CurrentSrcVBO());
    glBindVertexArray(0);

    //==================================================
    // Particle payload
    //==================================================
    ParticlePayload pp {};
    pp.cameraRight     = camRight;
    pp.cameraUp        = camUp;
    pp.particleLifeMax = mDesc.particleLife;
    pp.particleSize    = mDesc.size;

    const uint32_t payloadIndex = outQueue.PushParticlePayload(pp);

    //==================================================
    // RenderItem
    //==================================================
    RenderItem it {};
    it.pass      = RenderPass::World;
    it.layer     = VisualLayer::Effect3D;
    it.drawOrder = GetDrawOrder();

    it.type     = RenderItemType::Particle;
    it.dispatch = GetDispatch(it.type);

    // state
    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = mDesc.additiveBlend ? BlendMode::Additive : BlendMode::Alpha;
    it.cull       = CullMode::Front;
    it.frontFace  = FrontFace::CW;

    // shader / texture
    it.shader.ptr  = mRenderShader.get();
    it.texture.ptr = mTexture.get();
    it.textureUnit = 0;

    // transforms
    it.world    = Matrix4::Identity;
    it.viewProj = viewProj;

    // instanced draw
    it.gpuVAO        = mRenderVAO;
    it.instanceCount = static_cast<int>(mDesc.maxParticles);

    it.topology   = PrimitiveTopology::Triangles;
    it.indexCount = 6; // quad

    it.payloadIndex = payloadIndex;

    outQueue.Push(it);
}
//======================================================================
// GL init / update
//======================================================================

void ParticleComponent::InitIfNeeded()
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

void ParticleComponent::ReleaseGL()
{
    if (mUpdateVAO)
    {
        glDeleteVertexArrays(1, &mUpdateVAO);
    }
    if (mRenderVAO)
    {
        glDeleteVertexArrays(1, &mRenderVAO);
    }
    if (mQuadVBO)
    {
        glDeleteBuffers(1, &mQuadVBO);
    }
    if (mQuadIBO)
    {
        glDeleteBuffers(1, &mQuadIBO);
    }
    if (mParticleVBO_A)
    {
        glDeleteBuffers(1, &mParticleVBO_A);
    }
    if (mParticleVBO_B)
    {
        glDeleteBuffers(1, &mParticleVBO_B);
    }

    mInitialized = false;
    mPingPong = false;
}

void ParticleComponent::InitQuadGeometry()
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

void ParticleComponent::InitUpdateVAO()
{
    glGenVertexArrays(1, &mUpdateVAO);
}

void ParticleComponent::InitRenderVAO()
{
    glGenVertexArrays(1, &mRenderVAO);
    glBindVertexArray(mRenderVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mQuadIBO);

    const GLsizei stride = sizeof(float) * 8;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));

    glBindVertexArray(0);
}

void ParticleComponent::UpdateParticlesGPU(float deltaTime)
{
    const unsigned int src = CurrentSrcVBO();
    const unsigned int dst = CurrentDstVBO();

    glBindVertexArray(mUpdateVAO);
    BindUpdateAttributes(src);

    if (!mUpdateShader)
    {
        glBindVertexArray(0);
        return;
    }

    mUpdateShader->SetActive();
    mUpdateShader->SetFloatUniform("uDeltaTime", deltaTime);
    mUpdateShader->SetFloatUniform("uTime",      mTimeAcc);
    mUpdateShader->SetFloatUniform("uLifeMax",   mDesc.particleLife);

    // ★追加：emitter（ワールド）
    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 emitterWorld = Vector3::Transform(mDesc.emitterOffset, actorWorld);
    mUpdateShader->SetVectorUniform("uEmitterPos", emitterWorld);

    // ★追加：mode/forces
    mUpdateShader->SetIntUniform  ("uMode",   (int)mDesc.mode);
    mUpdateShader->SetFloatUniform("uGravity", mDesc.gravity);
    mUpdateShader->SetFloatUniform("uLift",    mDesc.lift);
    mUpdateShader->SetFloatUniform("uSpread",  mDesc.spread);

    // ★追加：spawn
    mUpdateShader->SetFloatUniform("uSpawnRate",    mDesc.spawnRatePerSec);
    mUpdateShader->SetFloatUniform("uSpawnRampSec", mDesc.spawnRampSec);

    glEnable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, dst);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mDesc.maxParticles);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0); // ★保険
    glDisable(GL_RASTERIZER_DISCARD);
    glBindVertexArray(0);

    mPingPong = !mPingPong;
}

//======================================================================
// Attribute bind / helpers
//======================================================================

void ParticleComponent::BindUpdateAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);
    const GLsizei stride = sizeof(Particle);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Particle, px));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Particle, vx));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Particle, life));
}

void ParticleComponent::BindInstanceAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);
    const GLsizei stride = sizeof(Particle);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Particle, px));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Particle, life));
    glVertexAttribDivisor(4, 1);
}

unsigned int ParticleComponent::CurrentSrcVBO() const
{
    return mPingPong ? mParticleVBO_B : mParticleVBO_A;
}

unsigned int ParticleComponent::CurrentDstVBO() const
{
    return mPingPong ? mParticleVBO_A : mParticleVBO_B;
}

void ParticleComponent::ApplyModePresetIfNeeded()
{
    if (mDesc.mode == ParticleMode::Water)
    {
        mDesc.gravity = std::max(mDesc.gravity, 9.8f);
        mDesc.lift = 0.0f;
    }
    else if (mDesc.mode == ParticleMode::Smoke)
    {
        mDesc.gravity = 0.0f;
        mDesc.lift = std::max(mDesc.lift, 2.0f);
    }
}

ParticleComponent::ParticleMode
ParticleComponent::ParseModeString(const std::string& s)
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

void ParticleComponent::InitParticleBuffers(bool warmStart)
{
    InitIfNeeded();

    const uint32_t N = mDesc.maxParticles;

    std::vector<Particle> init;
    init.resize(N);

    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> u11(-1.0f, 1.0f);

    for (uint32_t i = 0; i < N; ++i)
    {
        auto& p = init[i];

        // position
        p.px = 0.0f;
        p.py = 0.0f;
        p.pz = 0.0f;

        // velocity
        p.vx = 0.0f;
        p.vy = 0.0f;
        p.vz = 0.0f;

        if (warmStart)
        {
            // 0..lifeMax のどこかに散らして、既に飛んでる見た目にする
            const float t = (N > 1) ? (static_cast<float>(i) / static_cast<float>(N - 1)) : 0.0f;
            p.life = t * mDesc.particleLife;

            // ほんの少しランダム速度（見た目の自然さ用）
            p.vx = u11(rng) * 0.05f;
            p.vy = u11(rng) * 0.05f;
            p.vz = u11(rng) * 0.05f;
        }
        else
        {
            // “死んでる” 初期値：iLife >= uLifeMax 扱い
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
    
    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_A);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(Particle) * init.size()),
                 init.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_B);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(Particle) * init.size()),
                 init.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // ping-pong 初期は A を src にする
    mPingPong = false;
}

} // namespace toy
