#version 450

layout(location=0) in vec2 vUV;
layout(location=1) in vec3 vNormalWS;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;


//======================================================================
// Data structs (std140-safe packing)
//  - C++ side should match VKPointLight:
//    position_radius[4], color_intensity[4], atten[4]
//======================================================================
struct PointLight
{
    vec4 position_radius;  // xyz=pos, w=radius
    vec4 color_intensity;  // xyz=color, w=intensity
    vec4 atten;            // x=constant, y=linear, z=quadratic, w=pad
};

layout(set=0, binding=0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;

    vec4 dirDir;        // xyz = direction
    vec4 dirDiffuse;
    vec4 dirSpecular;

    vec4 fogColor;
    vec4 fogParams;     // x=minDist, y=maxDist

    // ---- Point lights ----
    // std140: 16-byte boundary before arrays => int + pad3
    int  numPointLights;
    int  _padPL0;
    int  _padPL1;
    int  _padPL2;

    PointLight pointLights[8];

    // ---- Shadow ----
    mat4 shadowVP0;
    mat4 shadowVP1;
    vec4 shadowParams;  // x=split0 y=blend z=strength w=bias
} uScene;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

layout(set=3, binding=0) uniform sampler2DShadow uShadowMap0;
layout(set=3, binding=1) uniform sampler2DShadow uShadowMap1;

layout(push_constant) uniform PC
{
    mat4 world;
    vec4 baseColor_useTex; // rgb=baseColor, a=useTex
    vec4 misc;             // x=specPower y=toon z=overrideEnabled w=alpha
    vec4 overrideColor;
} pc;

//======================================================================
// constants (Toon)
//======================================================================
const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

//======================================================================
// Dir light (Phong / Toon)
//======================================================================
vec3 ComputeDirLight(vec3 N, vec3 V, vec3 L, float specPower, float toon)
{
    float NdotL = dot(N, L);
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

//======================================================================
// Point light (Phong / Toon)  ※GL版移植（packed struct対応）
//======================================================================
vec3 ComputePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos, float specPower, float toon)
{
    vec3  pos   = light.position_radius.xyz;
    float radius = light.position_radius.w;

    vec3  color = light.color_intensity.xyz;
    float intensity = light.color_intensity.w;

    float c = light.atten.x;
    float l = light.atten.y;
    float q = light.atten.z;

    vec3  Lvec = pos - fragPos;
    float dist = length(Lvec);

    if (dist <= 0.0001) return vec3(0.0);
    if (dist > radius)  return vec3(0.0);

    vec3  L = Lvec / dist;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    float attenuation =
        1.0 / (c + l * dist + q * dist * dist);

    vec3 result = vec3(0.0);

    if (toon > 0.5)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity =
            pow(max(dot(reflect(-L, N), V), 0.0), specPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        // GL版：diff/specとも同色（同色ハイライト）
        result += color * diffIntensity;
        result += color * specIntensity;
    }
    else
    {
        vec3 diffuse = color * NdotL;

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), specPower);
        vec3 specular = color * spec;

        result += diffuse + specular;
    }

    return result * intensity * attenuation;
}

//======================================================================
// Fog factor (GLと同じ形)
//======================================================================
float ComputeFogFactor(float dist, float minDist, float maxDist)
{
    float denom = max(maxDist - minDist, 0.0001);
    return clamp((maxDist - dist) / denom, 0.0, 1.0);
}

//======================================================================
// Shadow PCF 3x3 (GL互換：vec * mat)
//  - sampler2DShadow expects compare value in .z
//======================================================================
float ShadowPCF_3x3(sampler2DShadow smp, mat4 lightVP, vec3 worldPos, float shadowBias)
{
    vec4 lp = lightVP * vec4(worldPos, 1.0);

    vec3 proj = lp.xyz / max(lp.w, 1.0e-6);

    proj = proj * 0.5 + 0.5; // NDC(-1..1) -> UV(0..1)

    // outside => lit
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
            sum += texture(smp, vec3(proj.xy + offset, proj.z - shadowBias));
        }
    }

    float lit = sum / 9.0;

    // GL版：真っ黒にしない
    return mix(0.5, 1.0, lit);
}

float ComputeShadow(float camDist)
{
    float split0   = uScene.shadowParams.x;
    float blend    = uScene.shadowParams.y;
    float strength = uScene.shadowParams.z;
    float bias     = uScene.shadowParams.w;

    float s0 = ShadowPCF_3x3(uShadowMap0, uScene.shadowVP0, vWorldPos, bias);
    float s1 = ShadowPCF_3x3(uShadowMap1, uScene.shadowVP1, vWorldPos, bias);

    float t = smoothstep(split0 - blend, split0 + blend, camDist);
    float shadowFactor = mix(s0, s1, t);

    // strengthで混ぜ（1=影なし、0.5..1 が shadowFactor）
    return mix(1.0, shadowFactor, strength);
}

void main()
{
    float camDist   = length(uScene.cameraPos.xyz - vWorldPos);
    float fogFactor = ComputeFogFactor(camDist, uScene.fogParams.x, uScene.fogParams.y);

    // override (GL互換)
    if (pc.misc.z > 0.5)
    {
        vec3 col = mix(uScene.fogColor.xyz, pc.overrideColor.rgb, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    vec3 N = normalize(vNormalWS);
    vec3 V = normalize(uScene.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-uScene.dirDir.xyz);

    float specPower = max(pc.misc.x, 1.0);
    float toon      = pc.misc.y;

    // lighting (GL互換：ambient + dir + point)
    vec3 lighting = uScene.ambientLight.xyz;
    lighting += ComputeDirLight(N, V, L, specPower, toon);

    int nPL = clamp(uScene.numPointLights, 0, 8);
    for (int i = 0; i < nPL; ++i)
    {
        lighting += ComputePointLight(uScene.pointLights[i], N, V, vWorldPos, specPower, toon);
    }

    // shadow (CSM blend + strength)
    float shadowFactor = ComputeShadow(camDist);

    // base color (texture or constant)
    vec4 baseColor;
    if (pc.baseColor_useTex.a > 0.5)
        baseColor = texture(uBaseMap, vUV);
    else
        baseColor = vec4(pc.baseColor_useTex.rgb, 1.0);

    vec3 litColor = baseColor.rgb * (lighting * shadowFactor);
    vec3 finalRgb = mix(uScene.fogColor.xyz, litColor, fogFactor);

    float alpha = pc.misc.w;
    outColor = vec4(finalRgb, baseColor.a * alpha);
}
