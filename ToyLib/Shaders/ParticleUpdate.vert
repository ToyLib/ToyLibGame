// shaders/ParticleUpdate.vert
#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aVel;
layout(location = 2) in float aLife;

// Transform Feedback outputs (interleaved)
out vec3 tfPos;
out vec3 tfVel;
out float tfLife;

uniform float uDeltaTime;
uniform float uTime;

uniform float uLifeMax;
uniform vec3  uEmitterPos;

uniform int   uMode;     // 0:Spark 1:Water 2:Smoke
uniform float uGravity;
uniform float uLift;
uniform float uSpread;

uniform float uSpawnRate;     // per-second chance (0 = no respawn)
uniform float uSpawnRampSec;  // seconds to ramp up spawn chance

// ------------------------------------------------------------
// hash helpers (deterministic, no textures)
// ------------------------------------------------------------
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

vec3 randomDir(int id, float t)
{
    float n = float(id) * 12.9898 + t * 78.233;
    vec3 r = hash31(n) * 2.0 - 1.0;
    // avoid zero
    float len2 = max(dot(r, r), 1e-6);
    return r * inversesqrt(len2);
}

float spawnGate(int id)
{
    // ramp: 0..1
    float ramp = (uSpawnRampSec <= 0.0) ? 1.0 : clamp(uTime / uSpawnRampSec, 0.0, 1.0);

    // convert per-second chance to per-frame probability:
    // p = 1 - exp(-rate * dt)
    float p = 1.0 - exp(-max(uSpawnRate, 0.0) * uDeltaTime);
    p *= ramp;

    float r = hash11(float(id) * 3.17 + floor(uTime * 60.0) * 0.77);
    return (r < p) ? 1.0 : 0.0;
}

void main()
{
    int id = gl_VertexID;

    vec3 pos = aPos;
    vec3 vel = aVel;
    float life = aLife;

    // dead if life >= uLifeMax
    bool dead = (life >= uLifeMax);

    if (dead)
    {
        // respawn only if spawn enabled and gate passes
        if (uSpawnRate > 0.0 && spawnGate(id) > 0.5)
        {
            life = 0.0;
            pos = uEmitterPos;

            vec3 dir = randomDir(id, uTime);

            if (uMode == 1)
            {
                // Water: bias downward
                dir.y = -abs(dir.y) * 0.6 - 0.4;
            }
            else if (uMode == 2)
            {
                // Smoke: bias upward
                dir.y = abs(dir.y) * 0.6 + 0.4;
            }

            vel = dir * uSpread;
        }
        else
        {
            // keep dead
            life = uLifeMax + 1.0;
            vel = vec3(0.0);
            pos = pos; // unchanged
        }
    }
    else
    {
        // integrate forces
        if (uMode == 1)
        {
            // Water: gravity
            vel.y -= uGravity * uDeltaTime;
        }
        else if (uMode == 2)
        {
            // Smoke: lift (simple)
            vel.y += uLift * uDeltaTime;
        }

        // integrate
        pos += vel * uDeltaTime;
        life += uDeltaTime;

        // if reached end, mark dead (Render/Update both treat as dead)
        if (life >= uLifeMax)
        {
            life = uLifeMax + 1.0;
        }
    }

    tfPos = pos;
    tfVel = vel;
    tfLife = life;
}
