#include "Graphics/Effect/GLParticleBackend.h"

#include "Graphics/VisualComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

#include "Render/IRenderer.h"
#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"
#include "Render/RenderItemPayloads.h"
#include "Render/GL/GLRenderer.h"
#include "Render/GL/GLShader.h"
#include "Render/GL/UniformNamesGL.h"

#include "Asset/Material/Texture.h"
#include "Utils/JsonHelper.h"

#include "glad/glad.h"

#include <vector>
#include <cstddef>
#include <random>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace toy
{

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

namespace
{
    std::mt19937 gRng(1337u);

    float RandRange(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gRng);
    }
}


//======================================================================
// ctor / dtor
//======================================================================
GLParticleBackend::GLParticleBackend(Actor* owner)
    : IParticleBackend(owner)
{
    if (auto* app = GetOwner()->GetApp())
    {
        if (auto* r = app->GetRenderer())
        {
            mUpdatePipelineName = "ParticleUpdate";
            mRenderPipelineName = "Particle";
            mUpdateShader = r->GetPipelineHandle(mUpdatePipelineName).ptrGLShader;
        }
    }
}

GLParticleBackend::~GLParticleBackend()
{
    ReleaseGL();
}

//======================================================================
// Public API
//======================================================================
void GLParticleBackend::Init(const ParticleDesc& desc)
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

    mRunning = true;
    mTimeAcc = 0.0f;
    mComponentLifeAcc = 0.0f;

    if (needRebuild)
    {
        ReleaseGL();
        InitIfNeeded();
    }

    InitParticleBuffers(mDesc.warmStart);
}

bool GLParticleBackend::InitFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json data;
    file >> data;

    ParticleDesc desc {};

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

void GLParticleBackend::Start()
{
    mRunning = true;
}

void GLParticleBackend::Stop()
{
    mRunning = false;
}

void GLParticleBackend::Reset()
{
    mPendingHardReset = true;
    mSkipDrawFrames   = 2;
    mRunning          = false;
}

//======================================================================
// Update
//======================================================================
void GLParticleBackend::Update(float deltaTime)
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

    if (!mRunning || !mInitialized)
    {
        return;
    }

    if (mDesc.componentLife > 0.0f)
    {
        mComponentLifeAcc += deltaTime;
        if (mComponentLifeAcc >= mDesc.componentLife)
        {
            mRunning = false;
            return;
        }
    }

    mTimeAcc += deltaTime;
    UpdateParticlesGPU(deltaTime);
}

//======================================================================
// RenderQueue
//======================================================================
void GLParticleBackend::GatherRenderItems(RenderQueue& outQueue,
                                          const VisualComponent& host)
{
    if (mSkipDrawFrames > 0)
    {
        --mSkipDrawFrames;
        return;
    }

    if (!host.IsVisible() || !mRunning)
    {
        return;
    }

    if (!mInitialized || !host.GetTexture())
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

    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();

    glBindVertexArray(mRenderVAO);
    BindInstanceAttributes(CurrentSrcVBO());
    glBindVertexArray(0);

    ParticlePayload pp {};
    pp.cameraRight     = camRight;
    pp.cameraUp        = camUp;
    pp.particleLifeMax = mDesc.particleLife;
    pp.particleSize    = mDesc.size;

    const uint32_t payloadIndex = outQueue.PushParticlePayload(pp);

    RenderItem it {};
    it.pass      = RenderPass::World;
    it.layer     = host.GetLayer();
    it.drawOrder = host.GetDrawOrder();

    it.type     = RenderItemType::Particle;
    it.dispatch = GetDispatch(it.type);

    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = mDesc.additiveBlend ? BlendMode::Additive : BlendMode::Alpha;
    it.cull       = CullMode::Back;
    it.frontFace  = FrontFace::CCW;

    it.pipeline    = renderer->GetPipelineHandle(mRenderPipelineName);
    it.texture.ptr = host.GetTexture().get();
    it.textureUnit = 0;

    it.world    = Matrix4::Identity;
    it.viewProj = viewProj;

    it.gpuVAO        = mRenderVAO;
    it.instanceCount = static_cast<int>(mDesc.maxParticles);

    it.topology   = PrimitiveTopology::Triangles;
    it.indexCount = 6;

    it.payloadIndex = payloadIndex;

    outQueue.Push(it);
    
}

