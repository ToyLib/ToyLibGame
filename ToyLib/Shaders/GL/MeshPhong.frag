#version 410

//======================================================================
// ToyLib Uniform Contract (v2) - SceneUBO
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLSceneUBO)
//   Binding    : Render/GL/GLBindingPoints.h (kSceneUBOBinding = 1)
//======================================================================

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

// GL 4.1 は layout(binding=N) 不可 → C++ 側で glUniformBlockBinding 設定済み
// row_major: ToyLib は v*M（行ベクトル×行列）規約
layout(std140, row_major) uniform SceneUBO
{
    mat4  viewProj;
    vec4  cameraAndSun;      // xyz=cameraPos, w=sunIntensity
    vec4  ambientLight;      // xyz=ambient
    vec4  dirDirection;      // xyz=dirLight.direction
    vec4  dirDiffuse;        // xyz=dirLight.diffuse
    vec4  dirSpecular;       // xyz=dirLight.specular
    int   numPointLights;
    int   _plPad0, _plPad1, _plPad2;
    vec4  plPosRadius[8];      // xyz=position, w=radius
    vec4  plColorIntensity[8]; // xyz=color, w=intensity
    vec4  plAtten[8];          // x=constant, y=linear, z=quadratic
    vec4  fogColor;            // xyz=fog.color
    vec4  fogParams;           // x=minDist, y=maxDist
    mat4  lightViewProj0;
    mat4  lightViewProj1;
    vec4  shadowParams;        // x=cascadeSplit0, y=cascadeBlend, z=shadowBias
    ivec4 shadowFlags;         // x=shadowEnable
} uScene;

uniform sampler2DShadow uShadowMap0;
uniform sampler2DShadow uShadowMap1;

uniform ObjectData   uObject;
uniform MaterialData uMaterial;


//======================================================================
// Varyings
//======================================================================

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPos;


//======================================================================
// Output
//======================================================================

out vec4 outColor;


//======================================================================
// Toon thresholds
//======================================================================

const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;


//======================================================================
// Lighting
//======================================================================

vec3 ComputeLighting(vec3 N, vec3 V, vec3 L)
{
    vec3 result = vec3(0.0);
    float NdotL = dot(N, L);

    if (NdotL > 0.0)
    {
        if (uMaterial.toon)
        {
            float diffIntensity = step(toonDiffuseThreshold, NdotL);

            float specIntensity =
                pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

            specIntensity = step(toonSpecThreshold, specIntensity);

            result += uScene.dirDiffuse.xyz * diffIntensity;
            result += uScene.dirSpecular.xyz * specIntensity;
        }
        else
        {
            vec3 diffuse = uScene.dirDiffuse.xyz * NdotL;

            vec3 specular =
                uScene.dirSpecular.xyz *
                pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

            result += diffuse + specular;
        }
    }

    return result;
}


//======================================================================
// Point Light
//======================================================================

// 引数: plPosRadius(xyz=position, w=radius), plColorIntensity(xyz=color, w=intensity),
//       plAtten(x=constant, y=linear, z=quadratic)
vec3 ComputePointLight(vec4 posRadius, vec4 colorIntensity, vec4 atten,
                       vec3 N, vec3 V, vec3 fragPos)
{
    vec3 Lvec = posRadius.xyz - fragPos;
    float dist = length(Lvec);

    if (dist <= 0.0001) return vec3(0.0);
    if (dist > posRadius.w) return vec3(0.0);

    vec3 L = normalize(Lvec);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    float attenuation =
        1.0 / (atten.x +
               atten.y * dist +
               atten.z * dist * dist);

    vec3 result = vec3(0.0);

    if (uMaterial.toon)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity =
            pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

        specIntensity = step(toonSpecThreshold, specIntensity);

        result += colorIntensity.xyz * diffIntensity;
        result += colorIntensity.xyz * specIntensity;
    }
    else
    {
        vec3 diffuse = colorIntensity.xyz * NdotL;

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), uMaterial.specPower);

        vec3 specular = colorIntensity.xyz * spec;

        result += diffuse + specular;
    }

    return result * colorIntensity.w * attenuation;
}


