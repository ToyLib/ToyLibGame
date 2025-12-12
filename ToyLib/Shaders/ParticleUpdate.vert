#version 410 core

//============================================================
// ParticleUpdate.vert (Transform Feedback Update)
//  - Input : aPos, aVel, aLife
//  - Output: tfPos, tfVel, tfLife   (INTERLEAVED TF)
//  - RasterizerDiscard 前提（描画しない）
//
//  目的：発生タイミングの“塊”を崩す
//   * gl_VertexID を乱数 seed に必ず混ぜる（粒子ごとにユニーク）
//   * deadTime に粒子固有の personalDelay を入れて、復活の到達時刻を分散
//   * life依存確率（ramp）で「死んですぐ湧かない→徐々に湧く」を維持
//============================================================

//------------------------------------------------------------
// Attributes (from particle buffer)
//------------------------------------------------------------
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec3  aVel;
layout(location = 2) in float aLife;

//------------------------------------------------------------
// Transform Feedback outputs (INTERLEAVED)
//------------------------------------------------------------
out vec3  tfPos;
out vec3  tfVel;
out float tfLife;

//------------------------------------------------------------
// Uniforms
//------------------------------------------------------------
uniform float uDeltaTime;      // dt
uniform float uLifeMax;        // particle lifetime (sec)
uniform vec3  uEmitterPos;     // emitter world position（CPUでActor反映済み）
uniform int   uMode;           // 0:Spark 1:Water 2:Smoke
uniform float uGravity;        // Water: gravity strength (down)
uniform float uLift;           // Smoke: lift strength (up)
uniform float uSpread;         // initial speed scale / spread

// Spawn control
uniform float uSpawnProb;      // 0..1 base probability
uniform float uSpawnRampSec;   // seconds to reach full prob after death

//------------------------------------------------------------
// Mode constants (match C++ enum order)
//------------------------------------------------------------
const int P_SPARK = 0;
const int P_WATER = 1;
const int P_SMOKE = 2;

//============================================================
// Hash / random (GLSL 4.10 friendly)
//============================================================
float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash31(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

vec3 randDir(vec3 seed)
{
    float rx = hash31(seed + vec3(1.0, 2.0, 3.0)) * 2.0 - 1.0;
    float ry = hash31(seed + vec3(4.0, 5.0, 6.0)) * 2.0 - 1.0;
    float rz = hash31(seed + vec3(7.0, 8.0, 9.0)) * 2.0 - 1.0;
    vec3 v = vec3(rx, ry, rz);
    float len2 = max(dot(v, v), 1e-6);
    return v * inversesqrt(len2);
}

//============================================================
// Respawn (mode-dependent)
//============================================================
void respawn(in vec3 seed, out vec3 pos, out vec3 vel, out float life)
{
    pos  = uEmitterPos;
    life = 0.0;

    vec3 d = randDir(seed);

    if (uMode == P_WATER)
    {
        // mostly downward + small lateral
        d.y = -abs(d.y);
        d = normalize(d);

        vel = d * uSpread;
        vel.x *= 0.25;
        vel.z *= 0.25;
    }
    else if (uMode == P_SMOKE)
    {
        // upward bias
        d.y = abs(d.y);
        d = normalize(d);

        vel = d * uSpread;
        vel.y *= 0.5;
    }
    else
    {
        // omni-directional spark
        vel = d * uSpread;
    }
}

//============================================================
// Main update
//============================================================
void main()
{
    vec3  pos  = aPos;
    vec3  vel  = aVel;
    float life = aLife;

    // 粒子ごとに必ずユニーク（TF update は glDrawArrays なので有効）
    float id = float(gl_VertexID);

    // life update（alive/dead 共通）
    life += uDeltaTime;

    //========================================================
    // Dead particle -> Life-dependent probabilistic respawn
    //========================================================
    if (life > uLifeMax)
    {
        float deadTime = life - uLifeMax;

        // 粒子固有の復活ディレイ（0..uSpawnRampSec）
        float rampSec = max(uSpawnRampSec, 1e-3);
        float personalDelay = hash11(id * 17.23) * rampSec;

        // 「deadTime が personalDelay を越えてから」確率が上がる
        float spawnT = clamp((deadTime - personalDelay) / rampSec, 0.0, 1.0);

        // ベース確率 * ランプ
        float spawnProb = uSpawnProb * spawnT;

        // 乱数seed：idを必ず混ぜる（これが塊潰しの本体）
        vec3 seed = vec3(id, id * 1.37, id * 9.21) + vec3(life, life * 2.0, life * 3.0);

        // 微ジッター（粒子ごとの微妙な揺らぎ）
        float jitter = 0.6 + 0.4 * hash11(id * 3.11 + life * 13.7);
        spawnProb *= jitter;

        float r = hash31(seed);

        if (r < spawnProb)
        {
            respawn(seed, pos, vel, life);
        }
        // else: dead のまま待機（life が増え続け、spawnProb も増える）
    }
    else
    {
        //====================================================
        // Alive particle simulation
        //====================================================
        if (uMode == P_WATER)
        {
            vel.y -= uGravity * uDeltaTime;
        }
        else if (uMode == P_SMOKE)
        {
            vel.y += uLift * uDeltaTime;

            // subtle horizontal drift (id-based seed)
            vec3 seed = pos + vec3(life) + vec3(id, id * 0.5, id * 0.25);
            vec3 drift = randDir(seed) * 0.2;
            drift.y = 0.0;
            vel += drift * uDeltaTime;
        }
        else
        {
            // spark damping (optional)
            vel *= 0.98;
        }

        pos += vel * uDeltaTime;
    }

    //========================================================
    // Transform Feedback outputs
    //========================================================
    tfPos  = pos;
    tfVel  = vel;
    tfLife = life;

    // RasterizerDiscard 前提だが必須
    gl_Position = vec4(pos, 1.0);
}
