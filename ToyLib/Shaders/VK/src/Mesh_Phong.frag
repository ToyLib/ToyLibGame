#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

//============================================================
// set0: Diffuse texture
//============================================================
layout(set = 0, binding = 0) uniform sampler2D uTexture;

//============================================================
// set1: Scene/Common
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

    mat4  uLightViewProj0;
    mat4  uLightViewProj1;

    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;
    int   uUseToon;
    float _pad4;
} sc;

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
// Push constants (World + Material)
//============================================================
layout(push_constant) uniform Push
{
    mat4 pcWorld;

    vec4 pcDiffuse;    // xyz = diffuse
    vec4 pcUniform;    // xyz = override color
    vec4 pcFlagsSpec;  // x=useTex, y=overrideColor, z=specPower, w=unused
} pc;

//============================================================
// Toon const
//============================================================
const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

//------------------------------------------------------------
// Directional lighting
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

            float specIntensity =
                pow(max(dot(reflect(-L, N), V), 0.0), pc.pcFlagsSpec.z);
            specIntensity = step(toonSpecThreshold, specIntensity);

            result += dl.uDirLight.mDiffuseColor * diffIntensity;
            result += dl.uDirLight.mSpecColor   * specIntensity;
        }
        else
        {
            vec3 diffuse =
                dl.uDirLight.mDiffuseColor * NdotL;

            vec3 specular =
                dl.uDirLight.mSpecColor *
                pow(max(dot(reflect(-L, N), V), 0.0), pc.pcFlagsSpec.z);

            result += diffuse + specular;
        }
    }

    return result;
}

//------------------------------------------------------------
// Point lighting
//------------------------------------------------------------
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
        1.0 / (light.constant +
               light.linear * dist +
               light.quadratic * dist * dist);

    vec3 result = vec3(0.0);

    if (sc.uUseToon != 0)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity =
            pow(max(dot(reflect(-L, N), V), 0.0), pc.pcFlagsSpec.z);
        specIntensity = step(toonSpecThreshold, specIntensity);

        result += light.color * diffIntensity;
        result += light.color * specIntensity;
    }
    else
    {
        vec3 diffuse = light.color * NdotL;

        vec3 R = reflect(-L, N);
        float spec =
            pow(max(dot(V, R), 0.0), pc.pcFlagsSpec.z);

        vec3 specular = light.color * spec;

        result += diffuse + specular;
    }

    return result * light.intensity * attenuation;
}

//============================================================
// main
//============================================================
void main()
{
    //--------------------------------------------------------
    // Fog
    //--------------------------------------------------------
    float dist = length(sc.uCameraPos - fragWorldPos);

    float fogFactor = clamp(
        (sc.uFogMaxDist - dist) /
        (sc.uFogMaxDist - sc.uFogMinDist),
        0.0, 1.0);

    //--------------------------------------------------------
    // Override color
    //--------------------------------------------------------
    if (int(pc.pcFlagsSpec.y + 0.5) != 0)
    {
        vec3 col =
            mix(sc.uFogColor, pc.pcUniform.xyz, fogFactor);

        outColor = vec4(col, 1.0);
        return;
    }

    //--------------------------------------------------------
    // Lighting
    //--------------------------------------------------------
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(sc.uCameraPos - fragWorldPos);
    vec3 L = normalize(-dl.uDirLight.mDirection);

    vec3 lighting =
        sc.uAmbientLight +
        ComputeLighting(N, V, L);

    for (int i = 0; i < pl.uNumPointLights; ++i)
    {
        lighting +=
            ComputePointLight(
                pl.uPointLights[i],
                N, V, fragWorldPos);
    }

    float shadowFactor = 1.0;

    //--------------------------------------------------------
    // Base color
    //--------------------------------------------------------
    vec4 baseColor;

    if (int(pc.pcFlagsSpec.x + 0.5) != 0)
    {
        baseColor = texture(uTexture, fragTexCoord);
    }
    else
    {
        baseColor = vec4(pc.pcDiffuse.xyz, 1.0);
    }

    baseColor.rgb *= lighting * shadowFactor;

    vec3 finalColor =
        mix(sc.uFogColor, baseColor.rgb, fogFactor);

    outColor = vec4(finalColor, baseColor.a);
}
