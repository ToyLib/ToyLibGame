#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

//============================================================
// set0: Material (Diffuse)
//============================================================
layout(set = 0, binding = 0) uniform sampler2D uTexture;

//============================================================
// set1: Scene/Common
//  ※ MoltenVK 対策：binding を欠番なしで 0..3 に詰める
//============================================================
layout(set = 1, binding = 0, std140) uniform WorldCommon
{
    mat4 uViewProj;
    vec3 uCameraPos; float _pad0;

    vec3 uAmbientLight; float _pad1;

    float uFogMaxDist;
    float uFogMinDist;
    vec2  _pad2;
    vec3  uFogColor;
    float _pad3;

    // Shadow / CSM（互換維持用に残す：今は使わない）
    mat4  uLightViewProj0;
    mat4  uLightViewProj1;
    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;   // 0/1（今は常に 0 想定）
    int   uUseToon;     // 0/1
    float _pad4;
} sc;

//============================================================
// Shadow maps（実装再開用：今は無効）
//  ※ 連番維持のため、復活させるなら set=1 binding=4,5 などで追加推奨
//============================================================
// layout(set = 1, binding = 4) uniform sampler2DShadow uShadowMap0;
// layout(set = 1, binding = 5) uniform sampler2DShadow uShadowMap1;

//============================================================
// Material params  (set1 binding=1)
//============================================================
layout(set = 1, binding = 1, std140) uniform MaterialParams
{
    vec3  uDiffuseColor; int uUseTexture;    // 0/1
    vec3  uUniformColor; int uOverrideColor; // 0/1
    float uSpecPower;
    float _padM0;
    float _padM1;
    float _padM2;
} mp;

//============================================================
// DirLight (set1 binding=2)
//============================================================
struct DirectionalLight
{
    vec3 mDirection;    float _p0;
    vec3 mDiffuseColor; float _p1;
    vec3 mSpecColor;    float _p2;
};

layout(set = 1, binding = 2, std140) uniform DirLightBlock
{
    DirectionalLight uDirLight;
} dl;

//============================================================
// PointLight (set1 binding=3)
//============================================================
struct PointLight
{
    vec3 position; float intensity;
    vec3 color;    float constant;
    float linear;
    float quadratic;
    float radius;
    float _p;
};

layout(set = 1, binding = 3, std140) uniform PointLightBlock
{
    int uNumPointLights;
    int _pA; int _pB; int _pC;
    PointLight uPointLights[8];
} pl;

//============================================================
// Toon const
//============================================================
const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

//------------------------------------------------------------
// Lighting helpers
//------------------------------------------------------------
vec3 ComputeLighting(vec3 N, vec3 V, vec3 L)
{
    vec3 result = vec3(0.0);
    float NdotL = dot(N, L);

    if (NdotL > 0.0)
    {
        if (sc.uUseToon != 0)
        {
            float diffIntensity = step(toonDiffuseThreshold, NdotL);
            float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), mp.uSpecPower);
            specIntensity = step(toonSpecThreshold, specIntensity);

            result += dl.uDirLight.mDiffuseColor * diffIntensity;
            result += dl.uDirLight.mSpecColor   * specIntensity;
        }
        else
        {
            vec3 diffuse  = dl.uDirLight.mDiffuseColor * NdotL;
            vec3 specular = dl.uDirLight.mSpecColor *
                            pow(max(dot(reflect(-L, N), V), 0.0), mp.uSpecPower);
            result += diffuse + specular;
        }
    }
    return result;
}

vec3 ComputePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos)
{
    vec3 Lvec = light.position - fragPos;
    float dist = length(Lvec);

    if (dist <= 0.0001) return vec3(0.0);
    if (dist > light.radius) return vec3(0.0);

    vec3 L = normalize(Lvec);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    float attenuation =
        1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 result = vec3(0.0);

    if (sc.uUseToon != 0)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);
        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), mp.uSpecPower);
        specIntensity = step(toonSpecThreshold, specIntensity);
        result += light.color * diffIntensity;
        result += light.color * specIntensity;
    }
    else
    {
        vec3 diffuse = light.color * NdotL;
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), mp.uSpecPower);
        vec3 specular = light.color * spec;
        result += diffuse + specular;
    }

    return result * light.intensity * attenuation;
}

//============================================================
// ShadowPCF（実装再開用：今は無効）
//============================================================
/*
float ShadowPCF(sampler2DShadow smp, mat4 lightVP, vec3 worldPos)
{
    vec4 lp = vec4(worldPos, 1.0) * lightVP; // v*M
    vec3 projCoords = lp.xyz / lp.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
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
            sum += texture(smp, vec3(projCoords.xy + offset,
                                     projCoords.z - sc.uShadowBias));
        }
    }

    float lit = sum / 9.0;
    return mix(0.5, 1.0, lit);
}
*/

void main()
{
    //========================================================
    // Fog
    //========================================================
    float dist = length(sc.uCameraPos - fragWorldPos);
    float fogFactor = clamp(
                            (sc.uFogMaxDist - dist) / (sc.uFogMaxDist - sc.uFogMinDist),
                            0.0, 1.0);
    
    //========================================================
    // override color
    //========================================================
    if (mp.uOverrideColor != 0)
    {
        vec3 col = mix(sc.uFogColor, mp.uUniformColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }
    
    //========================================================
    // Lighting
    //========================================================
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(sc.uCameraPos - fragWorldPos);
    vec3 L = normalize(-dl.uDirLight.mDirection);
    
    vec3 lighting = sc.uAmbientLight + ComputeLighting(N, V, L);
    
    for (int i = 0; i < pl.uNumPointLights; ++i)
    {
        lighting += ComputePointLight(pl.uPointLights[i], N, V, fragWorldPos);
    }
    
    //========================================================
    // Shadow（今は無効：固定で 1.0）
    //========================================================
    float shadowFactor = 1.0;
    
    // 影処理（実装再開用：今は無効）
    /*
     float shadowFactor = 1.0;
     if (sc.uUseShadow != 0)
     {
     float s0 = ShadowPCF(uShadowMap0, sc.uLightViewProj0, fragWorldPos);
     float s1 = ShadowPCF(uShadowMap1, sc.uLightViewProj1, fragWorldPos);
     
     float t = smoothstep(
     sc.uCascadeSplit0 - sc.uCascadeBlend,
     sc.uCascadeSplit0 + sc.uCascadeBlend,
     dist);
     
     shadowFactor = mix(s0, s1, t);
     }
     */
    
    //========================================================
    // Base color
    //========================================================
    vec4 baseColor;
    if (mp.uUseTexture != 0)
    {
        baseColor = texture(uTexture, fragTexCoord);
    }
    else
    {
        baseColor = vec4(mp.uDiffuseColor, 1.0);
    }
    
    baseColor.rgb *= lighting * shadowFactor;
    
    vec3 finalColor = mix(sc.uFogColor, baseColor.rgb, fogFactor);
    outColor = vec4(finalColor, baseColor.a);
    float d = length(fragWorldPos);          // ワールド原点からの距離
    outColor = vec4(vec3(clamp(d * 0.02, 0.0, 1.0)), 1.0);
}
