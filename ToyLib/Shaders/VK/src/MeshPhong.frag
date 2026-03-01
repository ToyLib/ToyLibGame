#version 450

layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

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

    // Shadow (biased 行列が入ってくる前提：0..1空間)
    mat4 shadowVP0;
    mat4 shadowVP1;
    vec4 shadowParams;  // x=split0, y=split1, z=strength
} uScene;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

layout(set=3, binding=0) uniform sampler2DShadow uShadowMap0;
layout(set=3, binding=1) uniform sampler2DShadow uShadowMap1;

layout(push_constant) uniform PC
{
    mat4 world;
    vec4 baseColor_useTex;
    vec4 misc;           // x=specPower y=toon z=overrideEnabled w=alpha
    vec4 overrideColor;
} pc;

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

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
        vec3 diffuse  = uScene.dirDiffuse.xyz * NdotL;
        vec3 specular = uScene.dirSpecular.xyz *
                        pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        return diffuse + specular;
    }
}

float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

// biased 行列前提：shadowVP * worldPos がそのまま 0..1 の uvz を返す
float SampleShadowMap(sampler2DShadow smp, mat4 shadowVP)
{
    vec4 sc = shadowVP * vec4(vWorldPos, 1.0);
    sc.xyz /= sc.w;

    // 0..1 範囲外は影なし
    if (sc.x < 0.0 || sc.x > 1.0 ||
        sc.y < 0.0 || sc.y > 1.0 ||
        sc.z < 0.0 || sc.z > 1.0)
    {
        return 1.0;
    }

    // NOTE: bias は今は UBOに無いので（GL再現は後で）まず bias=0
    return texture(smp, sc.xyz);
}

float ComputeShadow()
{
    float camDist = length(uScene.cameraPos.xyz - vWorldPos);

    float split0    = uScene.shadowParams.x;
    float strength  = uScene.shadowParams.z;

    float shadow = (camDist < split0)
        ? SampleShadowMap(uShadowMap0, uScene.shadowVP0)
        : SampleShadowMap(uShadowMap1, uScene.shadowVP1);

    // 1=影なし 0=影 → strengthで混ぜる
    return mix(1.0, shadow, strength);
}

void main()
{
    float camDist = length(uScene.cameraPos.xyz - vWorldPos);
    float fogFactor = ComputeFogFactor(camDist, uScene.fogParams.x, uScene.fogParams.y);

    float overrideEnabled = pc.misc.z;
    if (overrideEnabled > 0.5)
    {
        // Fogを最後にかけたいならここも fog mix する（GL互換寄り）
        vec3 col = mix(uScene.fogColor.xyz, pc.overrideColor.rgb, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    vec3 dirLightOnly = ComputeDirLight(N, V, L, specPower, toon);

    float shadowFactor = ComputeShadow();

    vec3 lighting =
        uScene.ambientLight.xyz +
        dirLightOnly * shadowFactor;

    vec4 baseColor;
    float useTex = pc.baseColor_useTex.a;

    if (useTex > 0.5) baseColor = texture(uBaseMap, vUV);
    else              baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);

    vec3 litColor = baseColor.rgb * lighting;

    vec3 finalRgb = mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;
    outColor = vec4(finalRgb, baseColor.a * alpha);
}
