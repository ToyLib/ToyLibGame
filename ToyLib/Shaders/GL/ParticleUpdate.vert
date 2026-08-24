#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - SceneUBO
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLSceneUBO)
//   Binding    : Render/GL/GLBindingPoints.h (kSceneUBOBinding = 1)
//======================================================================

struct ObjectData
{
    mat4 world;
};

struct MaterialData
{
    sampler2D baseMap;

    vec3 baseColor;
    bool useTexture;

    bool toon;

    bool overrideEnabled;
    vec3 overrideColor;

    float specPower;
};

// GL 4.1 は layout(binding=N) 不可 → C++ 側で glUniformBlockBinding 設定済み
// row_major: ToyLib は v*M（行ベクトル×行列）規約
layout(std140, row_major) uniform SceneUBO
{
    mat4  viewProj;
    vec4  cameraAndSun;      // xyz=cameraPos, w=sunIntensity
    vec4  ambientLight;      // xyz=ambient
    vec4  dirDirection;      // xyz=dirLight.direction
    vec4  dirDiffuse;        // xyz=dirLight.diffuse
    vec4  dirSpecular;       // xyz=dirLight.specular
    int   numPointLights;
    int   _plPad0, _plPad1, _plPad2;
    vec4  plPosRadius[8];      // xyz=position, w=radius
    vec4  plColorIntensity[8]; // xyz=color, w=intensity
    vec4  plAtten[8];          // x=constant, y=linear, z=quadratic
    vec4  fogColor;            // xyz=fog.color
    vec4  fogParams;           // x=minDist, y=maxDist
    mat4  lightViewProj0;
    mat4  lightViewProj1;
    vec4  shadowParams;        // x=cascadeSplit0, y=cascadeBlend, z=shadowBias
    ivec4 shadowFlags;         // x=shadowEnable
} uScene;

uniform sampler2DShadow uShadowMap0;
uniform sampler2DShadow uShadowMap1;

uniform ObjectData   uObject;
uniform MaterialData uMaterial;

//======================================================================
// ParticleUpdate.vert
// ---------------------------------------------------------------------
// Transform Feedback 用の GPU particle update shader
//
// 入力:
//   aPos  : 現在位置
//   aVel  : 現在速度
//   aLife : 経過寿命
//
// 出力:
//   tfPos / tfVel / tfLife
//
// mode:
//   0 = Spark
//   1 = Water
//   2 = Smoke
//   3 = SnowField
//======================================================================

//----------------------------------------------------------------------
// Input attributes
//----------------------------------------------------------------------
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec3  aVel;
layout(location = 2) in float aLife;

//----------------------------------------------------------------------
// Transform Feedback outputs
//----------------------------------------------------------------------
out vec3  tfPos;
out vec3  tfVel;
out float tfLife;

//----------------------------------------------------------------------
// Common update uniforms
//----------------------------------------------------------------------
uniform float uDeltaTime;
uniform float uTime;

uniform float uLifeMax;
uniform vec3  uEmitterPos;

uniform int   uMode;

uniform float uGravity;
uniform float uLift;
uniform float uSpread;

uniform float uSpawnRate;
uniform float uSpawnRampSec;

//----------------------------------------------------------------------
// SnowField uniforms
//----------------------------------------------------------------------
uniform vec3 uFieldCenter;
uniform vec3 uFieldExtent;
uniform vec3 uWind;
uniform int  uRespawnTop;

//----------------------------------------------------------------------
// Hash helpers
//----------------------------------------------------------------------
float hash11(float n)
{
    return fract(sin(n) * 43758.5453123);
}

vec3 hash31(float n)
{
    return vec3(
        hash11(n + 1.0),
        hash11(n + 2.0),
        hash11(n + 3.0)
    );
}

//----------------------------------------------------------------------
// Random unit-ish direction from particle id and time
//----------------------------------------------------------------------
vec3 randomDir(int id, float t)
{
    float n = float(id) * 12.9898 + t * 78.233;
    vec3 r = hash31(n) * 2.0 - 1.0;

    float len2 = max(dot(r, r), 1e-6);
    return r * inversesqrt(len2);
}

