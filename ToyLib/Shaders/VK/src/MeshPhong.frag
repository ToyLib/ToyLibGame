#version 450

//==============================================================
// Inputs
//==============================================================
layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

//==============================================================
// Scene UBO (set=0)
//==============================================================
layout(set=0, binding=0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;

    vec4 fogColor;
    vec4 fogParams;     // x=minDist, y=maxDist

    // Shadow
    mat4 shadowVP0;
    mat4 shadowVP1;
    vec4 shadowParams;  // x=split0, y=split1, z=strength
} uScene;

//==============================================================
// Base texture (set=1)
//==============================================================
layout(set=1, binding=0) uniform sampler2D uBaseMap;

//==============================================================
// Shadow maps (set=3)
//==============================================================
layout(set=3, binding=0) uniform sampler2DShadow uShadowMap0;
layout(set=3, binding=1) uniform sampler2DShadow uShadowMap1;

//==============================================================
// Push Constant
//==============================================================
layout(push_constant) uniform PC
{
    mat4 world;
    vec4 baseColor_useTex;
    vec4 misc;           // x=specPower y=toon z=overrideEnabled w=alpha
    vec4 overrideColor;
} pc;

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

//==============================================================
// Lighting
//==============================================================
vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return vec3(0.0);

    if (toon > 0.5)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity =
            pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        return uScene.dirDiffuse.xyz * diffIntensity +
               uScene.dirSpecular.xyz * specIntensity;
    }
    else
    {
        vec3 diffuse =
            uScene.dirDiffuse.xyz * NdotL;

        vec3 specular =
            uScene.dirSpecular.xyz *
            pow(max(dot(reflect(-L, N), V), 0.0), specPower);

        return diffuse + specular;
    }
}

//==============================================================
// Fog (GL再現)
//==============================================================
float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

//==============================================================
// Shadow Sampling
//==============================================================
float SampleShadowMap(sampler2DShadow smp, mat4 shadowVP)
{
    vec4 sc = shadowVP * vec4(vWorldPos, 1.0);

    sc.xyz /= sc.w;

    // clip outside shadow map
    if (sc.x < 0.0 || sc.x > 1.0 ||
        sc.y < 0.0 || sc.y > 1.0 ||
        sc.z < 0.0 || sc.z > 1.0)
    {
        return 1.0;
    }

    return texture(smp, sc.xyz);
}

float ComputeShadow(vec3 L)
{
    float camDist = length(uScene.cameraPos.xyz - vWorldPos);

    float shadow = 1.0;

    if (camDist < uScene.shadowParams.x)
    {
        shadow = SampleShadowMap(uShadowMap0, uScene.shadowVP0);
    }
    else
    {
        shadow = SampleShadowMap(uShadowMap1, uScene.shadowVP1);
    }

    float strength = uScene.shadowParams.z;

    // 1=影なし 0=完全影 → strengthでブレンド
    return mix(1.0, shadow, strength);
}

