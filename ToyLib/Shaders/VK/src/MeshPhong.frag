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
