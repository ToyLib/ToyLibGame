#version 410 core

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
// 0:Spark 1:Water 2:Smoke
uniform int   uMode;

// 力・拡散
uniform float uGravity;     // Water 用（下向き加速度）
uniform float uLift;        // Smoke 用（上向き加速度）
uniform float uSpread;      // 初速スケール（dir * spread）

// スポーン制御
// uSpawnRate : 1秒あたりのリスポーン試行率（0 = respawn しない）
// uSpawnRampSec : ランプ時間（0 = 即 100%）
uniform float uSpawnRate;
uniform float uSpawnRampSec;

//==============================================================
// 乱数（決定性のある hash。テクスチャ不要）
//----------------------------------------------------------------------
// ・同じ id と同じ時間スライスなら同じ乱数になる
// ・GPU 上で軽量に “それっぽいランダム” を作る用途
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

    // r がほぼゼロになるのを避けて正規化
    float len2 = max(dot(r, r), 1e-6);
    return r * inversesqrt(len2);
}

//--------------------------------------------------------------
// spawnGate
// ・「uSpawnRate（/sec）」を「フレームごとの発生確率」に変換し、
//   ハッシュ乱数で gate を通った粒だけ respawn させる。
//
// ポイント：
// ・rate を dt に応じて自然に変換したいので、ポアソン過程の近似
//   p = 1 - exp(-rate * dt) を使う（dt が変動しても挙動が安定）
// ・さらに ramp（0..1）を掛けて徐々に発生量を増やせる
//--------------------------------------------------------------
float spawnGate(int id)
{
    // ramp: 0..1（uSpawnRampSec <= 0 なら常に 1）
    float ramp = (uSpawnRampSec <= 0.0) ? 1.0 : clamp(uTime / uSpawnRampSec, 0.0, 1.0);

    // per-second chance -> per-frame probability
    float p = 1.0 - exp(-max(uSpawnRate, 0.0) * uDeltaTime);
    p *= ramp;

    // 60Hz 想定で時間をスライスして、フレームごとに乱数が変わるようにする
    // ※ floor(uTime * 60) なので dt が多少揺れても “1/60秒単位” で更新される
    float r = hash11(float(id) * 3.17 + floor(uTime * 60.0) * 0.77);

    return (r < p) ? 1.0 : 0.0;
}

void main()
{
    // Transform Feedback は gl_VertexID を粒IDとして扱える（0..N-1）
    int id = gl_VertexID;

    // 現在状態（src VBO からコピー）
    vec3 pos  = aPos;
    vec3 vel  = aVel;
    float life = aLife;

    // dead 判定（life >= lifeMax）
    bool dead = (life >= uLifeMax);

    if (dead)
    {
        //----------------------------------------------------------
        // dead 粒：respawn するか、死体のまま維持するか
        //----------------------------------------------------------
        if (uSpawnRate > 0.0 && spawnGate(id) > 0.5)
        {
            // respawn
            life = 0.0;
            pos  = uEmitterPos;

            // 方向は決定性乱数で生成
            vec3 dir = randomDir(id, uTime);

            // mode ごとの “らしさ” を方向にバイアスとして入れる
            if (uMode == 1)
            {
                // Water：下向きに寄せる
                dir.y = -abs(dir.y) * 0.6 - 0.4;
            }
            else if (uMode == 2)
            {
                // Smoke：上向きに寄せる
                dir.y =  abs(dir.y) * 0.6 + 0.4;
            }

            // 初速
            vel = dir * uSpread;
        }
        else
        {
            // keep dead：寿命を lifeMax より大きくして明確に “死” を表す
            life = uLifeMax + 1.0;
            vel  = vec3(0.0);

            // pos は維持（意味はないがデバッグ時に追跡しやすい）
            pos = pos;
        }
    }
    else
    {
        //----------------------------------------------------------
        // alive 粒：力を加えて積分 → 寿命を進める
        //----------------------------------------------------------
        if (uMode == 1)
        {
            // Water：重力（下向き）
            vel.y -= uGravity * uDeltaTime;
        }
        else if (uMode == 2)
        {
            // Smoke：揚力（上向き）
            vel.y += uLift * uDeltaTime;
        }

        // 位置更新（単純オイラー）
        pos  += vel * uDeltaTime;
        life += uDeltaTime;

        // 寿命到達で dead 扱い（Update/Render 両方で共通判定にできる）
        if (life >= uLifeMax)
        {
            life = uLifeMax + 1.0;
        }
    }

    //--------------------------------------------------------------
    // Transform Feedback 出力（dst VBO に書き戻される）
    //--------------------------------------------------------------
    tfPos  = pos;
    tfVel  = vel;
    tfLife = life;
}