//==============================================================
// Main
//==============================================================
void main()
{
    float overrideEnabled = pc.misc.z;

    if (overrideEnabled > 0.5)
    {
        outColor = vec4(pc.overrideColor.rgb, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    vec3 dirLightOnly =
        ComputeDirLight(N, V, L, specPower, toon);

    float shadowFactor = ComputeShadow(L);

    vec3 lighting =
        uScene.ambientLight.xyz +
        dirLightOnly * shadowFactor;

    vec4 baseColor;
    float useTex = pc.baseColor_useTex.a;

    if (useTex > 0.5)
        baseColor = texture(uBaseMap, vUV);
    else
        baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);

    vec3 litColor = baseColor.rgb * lighting;

    float dist = length(uScene.cameraPos.xyz - vWorldPos);
    float fogFactor =
        ComputeFogFactor(dist,
                         uScene.fogParams.x,
                         uScene.fogParams.y);

    vec3 finalRgb =
        mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;

    outColor = vec4(finalRgb, baseColor.a * alpha);
}

/*
#version 450

layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;     // xyz
    vec4 ambientLight;  // xyz
    vec4 dirDir;        // xyz
    vec4 dirDiffuse;    // xyz
    vec4 dirSpecular;   // xyz

    // ---- Fog (GL再現) ----
    vec4 fogColor;      // xyz
    vec4 fogParams;     // x=minDist, y=maxDist, z=reserved, w=reserved

    // ---- Shadow (Step3) ----
    mat4 shadowVP0;     // biased lightVP cascade0
    mat4 shadowVP1;     // biased lightVP cascade1
    vec4 shadowParams;  // x=split0, y=split1, z=strength, w=reserved
} uScene;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

layout(push_constant) uniform PC
{
    mat4 world;               // 0
    vec4 baseColor_useTex;    // 64 (rgb + useTex)
    vec4 misc;                // 80 (specPower, toon, overrideEnabled, alpha)
    vec4 overrideColor;       // 96 (rgb)
} pc;

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
    {
        return vec3(0.0);
    }

    if (toon > 0.5)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        return uScene.dirDiffuse.xyz * diffIntensity
             + uScene.dirSpecular.xyz * specIntensity;
    }
    else
    {
        vec3 diffuse  = uScene.dirDiffuse.xyz * NdotL;
        vec3 specular = uScene.dirSpecular.xyz *
                        pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        return diffuse + specular;
    }
}

// GLと同じ Fog 係数
float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

void main()
{
    float overrideEnabled = pc.misc.z;
    if (overrideEnabled > 0.5)
    {
        // Fogは掛けない（GLの「単色描画モード」相当）
        outColor = vec4(pc.overrideColor.rgb, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    vec3 lighting = uScene.ambientLight.xyz + ComputeDirLight(N, V, L, specPower, toon);

    vec4 baseColor;
    float useTex = pc.baseColor_useTex.a;

    if (useTex > 0.5)
    {
        baseColor = texture(uBaseMap, vUV);
    }
    else
    {
        baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);
    }

    // ライティング適用
    vec3 litColor = baseColor.rgb * lighting;

    // Fog（GLと同じ：ワールド距離）
    float dist = length(uScene.cameraPos.xyz - vWorldPos);
    float fogFactor = ComputeFogFactor(dist, uScene.fogParams.x, uScene.fogParams.y);

    vec3 finalRgb = mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;
    outColor = vec4(finalRgb, baseColor.a * alpha);
}
*/
/*
#version 450

layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;     // xyz
    vec4 ambientLight;  // xyz
    vec4 dirDir;        // xyz (direction)
    vec4 dirDiffuse;    // xyz
    vec4 dirSpecular;   // xyz

    // ---- Fog (GL再現) ----
    vec4 fogColor;      // xyz
    vec4 fogParams;     // x=minDist, y=maxDist, z=reserved, w=reserved
} uScene;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

layout(push_constant) uniform PC
{
    mat4 world;               // 0
    vec4 baseColor_useTex;    // 64 (rgb + useTex)
    vec4 misc;                // 80 (specPower, toon, overrideEnabled, alpha)
    vec4 overrideColor;       // 96 (rgb)
} pc;

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
    {
        return vec3(0.0);
    }

    if (toon > 0.5)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        return uScene.dirDiffuse.xyz * diffIntensity
             + uScene.dirSpecular.xyz * specIntensity;
    }
    else
    {
        vec3 diffuse  = uScene.dirDiffuse.xyz * NdotL;
        vec3 specular = uScene.dirSpecular.xyz *
                        pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        return diffuse + specular;
    }
}

// GLと同じ Fog 係数
float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

void main()
{
    float overrideEnabled = pc.misc.z;
    if (overrideEnabled > 0.5)
    {
        // Fogは掛けない（GLの「単色描画モード」相当）
        outColor = vec4(pc.overrideColor.rgb, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    vec3 lighting = uScene.ambientLight.xyz + ComputeDirLight(N, V, L, specPower, toon);

    vec4 baseColor;
    float useTex = pc.baseColor_useTex.a;

    if (useTex > 0.5)
    {
        baseColor = texture(uBaseMap, vUV);
    }
    else
    {
        baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);
    }

    // ライティング適用
    vec3 litColor = baseColor.rgb * lighting;

    // Fog（GLと同じ：ワールド距離）
    float dist = length(uScene.cameraPos.xyz - vWorldPos);
    float fogFactor = ComputeFogFactor(dist, uScene.fogParams.x, uScene.fogParams.y);

    vec3 finalRgb = mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;
    outColor = vec4(finalRgb, baseColor.a * alpha);
}
*/
/*
#version 450

layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

//======================================================================
// SceneUBO (set=0)
//  - 既存 + Fog + Shadow(CSM2)
//  - row-vector 前提の既存実装に合わせ、worldPos * lightVP で扱う
//======================================================================
layout(set=0, binding=0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;     // xyz
    vec4 ambientLight;  // xyz
    vec4 dirDir;        // xyz (direction)
    vec4 dirDiffuse;    // xyz
    vec4 dirSpecular;   // xyz

    // ---- Fog (GL再現) ----
    vec4 fogColor;      // xyz
    vec4 fogParams;     // x=minDist, y=maxDist, z=reserved, w=reserved

    // ---- Shadow (CSM2) ----
    // NOTE:
    //   - lightViewProj0/1 は “Vulkanのdepth range 0..1” に揃っている前提。
    //     (もしGL -1..1 のままなら ShadowPCF 内で proj.xyz = proj.xyz*0.5+0.5 に変更)
    mat4 lightViewProj0;
    mat4 lightViewProj1;

    // x=cascadeSplit0, y=cascadeBlend, z=shadowBias, w=reserved
    vec4 shadowParams;
} uScene;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

// Shadow maps (set=3)
//  - Stage C: 本番描画で sampling を有効化
layout(set=3, binding=0) uniform sampler2DShadow uShadowMap0;
layout(set=3, binding=1) uniform sampler2DShadow uShadowMap1;

layout(push_constant) uniform PC
{
    mat4 world;               // 0
    vec4 baseColor_useTex;    // 64 (rgb + useTex)
    vec4 misc;                // 80 (specPower, toon, overrideEnabled, alpha)
    vec4 overrideColor;       // 96 (rgb)
} pc;

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
    {
        return vec3(0.0);
    }

    if (toon > 0.5)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        return uScene.dirDiffuse.xyz * diffIntensity
            + uScene.dirSpecular.xyz * specIntensity;
    }
    else
    {
        vec3 diffuse  = uScene.dirDiffuse.xyz * NdotL;
        vec3 specular = uScene.dirSpecular.xyz *
                        pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        return diffuse + specular;
    }
}

// GLと同じ Fog 係数
float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

//======================================================================
// Shadow PCF (3x3) : GL版の再現
//  - sampler2DShadow + compare sampler 前提
//  - row-vector 前提: lp = vec4(worldPos,1) * lightVP
//  - Vulkan前提: xyだけ 0..1 化（zは 0..1 前提）
//    ※もし lightVP の z が -1..1 の場合は、proj.xyz を 0..1 化に切り替えてください。
//======================================================================
float ShadowPCF(sampler2DShadow smp, mat4 lightVP, vec3 worldPos, float bias)
{
    vec4 lp = vec4(worldPos, 1.0) * lightVP;
    vec3 proj = lp.xyz / lp.w;

     // ---- Vulkan: xy だけを 0..1 に変換（z は 0..1 前提）
     proj.xy = proj.xy * 0.5 + 0.5;

     // 投影外は影なし
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0)
    {
        return 1.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(smp, 0));

    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;
            sum += texture(smp, vec3(proj.xy + offset, proj.z - bias));
        }
    }

    float lit = sum / 9.0;

    // 「真っ黒にしない」仕様を維持（GL版と同じ）
    return mix(0.5, 1.0, lit);
}

void main()
{
    float overrideEnabled = pc.misc.z;
    if (overrideEnabled > 0.5)
    {
        // Fogは掛けない（GLの「単色描画モード」相当）
        outColor = vec4(pc.overrideColor.rgb, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    vec3 lighting = uScene.ambientLight.xyz + ComputeDirLight(N, V, L, specPower, toon);

    vec4 baseColor;
    float useTex = pc.baseColor_useTex.a;

    if (useTex > 0.5)
    {
        baseColor = texture(uBaseMap, vUV);
    }
    else
    {
        baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);
    }

    // --- Shadow (CSM2: dist based, GL再現) ---
    float dist = length(uScene.cameraPos.xyz - vWorldPos);

    float split0 = uScene.shadowParams.x;
    float blend  = uScene.shadowParams.y;
    float bias   = uScene.shadowParams.z;

    float s0 = ShadowPCF(uShadowMap0, uScene.lightViewProj0, vWorldPos, bias);
    float s1 = ShadowPCF(uShadowMap1, uScene.lightViewProj1, vWorldPos, bias);

    float t = smoothstep(split0 - blend, split0 + blend, dist);
    float shadowFactor = mix(s0, s1, t);

    // ライティング + シャドウ適用（GLと同じ）
    vec3 litColor = baseColor.rgb * (lighting * shadowFactor);

    // --- Fog（GLと同じ：ワールド距離） ---
    float fogFactor = ComputeFogFactor(dist, uScene.fogParams.x, uScene.fogParams.y);
    vec3 finalRgb = mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;
    outColor = vec4(finalRgb, baseColor.a * alpha);
}

*/
