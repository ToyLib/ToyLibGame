#version 410 core

//======================================================================
// ToyLib Uniform Contract (v1) - generated
//   See Render/GL/UniformNamesGL.h
//======================================================================

struct DirLight
{
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight
{
    vec3  position;
    vec3  color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    float radius;
};

struct FogInfo
{
    float maxDist;
    float minDist;
    vec3  color;
};

struct SceneData
{
    mat4 viewProj;

    vec3 cameraPos;

    vec3  ambientLight;
    float sunIntensity;

    DirLight dirLight;

    int        numPointLights;
    PointLight pointLights[8];

    FogInfo fog;

    sampler2DShadow shadowMap0;
    sampler2DShadow shadowMap1;

    mat4  lightViewProj0;
    mat4  lightViewProj1;
    float cascadeSplit0;
    float cascadeBlend;
    float shadowBias;
};

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

// Max palette size must match engine-side upload
const int kMaxPalette = 96;

struct SkinnedData
{
    mat4 matrixPalette[kMaxPalette];
};

uniform SceneData    uScene;
uniform ObjectData   uObject;
uniform MaterialData uMaterial;
uniform SkinnedData  uSkinned;

//==============================================================
// ParticleUpdate.vert
//----------------------------------------------------------------------
// GPU パーティクル更新シェーダ（Transform Feedback 用）
//
// ・入力：粒ごとの状態（pos / vel / life）
// ・出力：更新後の状態を Transform Feedback で VBO に書き戻す
// ・描画はしない（C++ 側で GL_RASTERIZER_DISCARD を有効にする）
//
// life の扱い：
// ・life は「経過秒」
// ・life >= uLifeMax なら dead とみなす
// ・dead 粒は spawnGate を通った時だけ respawn される
//
// mode:
//   0: Spark
//   1: Water
//   2: Smoke
//   3: SnowField
//==============================================================

//==============================================================
// Inputs（src VBO から）
//==============================================================
layout(location = 0) in vec3  aPos;   // 現在位置
layout(location = 1) in vec3  aVel;   // 現在速度
layout(location = 2) in float aLife;  // 経過寿命（秒）

//==============================================================
// Transform Feedback outputs（dst VBO へ interleaved 書き戻し）
//==============================================================
out vec3  tfPos;
out vec3  tfVel;
out float tfLife;

//==============================================================
// Uniforms（CPU から供給）
//==============================================================
uniform float uDeltaTime;   // dt（秒）
uniform float uTime;        // 経過時間（秒）…スポーンのランプ等に使用

uniform float uLifeMax;     // 粒寿命（秒）
uniform vec3  uEmitterPos;  // リスポーン位置（ワールド座標）

// mode：C++ 側の enum と一致させる
// 0:Spark 1:Water 2:Smoke 3:SnowField
uniform int   uMode;

// 力・拡散
uniform float uGravity;     // Water / SnowField 用（下向き加速度）
uniform float uLift;        // Smoke 用（上向き加速度）
uniform float uSpread;      // 初速スケール（dir * spread）

// スポーン制御
// uSpawnRate : 1秒あたりのリスポーン試行率（0 = respawn しない）
// uSpawnRampSec : ランプ時間（0 = 即 100%）
uniform float uSpawnRate;
uniform float uSpawnRampSec;

// SnowField 用
uniform vec3  uFieldCenter;
uniform vec3  uFieldExtent;
uniform vec3  uWind;
uniform int   uRespawnTop;

//==============================================================
// 乱数（決定性のある hash。テクスチャ不要）
//==============================================================
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

//--------------------------------------------------------------
// randomDir
// 粒ID と時間から「単位ベクトルっぽい」方向を生成する
//--------------------------------------------------------------
vec3 randomDir(int id, float t)
{
    float n = float(id) * 12.9898 + t * 78.233;
    vec3 r = hash31(n) * 2.0 - 1.0;

    float len2 = max(dot(r, r), 1e-6);
    return r * inversesqrt(len2);
}

//--------------------------------------------------------------
// randomFieldPos
// SnowField 用：field 内のランダム位置を返す
//--------------------------------------------------------------
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
        float topBand = halfExt.y * 0.35;
        p.y = center.y + halfExt.y - topBand * r.y;
    }
    else
    {
        p.y = center.y + mix(-halfExt.y, halfExt.y, r.y);
    }

    return p;
}

//--------------------------------------------------------------
// spawnGate
// ・「uSpawnRate（/sec）」を「フレームごとの発生確率」に変換し、
//   ハッシュ乱数で gate を通った粒だけ respawn させる。
//--------------------------------------------------------------
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
    // TF専用でも明示しておく
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);

    int id = gl_VertexID;

    vec3  pos  = aPos;
    vec3  vel  = aVel;
    float life = aLife;

    bool dead = (life >= uLifeMax);

    if (dead)
    {
        if (uMode == 3)
        {
            //======================================================
            // SnowField:
            // dead 粒は field 内に直接再配置
            //======================================================
            pos = randomFieldPos(id, uTime, uFieldCenter, uFieldExtent, (uRespawnTop != 0));

            vec3 dir = randomDir(id, uTime);
            vel.x = uWind.x + dir.x * 0.15;
            vel.y = -(0.6 + abs(dir.y) * 0.4);
            vel.z = uWind.z + dir.z * 0.15;

            life = 0.0;
        }
        else if (uSpawnRate > 0.0 && spawnGate(id) > 0.5)
        {
            //======================================================
            // Spark / Water / Smoke: respawn
            //======================================================
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
            life = uLifeMax + 1.0;
            vel  = vec3(0.0);
        }
    }
    else
    {
        //==========================================================
        // alive 粒：力を加えて積分 → 寿命を進める
        //==========================================================
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
                vel.x = uWind.x + dir.x * 0.15;
                vel.y = -(0.6 + abs(dir.y) * 0.4);
                vel.z = uWind.z + dir.z * 0.15;

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

    tfPos  = pos;
    tfVel  = vel;
    tfLife = life;
}
