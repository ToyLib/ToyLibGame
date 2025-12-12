// Graphics/Effect/GPUParticleComponent.cpp
#include "Graphics/Effect/GPUParticleComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"

#include "glad/glad.h"
#include <vector>
#include <cstddef> // offsetof

namespace toy {

//------------------------------------------------------------
// GPU particle layout (Transform Feedback interleaved)
//   vec3 pos + vec3 vel + float life  => 7 floats = 28 bytes
//------------------------------------------------------------
struct GPUParticle
{
    float px, py, pz;   // position (bytes  0..11)
    float vx, vy, vz;   // velocity (bytes 12..23)
    float life;         // life     (bytes 24..27)
};
static_assert(sizeof(GPUParticle) == sizeof(float) * 7,
              "GPUParticle layout must be 7 floats");

//------------------------------------------------------------
// Quad geometry (SpriteVerts互換: pos3 + normal3 + uv2)
//   - 描画では pos + uv のみ使用（normal は未使用）
//------------------------------------------------------------
static const float kQuadVerts[4 * 8] =
{
    // x,    y,    z,    nx, ny, nz,    u,   v
    -0.5f,  0.5f, 0.0f,  0,  0,  0,   0.0f, 0.0f, // top left
     0.5f,  0.5f, 0.0f,  0,  0,  0,   1.0f, 0.0f, // top right
     0.5f, -0.5f, 0.0f,  0,  0,  0,   1.0f, 1.0f, // bottom right
    -0.5f, -0.5f, 0.0f,  0,  0,  0,   0.0f, 1.0f  // bottom left
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
, mDrawOrder(drawOrder)
, mSpawnProb(0.05f)
, mSpawnRampSec(0.6f)
{
    mLayer = VisualLayer::Effect3D;

    auto* r = GetOwner()->GetApp()->GetRenderer();

    // - ParticleUpdate: Transform Feedback update (VS only)
    // - ParticleGPU   : Render (VS+FS)
    mUpdateShader = r->GetShader("ParticleUpdate");
    mRenderShader = r->GetShader("ParticleGPU");
}

GPUParticleComponent::~GPUParticleComponent()
{
    ReleaseGL();
}

//==============================================================
// public API
//==============================================================
void GPUParticleComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = tex;
}

//--------------------------------------------------------------
// CreateParticles
//   pos は「Actor ローカル空間での発生オフセット」
//   実際のワールド座標は UpdateParticlesGPU() 内で
//   Actor の WorldTransform を掛けて算出し、GPUに渡す。
//--------------------------------------------------------------
void GPUParticleComponent::CreateParticles(
    Vector3 pos,
    unsigned int num,
    float life,
    float partLife,
    float size,
    ParticleMode mode)
{
    // Actor に対するローカルオフセット（ワールドではない）
    mEmitterPos = pos;

    mIsVisible  = true;

    mMaxParticles     = num;
    mComponentLifeMax = life;     // 0 = infinite
    mComponentLife    = 0.0f;

    mParticleLifeMax  = (partLife > 0.0f) ? partLife : 1.0f;
    mSize             = (size > 0.0f) ? size : 1.0f;
    mMode             = mode;

    // 旧挙動に合わせた初期プリセット
    if (mMode == P_WATER)
    {
        mIsBlendAdd = false;
        mGravity = 9.8f;
        mLift = 0.0f;
    }
    else if (mMode == P_SMOKE)
    {
        mIsBlendAdd = false;
        mGravity = 0.0f;
        mLift = 2.0f;
    }
    else // Spark
    {
        mIsBlendAdd = true;
        mGravity = 0.0f;
        mLift = 0.0f;
    }

    InitIfNeeded();
}

void GPUParticleComponent::Update(float deltaTime)
{
    if (!mIsVisible) return;

    // コンポーネント寿命
    if (mComponentLifeMax > 0.0f)
    {
        mComponentLife += deltaTime;
        if (mComponentLife >= mComponentLifeMax)
        {
            mIsVisible = false;
            return;
        }
    }

    if (!mInitialized || mMaxParticles == 0) return;

    UpdateParticlesGPU(deltaTime);
}

void GPUParticleComponent::Draw()
{
    if (!mIsVisible || !mInitialized || mMaxParticles == 0) return;
    if (!mTexture || !mRenderShader) return;

    //----------------------------------------------------------
    // Blend
    //----------------------------------------------------------
    glEnable(GL_BLEND);
    if (mIsBlendAdd)
        glBlendFunc(GL_ONE, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    Matrix4 viewProj = view * proj;

    //----------------------------------------------------------
    // Billboard basis (camera right / up)
    //   ※ ToyLib の view 行列から抽出（Normalize推奨）
    //----------------------------------------------------------
    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();

    //----------------------------------------------------------
    // Render shader uniforms
    //----------------------------------------------------------
    mRenderShader->SetActive();
    mRenderShader->SetVectorUniform("uCameraRight", camRight);
    mRenderShader->SetVectorUniform("uCameraUp", camUp);
    mRenderShader->SetMatrixUniform("uViewProj", viewProj);
    mRenderShader->SetFloatUniform("uLifeMax", mParticleLifeMax);
    mRenderShader->SetFloatUniform("uSize", mSize);
    mRenderShader->SetFloatUniform("uLifeMax", mParticleLifeMax);

    // texture
    mTexture->SetActive(0);
    mRenderShader->SetTextureUniform("uTexture", 0);

    //----------------------------------------------------------
    // Draw instanced
    //   instance data は CurrentSrcVBO()（更新済みバッファ）
    //----------------------------------------------------------
    glBindVertexArray(mRenderVAO);

    const unsigned int src = CurrentSrcVBO();
    BindInstanceAttributes(src);

    glDrawElementsInstanced(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr,
        (GLsizei)mMaxParticles
    );

    renderer->AddDrawCall();
    renderer->AddDrawObject();

    // restore (必要なら ToyLib の規約に合わせて戻す)
    if (mIsBlendAdd)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==============================================================
// init / release
//==============================================================
void GPUParticleComponent::InitIfNeeded()
{
    if (mInitialized) return;
    if (mMaxParticles == 0) return;

    InitQuadGeometry();
    InitParticleBuffers();
    InitUpdateVAO();
    InitRenderVAO();

    mInitialized = true;
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
}

//--------------------------------------------------------------
// Quad (geometry) VBO/IBO
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
void GPUParticleComponent::InitParticleBuffers()
{
    std::vector<GPUParticle> init(mMaxParticles);

    // 初回Updateで必ずリスポーンさせるため、life を寿命超えにする
    for (auto& p : init)
    {
        p.px = p.py = p.pz = 0.0f;
        p.vx = p.vy = p.vz = 0.0f;
        p.life = mParticleLifeMax + 1.0f;
        
    }

    glGenBuffers(1, &mParticleVBO_A);
    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_A);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glGenBuffers(1, &mParticleVBO_B);
    glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO_B);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GPUParticle) * init.size(), init.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mPingPong = false;
}

