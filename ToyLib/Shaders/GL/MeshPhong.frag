#version 410

//======================================================================
// ToyLib Uniform Contract (v1)
//======================================================================

struct DirLight
{
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight
{
    vec3  position;
    vec3  color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    float radius;
};

struct FogInfo
{
    float maxDist;
    float minDist;
    vec3  color;
};

struct SceneData
{
    mat4 viewProj;

    vec3 cameraPos;

    vec3  ambientLight;
    float sunIntensity;

    DirLight dirLight;

    int        numPointLights;
    PointLight pointLights[8];

    FogInfo fog;

    sampler2DShadow shadowMap0;
    sampler2DShadow shadowMap1;

    mat4  lightViewProj0;
    mat4  lightViewProj1;
    float cascadeSplit0;
    float cascadeBlend;
    float shadowBias;
};

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

const int kMaxPalette = 96;

struct SkinnedData
{
    mat4 matrixPalette[kMaxPalette];
};

uniform SceneData    uScene;
uniform ObjectData   uObject;
uniform MaterialData uMaterial;
uniform SkinnedData  uSkinned;


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

            result += uScene.dirLight.diffuse * diffIntensity;
            result += uScene.dirLight.specular * specIntensity;
        }
        else
        {
            vec3 diffuse = uScene.dirLight.diffuse * NdotL;

            vec3 specular =
                uScene.dirLight.specular *
                pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

            result += diffuse + specular;
        }
    }

    return result;
}


//======================================================================
// Point Light
//======================================================================

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

    if (uMaterial.toon)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity =
            pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

        specIntensity = step(toonSpecThreshold, specIntensity);

        result += light.color * diffIntensity;
        result += light.color * specIntensity;
    }
    else
    {
        vec3 diffuse = light.color * NdotL;

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), uMaterial.specPower);

        vec3 specular = light.color * spec;

        result += diffuse + specular;
    }

    return result * light.intensity * attenuation;
}


//======================================================================
// Shadow PCF with Normal-based bias
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
    // Receiver plane bias
    //----------------------------------------------------------

    float ndotl = max(dot(N, L), 0.0);

    float bias = max(
        uScene.shadowBias * (1.0 - ndotl),
        uScene.shadowBias * 0.25
    );

    //----------------------------------------------------------
    // PCF 3x3
    //----------------------------------------------------------

    vec2 texelSize = 1.0 / vec2(textureSize(smp, 0));

    float sum = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;

            sum += texture(
                smp,
                vec3(projCoords.xy + offset,
                     projCoords.z - bias)
            );
        }
    }

    float lit = sum / 9.0;

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

    float dist = length(uScene.cameraPos - fragWorldPos);

    float fogFactor = clamp(
        (uScene.fog.maxDist - dist) /
        (uScene.fog.maxDist - uScene.fog.minDist),
        0.0,
        1.0
    );

    //----------------------------------------------------------
    // Override color
    //----------------------------------------------------------

    if (uMaterial.overrideEnabled)
    {
        vec3 col = mix(uScene.fog.color, uMaterial.overrideColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    //----------------------------------------------------------
    // Base vectors
    //----------------------------------------------------------

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uScene.cameraPos - fragWorldPos);
    vec3 L = normalize(-uScene.dirLight.direction);

    //----------------------------------------------------------
    // Directional light
    //----------------------------------------------------------

    vec3 dirLight = ComputeLighting(N, V, L);

    vec3 lighting = uScene.ambientLight + dirLight;

    //----------------------------------------------------------
    // Point lights
    //----------------------------------------------------------

    for (int i = 0; i < uScene.numPointLights; ++i)
    {
        lighting += ComputePointLight(
            uScene.pointLights[i],
            N,
            V,
            fragWorldPos
        );
    }

    //----------------------------------------------------------
    // Shadow
    //----------------------------------------------------------

    float s0 = ShadowPCF(
        uScene.shadowMap0,
        uScene.lightViewProj0,
        fragWorldPos,
        N,
        L
    );

    float s1 = ShadowPCF(
        uScene.shadowMap1,
        uScene.lightViewProj1,
        fragWorldPos,
        N,
        L
    );

    float t = smoothstep(
        uScene.cascadeSplit0 - uScene.cascadeBlend,
        uScene.cascadeSplit0 + uScene.cascadeBlend,
        dist
    );

    float shadowFactor = mix(s0, s1, t);

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
        mix(uScene.fog.color, baseColor.rgb, fogFactor);

    outColor = vec4(finalColor, baseColor.a);
}