//======================================================================
// Shadow PCF 5x5 with Normal-based bias
//======================================================================

float ShadowPCF(
    sampler2DShadow smp,
    mat4 lightVP,
    vec3 worldPos,
    vec3 N,
    vec3 L)
{
    vec4 lp = vec4(worldPos, 1.0) * lightVP;

    vec3 projCoords = lp.xyz / lp.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    //----------------------------------------------------------
    // Receiver-plane / normal-based bias
    //----------------------------------------------------------

    float ndotl = max(dot(N, L), 0.0);

    float bias = max(
        uScene.shadowParams.z * (1.0 - ndotl),
        uScene.shadowParams.z * 0.25
    );

    //----------------------------------------------------------
    // PCF 5x5
    //----------------------------------------------------------

    vec2 texelSize = 1.0 / vec2(textureSize(smp, 0));

    float sum = 0.0;
    int radius = 2;

    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;

            sum += texture(
                smp,
                vec3(
                    projCoords.xy + offset,
                    projCoords.z - bias
                )
            );
        }
    }

    float lit = sum / 25.0;

    // 真っ黒にしないゲーム寄り設定
    return mix(0.5, 1.0, lit);
}


//======================================================================
// Main
//======================================================================

void main()
{
    //----------------------------------------------------------
    // Fog factor
    //----------------------------------------------------------

    float dist = length(uScene.cameraAndSun.xyz - fragWorldPos);

    float fogFactor = clamp(
        (uScene.fogParams.y - dist) /
        (uScene.fogParams.y - uScene.fogParams.x),
        0.0,
        1.0
    );

    //----------------------------------------------------------
    // Override color
    //----------------------------------------------------------

    if (uMaterial.overrideEnabled)
    {
        vec3 col = mix(uScene.fogColor.xyz, uMaterial.overrideColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    //----------------------------------------------------------
    // Base vectors
    //----------------------------------------------------------

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uScene.cameraAndSun.xyz - fragWorldPos);
    vec3 L = normalize(-uScene.dirDirection.xyz);

    //----------------------------------------------------------
    // Directional light
    //----------------------------------------------------------

    vec3 dirLight = ComputeLighting(N, V, L);

    vec3 lighting = uScene.ambientLight.xyz + dirLight;

    //----------------------------------------------------------
    // Point lights
    //----------------------------------------------------------

    for (int i = 0; i < uScene.numPointLights; ++i)
    {
        lighting += ComputePointLight(
            uScene.plPosRadius[i],
            uScene.plColorIntensity[i],
            uScene.plAtten[i],
            N,
            V,
            fragWorldPos
        );
    }

    //----------------------------------------------------------
    // Shadow
    //----------------------------------------------------------
    float shadowFactor = 1.0;
    
    if (uScene.shadowFlags.x == 1)
    {
        float s0 = ShadowPCF(
                             uShadowMap0,
                             uScene.lightViewProj0,
                             fragWorldPos,
                             N,
                             L
                             );
        
        float s1 = ShadowPCF(
                             uShadowMap1,
                             uScene.lightViewProj1,
                             fragWorldPos,
                             N,
                             L
                             );
        
        float t = smoothstep(
                             uScene.shadowParams.x - uScene.shadowParams.y,
                             uScene.shadowParams.x + uScene.shadowParams.y,
                             dist
                             );
        
        shadowFactor = mix(s0, s1, t);
    }

    //----------------------------------------------------------
    // Base color
    //----------------------------------------------------------

    vec4 baseColor;

    if (uMaterial.useTexture)
        baseColor = texture(uMaterial.baseMap, fragTexCoord);
    else
        baseColor = vec4(uMaterial.baseColor, 1.0);

    baseColor.rgb *= lighting * shadowFactor;

    //----------------------------------------------------------
    // Fog
    //----------------------------------------------------------

    vec3 finalColor =
        mix(uScene.fogColor.xyz, baseColor.rgb, fogFactor);

    outColor = vec4(finalColor, baseColor.a);
}