//--------------------------------------------------------------
// Update VAO (attributes 0,1,2 from src particle VBO)
//--------------------------------------------------------------
void GPUParticleComponent::InitUpdateVAO()
{
    glGenVertexArrays(1, &mUpdateVAO);
    glBindVertexArray(mUpdateVAO);

    // ping-pong のため、ここで src VBO は固定しない。
    // BindUpdateAttributes() で毎フレーム差し替える。

    glBindVertexArray(0);
}

//--------------------------------------------------------------
// Render VAO
//   quad attrs:   0 (pos), 1 (uv)
//   instance attrs: 3 (pos), 4 (life) ※毎フレームバインド
//--------------------------------------------------------------
void GPUParticleComponent::InitRenderVAO()
{
    glGenVertexArrays(1, &mRenderVAO);
    glBindVertexArray(mRenderVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mQuadIBO);

    const GLsizei stride = sizeof(float) * 8;

    // location 0: aQuadPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // location 1: aUV (vec2)  (uv is at float[6])
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

    //----------------------------------------------------------
    // Emitter: Actor ローカル → ワールドへ変換
    //----------------------------------------------------------
    const Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    const Vector3 emitterWorld = Vector3::Transform(mEmitterPos, actorWorld);

    // Bind update VAO + src attributes
    glBindVertexArray(mUpdateVAO);
    BindUpdateAttributes(src);

    //----------------------------------------------------------
    // Update shader uniforms
    //   ※ Transform Feedback varying は link 前に設定済みである必要あり
    //----------------------------------------------------------
    mUpdateShader->SetActive();
    mUpdateShader->SetFloatUniform("uDeltaTime", dt);
    mUpdateShader->SetFloatUniform("uLifeMax", mParticleLifeMax);
    mUpdateShader->SetVectorUniform("uEmitterPos", emitterWorld);
    mUpdateShader->SetIntUniform("uMode", (int)mMode);
    mUpdateShader->SetFloatUniform("uGravity", mGravity);
    mUpdateShader->SetFloatUniform("uLift", mLift);
    mUpdateShader->SetFloatUniform("uSpread", mSpread);
    mUpdateShader->SetFloatUniform("uSpawnProb", mSpawnProb);
    mUpdateShader->SetFloatUniform("uSpawnRampSec", mSpawnRampSec);
    
    // TF: write to dst
    glEnable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, dst);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mMaxParticles);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glDisable(GL_RASTERIZER_DISCARD);

    glBindVertexArray(0);

    // swap
    mPingPong = !mPingPong;
}

//--------------------------------------------------------------
// bind attributes for update VAO (0:pos, 1:vel, 2:life)
//--------------------------------------------------------------
void GPUParticleComponent::BindUpdateAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);

    const GLsizei stride = sizeof(GPUParticle);

    // location 0: aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, px));

    // location 1: aVel
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, vx));

    // location 2: aLife
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, life));
}

//--------------------------------------------------------------
// bind instance attributes for render VAO (3:pos, 4:life)
//--------------------------------------------------------------
void GPUParticleComponent::BindInstanceAttributes(unsigned int srcVBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, srcVBO);

    const GLsizei stride = sizeof(GPUParticle);

    // location 3: iPos
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, px));
    glVertexAttribDivisor(3, 1);

    // location 4: iLife
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

} // namespace toy