//======================================================================
// GL init / update
//======================================================================
void GLParticleBackend::InitIfNeeded()
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

void GLParticleBackend::ReleaseGL()
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

void GLParticleBackend::InitQuadGeometry()
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

void GLParticleBackend::InitUpdateVAO()
{
    glGenVertexArrays(1, &mUpdateVAO);
}

void GLParticleBackend::InitRenderVAO()
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

void GLParticleBackend::UpdateParticlesGPU(float deltaTime)
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

    using namespace toy::glsl;

    mUpdateShader->SetActive();
    mUpdateShader->SetFloatUniform(ParticleUpdate::DeltaTime, deltaTime);
    mUpdateShader->SetFloatUniform(ParticleUpdate::Time,      mTimeAcc);
    mUpdateShader->SetFloatUniform(ParticleUpdate::LifeMax,   mDesc.particleLife);

    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 emitterWorld = Vector3::Transform(mDesc.emitterOffset, actorWorld);
    mUpdateShader->SetVectorUniform(ParticleUpdate::EmitterPos, emitterWorld);

    mUpdateShader->SetIntUniform  (ParticleUpdate::Mode,    (int)mDesc.mode);
    mUpdateShader->SetFloatUniform(ParticleUpdate::Gravity, mDesc.gravity);
    mUpdateShader->SetFloatUniform(ParticleUpdate::Lift,    mDesc.lift);
    mUpdateShader->SetFloatUniform(ParticleUpdate::Spread,  mDesc.spread);
    mUpdateShader->SetFloatUniform(ParticleUpdate::SpawnRate,    mDesc.spawnRatePerSec);
    mUpdateShader->SetFloatUniform(ParticleUpdate::SpawnRampSec, mDesc.spawnRampSec);
    
    
    if (mDesc.mode == ParticleMode::SnowField)
    {
        Vector3 fieldCenter = emitterWorld;
        if (mDesc.followCamera)
        {
            if (auto* renderer = GetOwner()->GetApp()->GetRenderer())
            {
                fieldCenter = renderer->GetInvViewMatrix().GetTranslation();
            }
        }

        mUpdateShader->SetVectorUniform("uFieldCenter", fieldCenter);
        mUpdateShader->SetVectorUniform("uFieldExtent", mDesc.fieldExtent);
        mUpdateShader->SetVectorUniform("uWind",        mDesc.wind);
        mUpdateShader->SetIntUniform   ("uRespawnTop",  mDesc.respawnTop ? 1 : 0);
    }

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

void GLParticleBackend::BindUpdateAttributes(unsigned int srcVBO)
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

void GLParticleBackend::BindInstanceAttributes(unsigned int srcVBO)
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

unsigned int GLParticleBackend::CurrentSrcVBO() const
{
    return mPingPong ? mParticleVBO_B : mParticleVBO_A;
}

unsigned int GLParticleBackend::CurrentDstVBO() const
{
    return mPingPong ? mParticleVBO_A : mParticleVBO_B;
}

void GLParticleBackend::ApplyModePresetIfNeeded()
{
    if (mDesc.mode == ParticleMode::Water)
    {
        mDesc.gravity = std::max(mDesc.gravity, 9.8f);
        mDesc.lift    = 0.0f;
    }
    else if (mDesc.mode == ParticleMode::Smoke)
    {
        mDesc.gravity = 0.0f;
        mDesc.lift    = std::max(mDesc.lift, 2.0f);
    }
    else if (mDesc.mode == ParticleMode::SnowField)
    {
        mDesc.maxParticles    = std::max<uint32_t>(mDesc.maxParticles, 256u);
        mDesc.componentLife   = 0.0f;   // 常時稼働
        mDesc.particleLife    = std::max(mDesc.particleLife, 8.0f);
        mDesc.size            = std::max(0.01f, mDesc.size);

        // いまは既存 update ロジックのまま使うので
        // 既存モードに悪影響のない preset だけ入れる
        mDesc.spawnRatePerSec = 0.0f;
        mDesc.spawnRampSec    = 0.0f;

        mDesc.gravity         = std::max(mDesc.gravity, 0.6f);
        mDesc.lift            = 0.0f;

        mDesc.additiveBlend   = false;
        mDesc.warmStart       = true;

        if (mDesc.fieldExtent.x <= 0.0f) mDesc.fieldExtent.x = 20.0f;
        if (mDesc.fieldExtent.y <= 0.0f) mDesc.fieldExtent.y = 12.0f;
        if (mDesc.fieldExtent.z <= 0.0f) mDesc.fieldExtent.z = 20.0f;

        mDesc.followCamera = true;
        mDesc.respawnTop   = true;
    }
}

