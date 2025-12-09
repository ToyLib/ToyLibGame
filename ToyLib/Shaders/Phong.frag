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

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    float shadow = textureProj(
        uShadowMap,
        vec4(projCoords.xy, projCoords.z - uShadowBias, 1.0)
    );

    // 0.5〜1.0 にマッピングして「完全な真っ暗」にはしない
    return mix(0.5, 1.0, shadow);
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


/*
//======================================================================
//  Phong.frag
//  ・Phong + Toon 切り替え可能なライティング
//  ・ディレクショナルライト + シャドウマッピング + フォグ対応
//======================================================================


//======================================================================
//  Varyings（頂点シェーダーから）
//======================================================================

// テクスチャ座標
in vec2 fragTexCoord;
// ワールド空間の法線
in vec3 fragNormal;
// ワールド空間の頂点座標
in vec3 fragWorldPos;
// ライト空間座標（シャドウマップ用）
in vec4 fragPosLightSpace;


//======================================================================
//  出力
//======================================================================
out vec4 outColor;


//======================================================================
//  Uniforms - マテリアル/カメラ/ライティング
//======================================================================

// ベースカラー用テクスチャ
uniform sampler2D uTexture;

// 単色で塗りつぶす場合の色
uniform vec3 uUniformColor;
// true の時はテクスチャを無視して uUniformColor を使う
uniform bool uOverrideColor;

// カメラ位置（視線ベクトル計算用）
uniform vec3 uCameraPos;

// スペキュラーの鋭さ（指数）
uniform float uSpecPower;

// 環境光（アンビエント）
uniform vec3 uAmbientLight;

// シャドウバイアス（シャドウアクネ対策）
uniform float uShadowBias;

// Toon シェーディングを使うかどうか
uniform bool uUseToon;

// 太陽光の強さ（朝夕や天候でのスケール）
uniform float uSunIntensity;


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
//  Fog（フォグ情報）
//======================================================================
struct FogInfo
{
    float maxDist;  // フォグが完全にかかる距離
    float minDist;  // フォグがかかり始める距離
    vec3  color;   // フォグの色
};
uniform FogInfo uFoginfo;


//======================================================================
//  Shadow Mapping
//======================================================================
// デプス比較付きのシャドウマップ
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

    // 光が当たっている側のみ計算
    if (NdotL > 0.0)
    {
        if (uUseToon)
        {
            //----------------------------
            // Toon Diffuse
            //----------------------------
            float diffIntensity = step(toonDiffuseThreshold, NdotL);

            //----------------------------
            // Toon Specular
            //----------------------------
            float specIntensity = pow(max(dot(reflect(-L, N), V), 0.0), uSpecPower);
            specIntensity = step(toonSpecThreshold, specIntensity);

            result += uDirLight.mDiffuseColor * diffIntensity;
            result += uDirLight.mSpecColor   * specIntensity;
        }
        else
        {
            //----------------------------
            // Phong Diffuse
            //----------------------------
            vec3 diffuse = uDirLight.mDiffuseColor * NdotL;

            //----------------------------
            // Phong Specular
            //----------------------------
            vec3 specular = uDirLight.mSpecColor *
                            pow(max(dot(reflect(-L, N), V), 0.0), uSpecPower);

            result += diffuse + specular;
        }
    }

    return result;
}


//======================================================================
//  関数：シャドウ判定
//  ・ライト空間座標からシャドウマップを参照
//  ・0.5〜1.0 の範囲で「少し柔らかい」シャドウに調整
//======================================================================
float ComputeShadow()
{
    // 透視除算
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // NDC(-1〜1) → テクスチャ座標(0〜1) に変換
    projCoords = projCoords * 0.5 + 0.5;

    // ライト視錐外なら「影なし」とみなす
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // シャドウマップで深度比較
    float shadow = textureProj(
        uShadowMap,
        vec4(projCoords.xy, projCoords.z - uShadowBias, 1.0)
    );

    // 0.5〜1.0 にマッピングして「完全な真っ暗」にはしない
    return mix(0.5, 1.0, shadow);
}


//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : フォグ係数の計算
    //------------------------------------------------------------------
    float dist = length(uCameraPos - fragWorldPos);
    float fogFactor = clamp(
        (uFoginfo.maxDist - dist) / (uFoginfo.maxDist - uFoginfo.minDist),
        0.0,
        1.0
    );

    //------------------------------------------------------------------
    // Step 2 : 単色描画モード（デバッグ等）
    //------------------------------------------------------------------
    if (uOverrideColor)
    {
        // フォグだけ適用して早期リターン
        vec3 col = mix(uFoginfo.color, uUniformColor, fogFactor);
        outColor = vec4(col, 1.0);
        return;
    }

    //------------------------------------------------------------------
    // Step 3 : 基本ベクトル（N:法線, V:視線, L:ライト方向）
    //------------------------------------------------------------------
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uCameraPos - fragWorldPos);
    vec3 L = normalize(-uDirLight.mDirection);

    
    //------------------------------------------------------------------
    // Step 4 : ディレクショナルライトによるライティング
    //------------------------------------------------------------------
    // まず太陽光(ディレクショナルライト)の分だけ計算
    vec3 dirLight = ComputeLighting(N, V, L);
    // アンビエント + 太陽光（太陽の強さでスケール）
    vec3 lighting = uAmbientLight + dirLight * uSunIntensity;

    //------------------------------------------------------------------
    // Step 5 : シャドウ（太陽の強さに応じて影もフェード）
    //------------------------------------------------------------------
    float shadowFactor = ComputeShadow();
    shadowFactor = mix(1.0, shadowFactor, uSunIntensity);

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
*/

