// Graphics/Effect/GPUParticleComponent.h
#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy {

class Texture;
class Shader;

class GPUParticleComponent : public VisualComponent
{
public:
    // 旧 ParticleComponent と同じモード名
    enum ParticleMode
    {
        P_SPARK = 0, // 爆発：拡散 + 減衰
        P_WATER = 1, // 水：重力落下
        P_SMOKE = 2  // 煙：上昇 + 減衰
    };

    GPUParticleComponent(class Actor* owner, int drawOrder = 20);
    ~GPUParticleComponent();

    void Update(float deltaTime) override;
    void Draw() override;

    void SetTexture(std::shared_ptr<Texture> tex) override;

    // 旧と同等の入口（互換優先）
    void CreateParticles(
        Vector3 pos,
        unsigned int num,
        float life,       // コンポーネント寿命（0で無限）
        float partLife,   // 粒寿命
        float size,       // 粒サイズ
        ParticleMode mode
    );

    void SetAddBlend(bool b) { mIsBlendAdd = b; }
    void SetSpeed(float speed) { mSpread = speed; } // 旧 mPartSpeed 相当

    int GetDrawOrder() const { return mDrawOrder; }

private:
    // --- GL init/release ---
    void InitIfNeeded();
    void ReleaseGL();

    void InitQuadGeometry();       // quad VBO/IBO + VAO (render)
    void InitParticleBuffers();    // ping-pong particle VBO
    void InitUpdateVAO();          // update VAO (attributes 0,1,2)
    void InitRenderVAO();          // render VAO (quad attrs 0,1)

    void UpdateParticlesGPU(float dt);

    unsigned int CurrentSrcVBO() const;
    unsigned int CurrentDstVBO() const;

    void BindUpdateAttributes(unsigned int srcVBO);
    void BindInstanceAttributes(unsigned int srcVBO);

private:
    // --- old-like params ---
    std::shared_ptr<Texture> mTexture;
    int mDrawOrder = 20;

    Vector3 mEmitterPos = Vector3::Zero;
    unsigned int mMaxParticles = 0;

    float mComponentLifeMax = 0.0f; // life (0 = infinite)
    float mComponentLife = 0.0f;

    float mParticleLifeMax = 1.0f;  // partLife
    float mSize = 1.0f;             // size
    float mSpread = 2.0f;           // speed/spread (旧 mPartSpeed)
    ParticleMode mMode = P_SPARK;
    bool mIsBlendAdd = true;

    // mode params
    float mGravity = 9.8f; // Water
    float mLift    = 2.0f; // Smoke

    // --- shaders ---
    std::shared_ptr<Shader> mUpdateShader; // TF update (VS only)
    std::shared_ptr<Shader> mRenderShader; // render (VS+FS)

    // --- GL objects ---
    bool mInitialized = false;
    bool mPingPong = false;

    // quad
    unsigned int mQuadVBO = 0;
    unsigned int mQuadIBO = 0;

    // particle ping-pong buffers
    unsigned int mParticleVBO_A = 0;
    unsigned int mParticleVBO_B = 0;

    // VAOs
    unsigned int mUpdateVAO = 0;
    unsigned int mRenderVAO = 0;
    
    float mSpawnProb;
    float mSpawnRampSec;
};

} // namespace toy
