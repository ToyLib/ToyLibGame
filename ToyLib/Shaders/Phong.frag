#version 410

//======================================================================
//  Phong.frag
//  ・Phong + Toon 切り替え可能なライティング
//  ・ディレクショナルライト + シャドウマッピング + フォグ対応
//======================================================================


//======================================================================
//  Varyings（頂点シェーダーから）
//======================================================================

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPos;
in vec4 fragPosLightSpace;


//======================================================================
//  出力
//======================================================================
out vec4 outColor;


//======================================================================
//  Uniforms - マテリアル/カメラ/ライティング
//======================================================================

uniform sampler2D uTexture;

uniform vec3 uUniformColor;
uniform bool uOverrideColor;

uniform vec3  uCameraPos;
uniform float uSpecPower;

uniform vec3 uAmbientLight;

uniform float uShadowBias;
uniform bool  uUseToon;

// ★ 太陽専用の強度スケールは使わない（CPU 側で色やアンビエントに焼き込む）
//uniform float uSunIntensity;


//======================================================================
//  Directional Light（平行光源）
//======================================================================
struct DirectionalLight
{
    vec3 mDirection;    // 光の向き（ライト → シーン）
    vec3 mDiffuseColor; // 拡散反射色
    vec3 mSpecColor;    // 鏡面反射色
};
uniform DirectionalLight uDirLight;

//======================================================================
//  Point Light （追加）
//======================================================================
struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    float radius;
};

uniform int uNumPointLights;
uniform PointLight uPointLights[8];

//======================================================================
//  Fog（フォグ情報）
//======================================================================
struct FogInfo
{
    float maxDist;
    float minDist;
    vec3  color;
};
uniform FogInfo uFoginfo;


//======================================================================
//  Shadow Mapping
//======================================================================
uniform sampler2DShadow uShadowMap;


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
        if (uUseToon)
        {
            float diffIntensity = step(toonDiffuseThreshold, NdotL);

            float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), uSpecPower);
            specIntensity = step(toonSpecThreshold, specIntensity);

            result += uDirLight.mDiffuseColor * diffIntensity;
            result += uDirLight.mSpecColor   * specIntensity;
        }
        else
        {
            vec3 diffuse = uDirLight.mDiffuseColor * NdotL;

            vec3 specular = uDirLight.mSpecColor *
                            pow(max(dot(reflect(-L, N), V), 0.0), uSpecPower);

            result += diffuse + specular;
        }
    }

    return result;
}


//======================================================================
//  関数：シャドウ判定
//======================================================================
float ComputeShadow()
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // ライト投影外は影なし
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // 影マップの 1 texel サイズ
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    // PCF 3x3
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;

            // sampler2DShadow: texture(uShadowMap, vec3(uv, refZ))
            // 返り値は「光が当たってる率」(0..1)
            sum += texture(uShadowMap, vec3(projCoords.xy + offset,
                                            projCoords.z - uShadowBias));
        }
    }

    float lit = sum / 9.0; // 1.0=明るい, 0.0=影

    // 今までの「真っ黒にしない」仕様を維持
    return mix(0.5, 1.0, lit);
}

//======================================================================
//  関数：Point Light 計算（Phong / Toon 両対応）
//======================================================================
vec3 ComputePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos)
{
    vec3 Lvec = light.position - fragPos;
    float dist = length(Lvec);

    if (dist <= 0.0001)
        return vec3(0.0);

    // 距離でカリングしたい場合はここでチェック
    if (dist > light.radius)
        return vec3(0.0);

    vec3 L = normalize(Lvec);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return vec3(0.0);

    // 距離減衰
    float attenuation =
        1.0 / (light.constant +
               light.linear * dist +
               light.quadratic * dist * dist);

    vec3 result = vec3(0.0);

    if (uUseToon)
    {
        // ---------- Toon 版 ----------
        // Diffuse はしきい値で 0 or 1
        float diffIntensity = step(toonDiffuseThreshold, NdotL);

        // Specular も同様に段階化
        float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), uSpecPower);
        specIntensity = step(toonSpecThreshold, specIntensity);

        // PointLight は色1つなので、Diffuse/Specular 同じ色を使う
        result += light.color * diffIntensity;
        result += light.color * specIntensity;
    }
    else
    {
        // ---------- Phong 版 ----------
        vec3 diffuse = light.color * NdotL;

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), uSpecPower);
        vec3 specular = light.color * spec;

        result += diffuse + specular;
    }

    return result * light.intensity * attenuation;
}

//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : フォグ係数
    //------------------------------------------------------------------
    float dist = length(uCameraPos - fragWorldPos);
    float fogFactor = clamp(
        (uFoginfo.maxDist - dist) / (uFoginfo.maxDist - uFoginfo.minDist),
        0.0,
        1.0
    );

    //------------------------------------------------------------------
    // Step 2 : 単色描画モード
    //------------------------------------------------------------------
    if (uOverrideColor)
    {
        vec3 col = mix(uFoginfo.color, uUniformColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    //------------------------------------------------------------------
    // Step 3 : 基本ベクトル
    //------------------------------------------------------------------
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uCameraPos - fragWorldPos);
    vec3 L = normalize(-uDirLight.mDirection);

    //------------------------------------------------------------------
    // Step 4 : ディレクショナルライトによるライティング
    //------------------------------------------------------------------
    vec3 dirLight = ComputeLighting(N, V, L);

    // ★ ここで uSunIntensity を掛けない。
    //    昼夜・天候による強さは CPU 側で
    //    uDirLight.mDiffuseColor / uAmbientLight に込める。
    vec3 lighting = uAmbientLight + dirLight;

    //--------------------------------------------------------------
    // Step 4.5 : Point Lights（追加）
    //--------------------------------------------------------------
    for (int i = 0; i < uNumPointLights; ++i)
    {
        lighting += ComputePointLight(uPointLights[i], N, V, fragWorldPos);
    }
    
    //------------------------------------------------------------------
    // Step 5 : シャドウ（強さも uSunIntensity では制御しない）
    //------------------------------------------------------------------
    float shadowFactor = ComputeShadow();

    // ★ 必要なら「影の濃さ」用に別 uniform を追加してもいい
    // shadowFactor = mix(1.0, shadowFactor, uShadowStrength);

    //------------------------------------------------------------------
    // Step 6 : テクスチャ取得 + ライティング適用
    //------------------------------------------------------------------
    vec4 texColor = texture(uTexture, fragTexCoord);
    texColor.rgb *= lighting * shadowFactor;

    //------------------------------------------------------------------
    // Step 7 : フォグ合成
    //------------------------------------------------------------------
    vec3 finalColor = mix(uFoginfo.color, texColor.rgb, fogFactor);
    outColor = vec4(finalColor, texColor.a);
}