ParticleMode GLParticleBackend::ParseModeString(const std::string& s)
{
    if (s == "Water" || s == "water")
    {
        return ParticleMode::Water;
    }
    if (s == "Smoke" || s == "smoke")
    {
        return ParticleMode::Smoke;
    }
    if (s == "SnowField" || s == "snowfield" || s == "snow")
    {
        return ParticleMode::SnowField;
    }
    return ParticleMode::Spark;
}

void GLParticleBackend::InitParticleBuffers(bool warmStart)
{
    const uint32_t N = mDesc.maxParticles;

    std::vector<Particle> init;
    init.resize(N);

    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> u11(-1.0f, 1.0f);

    auto* app = GetOwner() ? GetOwner()->GetApp() : nullptr;
    auto* renderer = app ? app->GetRenderer() : nullptr;

    Vector3 emitterWorld = Vector3::Zero;
    if (GetOwner())
    {
        emitterWorld = GetOwner()->GetWorldTransform().GetTranslation() + mDesc.emitterOffset;
    }

    Vector3 fieldCenter = emitterWorld;
    if (mDesc.followCamera && renderer)
    {
        fieldCenter = renderer->GetInvViewMatrix().GetTranslation();
    }

    for (uint32_t i = 0; i < N; ++i)
    {
        auto& p = init[i];

        if (mDesc.mode == ParticleMode::SnowField)
        {
            const Vector3 halfExt = mDesc.fieldExtent * 0.5f;

            // ------------------------------------------------------
            // 初期位置
            // ------------------------------------------------------
            p.px = fieldCenter.x + RandRange(-halfExt.x, halfExt.x);
            p.pz = fieldCenter.z + RandRange(-halfExt.z, halfExt.z);

            if (warmStart)
            {
                p.py = fieldCenter.y + RandRange(-halfExt.y, halfExt.y);
            }
            else
            {
                p.py = fieldCenter.y + halfExt.y;
            }

            // ------------------------------------------------------
            // 初期速度
            // ------------------------------------------------------
            const float fall = RandRange(0.8f, 1.2f) * std::max(mDesc.gravity, 0.01f);

            p.vx = mDesc.wind.x + RandRange(-0.15f, 0.15f);
            p.vy = -fall;
            p.vz = mDesc.wind.z + RandRange(-0.15f, 0.15f);

            // ------------------------------------------------------
            // 寿命
            // ------------------------------------------------------
            if (warmStart)
            {
                p.life = RandRange(0.0f, std::max(mDesc.particleLife, 0.1f));
            }
            else
            {
                p.life = 0.0f;
            }
        }
        else
        {
            //======================================================
            // 既存モード（Spark / Water / Smoke）は従来のまま
            //======================================================
            p.px = 0.0f;
            p.py = 0.0f;
            p.pz = 0.0f;

            p.vx = 0.0f;
            p.vy = 0.0f;
            p.vz = 0.0f;

            if (warmStart)
            {
                const float t = (N > 1) ? (static_cast<float>(i) / static_cast<float>(N - 1)) : 0.0f;
                p.life = t * mDesc.particleLife;

                p.vx = u11(rng) * 0.05f;
                p.vy = u11(rng) * 0.05f;
                p.vz = u11(rng) * 0.05f;
            }
            else
            {
                p.life = mDesc.particleLife + 1.0f;
            }
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

    mPingPong = false;
}

} // namespace toy