//----------------------------------------------------------------------
// SnowField spawn position
//  - XZ: field 全体にばらまく
//  - Y : topOnly 時は上側帯域から再配置
//----------------------------------------------------------------------
vec3 randomFieldPos(int id, float t, vec3 center, vec3 extent, bool topOnly)
{
    float n = float(id) * 17.13 + floor(t * 60.0) * 0.37;

    vec3 r = hash31(n);
    vec3 halfExt = extent * 0.5;

    vec3 p;
    p.x = center.x + mix(-halfExt.x, halfExt.x, r.x);
    p.z = center.z + mix(-halfExt.z, halfExt.z, r.z);

    if (topOnly)
    {
        float topBand = halfExt.y * 0.65;
        p.y = center.y + halfExt.y - topBand * r.y;
    }
    else
    {
        p.y = center.y + mix(-halfExt.y, halfExt.y, r.y);
    }

    return p;
}

//----------------------------------------------------------------------
// Respawn probability gate for Spark / Water / Smoke
//----------------------------------------------------------------------
float spawnGate(int id)
{
    float ramp = (uSpawnRampSec <= 0.0) ? 1.0 : clamp(uTime / uSpawnRampSec, 0.0, 1.0);

    float p = 1.0 - exp(-max(uSpawnRate, 0.0) * uDeltaTime);
    p *= ramp;

    float r = hash11(float(id) * 3.17 + floor(uTime * 60.0) * 0.77);
    return (r < p) ? 1.0 : 0.0;
}

void main()
{
    // TF 専用だが明示しておく
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);

    int id = gl_VertexID;

    vec3  pos  = aPos;
    vec3  vel  = aVel;
    float life = aLife;

    bool dead = (life >= uLifeMax);

    //------------------------------------------------------------------
    // Dead particle handling
    //------------------------------------------------------------------
    if (dead)
    {
        if (uMode == 3)
        {
            // SnowField:
            // field 内へ再配置して、風＋落下速度を与える
            pos = randomFieldPos(id, uTime, uFieldCenter, uFieldExtent, (uRespawnTop != 0));

            vec3 dir = randomDir(id, uTime);
            vel.x = uWind.x + dir.x * 0.12;
            vel.y = -(0.35 + abs(dir.y) * 0.55);
            vel.z = uWind.z + dir.z * 0.12;

            life = 0.0;
        }
        else if (uSpawnRate > 0.0 && spawnGate(id) > 0.5)
        {
            // Spark / Water / Smoke:
            // emitter 位置から respawn
            life = 0.0;
            pos  = uEmitterPos;

            vec3 dir = randomDir(id, uTime);

            if (uMode == 1)
            {
                // Water
                dir.y = -abs(dir.y) * 0.6 - 0.4;
            }
            else if (uMode == 2)
            {
                // Smoke
                dir.y =  abs(dir.y) * 0.6 + 0.4;
            }

            vel = dir * uSpread;
        }
        else
        {
            // dead のまま維持
            life = uLifeMax + 1.0;
            vel  = vec3(0.0);
        }
    }
    //------------------------------------------------------------------
    // Alive particle update
    //------------------------------------------------------------------
    else
    {
        if (uMode == 1)
        {
            // Water
            vel.y -= uGravity * uDeltaTime;
        }
        else if (uMode == 2)
        {
            // Smoke
            vel.y += uLift * uDeltaTime;
        }
        else if (uMode == 3)
        {
            // SnowField
            vel.x = mix(vel.x, uWind.x, 0.02);
            vel.z = mix(vel.z, uWind.z, 0.02);
            vel.y -= uGravity * uDeltaTime;
        }

        pos  += vel * uDeltaTime;
        life += uDeltaTime;

        if (uMode == 3)
        {
            vec3 halfExt = uFieldExtent * 0.5;

            bool outX = abs(pos.x - uFieldCenter.x) > halfExt.x;
            bool outZ = abs(pos.z - uFieldCenter.z) > halfExt.z;
            bool outY = (pos.y < (uFieldCenter.y - halfExt.y));

            if (outX || outZ || outY)
            {
                pos = randomFieldPos(id, uTime, uFieldCenter, uFieldExtent, (uRespawnTop != 0));

                vec3 dir = randomDir(id, uTime);
                vel.x = uWind.x + dir.x * 0.12;
                vel.y = -(0.35 + abs(dir.y) * 0.55);
                vel.z = uWind.z + dir.z * 0.12;

                life = 0.0;
            }
        }
        else
        {
            if (life >= uLifeMax)
            {
                life = uLifeMax + 1.0;
            }
        }
    }

    //------------------------------------------------------------------
    // Transform Feedback writeback
    //------------------------------------------------------------------
    tfPos  = pos;
    tfVel  = vel;
    tfLife = life;
}
