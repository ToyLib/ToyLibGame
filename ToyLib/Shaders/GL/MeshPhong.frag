#version 410

//======================================================================
// ToyLib Uniform Contract (v1) - generated
//   See Render/GL/UniformNamesGL.h
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

// Max palette size must match engine-side upload
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
//  MeshPhong.frag
//  ・Phong + Toon 切り替え可能なライティング
//  ・Directional + Point + Fog
//  ・CSM(2 cascades) + PCF 3x3
//======================================================================

//======================================================================
//  Varyings（頂点シェーダーから）
//======================================================================

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPos;

//======================================================================
//  出力
//======================================================================
out vec4 outColor;

//======================================================================
//  Uniforms - マテリアル/カメラ/ライティング
//======================================================================

//======================================================================
//  Uniforms - マテリアル基本色
//======================================================================

//======================================================================
//  Directional Light（平行光源）
//======================================================================

//======================================================================
//  Point Light
//======================================================================

//======================================================================
//  Fog
//======================================================================

//======================================================================
//  Shadow Mapping (CSM 2 cascades)
//======================================================================

// どこで近景→遠景に切り替えるか（ワールド距離ベースでOK）

// 境界でのフェード幅（大きいほど切り替えが目立ちにくい）

//======================================================================
//  定数（Toon 関連）
//======================================================================
const float toonDiffuseThreshold = 0.5;
const float toonSpecThreshold    = 0.95;

//======================================================================
//  関数：ライティング計算（Phong / Toon 切り替え）
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

            float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);
            specIntensity = step(toonSpecThreshold, specIntensity);

            result += uScene.dirLight.diffuse * diffIntensity;
            result += uScene.dirLight.specular   * specIntensity;
        }
        else
        {
            vec3 diffuse = uScene.dirLight.diffuse * NdotL;

            vec3 specular = uScene.dirLight.specular *
                            pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);

            result += diffuse + specular;
        }
    }

    return result;
}

//======================================================================
//  関数：Point Light 計算（Phong / Toon 両対応）
//======================================================================
vec3 ComputePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos)
{
    vec3 Lvec = light.position - fragPos;
    float dist = length(Lvec);
    
    if (dist <= 0.0001)
    {
        return vec3(0.0);
    }
    if (dist > light.radius)
    {
        return vec3(0.0);
    }
    vec3 L = normalize(Lvec);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
    {
        return vec3(0.0);
    }
    float attenuation =
        1.0 / (light.constant +
               light.linear * dist +
               light.quadratic * dist * dist);

    vec3 result = vec3(0.0);

    if (uMaterial.toon)
    {
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), uMaterial.specPower);
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
//  関数：CSM 用 Shadow PCF（3x3）
//======================================================================
float ShadowPCF(sampler2DShadow smp, mat4 lightVP, vec3 worldPos)
{
    vec4 lp = vec4(worldPos, 1.0) * lightVP;
    vec3 projCoords = lp.xyz / lp.w;
    projCoords = projCoords * 0.5 + 0.5;

    // ライト投影外は影なし
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // 影マップの 1 texel サイズ
    vec2 texelSize = 1.0 / vec2(textureSize(smp, 0));

    // PCF 3x3
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;
            sum += texture(smp, vec3(projCoords.xy + offset,
                                     projCoords.z - uScene.shadowBias));
        }
    }

    float lit = sum / 9.0;

    // 「真っ黒にしない」仕様を維持（ゲーム向け）
    return mix(0.5, 1.0, lit);
}

//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : フォグ係数
    //------------------------------------------------------------------
    float dist = length(uScene.cameraPos - fragWorldPos);
    float fogFactor = clamp(
        (uScene.fog.maxDist - dist) / (uScene.fog.maxDist - uScene.fog.minDist),
        0.0,
        1.0
    );

    //------------------------------------------------------------------
    // Step 2 : 単色描画モード
    //------------------------------------------------------------------
    if (uMaterial.overrideEnabled)
    {
        vec3 col = mix(uScene.fog.color, uMaterial.overrideColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    //------------------------------------------------------------------
    // Step 3 : 基本ベクトル
    //------------------------------------------------------------------
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uScene.cameraPos - fragWorldPos);
    vec3 L = normalize(-uScene.dirLight.direction);

    //------------------------------------------------------------------
    // Step 4 : ディレクショナルライト
    //------------------------------------------------------------------
    vec3 dirLight = ComputeLighting(N, V, L);
    vec3 lighting = uScene.ambientLight + dirLight;

    //------------------------------------------------------------------
    // Step 4.5 : Point Lights
    //------------------------------------------------------------------
    for (int i = 0; i < uScene.numPointLights; ++i)
    {
        lighting += ComputePointLight(uScene.pointLights[i], N, V, fragWorldPos);
    }

    //------------------------------------------------------------------
    // Step 5 : CSM シャドウ（2枚を距離でブレンド）
    //------------------------------------------------------------------
    float s0 = ShadowPCF(uScene.shadowMap0, uScene.lightViewProj0, fragWorldPos);
    float s1 = ShadowPCF(uScene.shadowMap1, uScene.lightViewProj1, fragWorldPos);

    float t = smoothstep(
        uScene.cascadeSplit0 - uScene.cascadeBlend,
        uScene.cascadeSplit0 + uScene.cascadeBlend,
        dist
    );

    float shadowFactor = mix(s0, s1, t);

    //------------------------------------------------------------------
    // Step 6 : テクスチャ取得 + ライティング適用
    //------------------------------------------------------------------
    /*
     vec4 texColor = texture(uMaterial.baseMap, fragTexCoord);
    texColor.rgb *= lighting * shadowFactor;
    */
    //------------------------------------------------------------------
    // Step 6 : テクスチャ or DiffuseColor
    //------------------------------------------------------------------
    vec4 baseColor;

    if (uMaterial.useTexture)
    {
        baseColor = texture(uMaterial.baseMap, fragTexCoord);
    }
    else
    {
        baseColor = vec4(uMaterial.baseColor, 1.0);
    }

    // ライティング適用
    baseColor.rgb *= lighting * shadowFactor;
    
    //------------------------------------------------------------------
    // Step 7 : フォグ合成
    //------------------------------------------------------------------
    //vec3 finalColor = mix(uScene.fog.color, texColor.rgb, fogFactor);
    vec3 finalColor = mix(uScene.fog.color, baseColor.rgb, fogFactor);
    outColor = vec4(finalColor, baseColor.a);
}
