#version 410 core

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
    int   shadowEnable;
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
// WeatherDome.frag
//
// ・全天を覆うスカイドーム用フラグメントシェーダ
// ・時間帯・天候・太陽方向から、空／雲／太陽／星／月／天の川を表現
//
//  uWeatherType :
//      0 Simple  - 雲なし、太陽/月なし、夜は星のみ
//      1 Clear   - 快晴（薄雲、太陽、月、星、天の川）
//      2 Cloudy
//      3 Rain
//      4 Storm
//      5 Snow
//
//  uTimeOfDay   : 0.0〜1.0（夜→昼→夜）
//  uSunDir      : 太陽のワールド方向ベクトル
//
//======================================================================

out vec4 FragColor;
in vec3 vWorldDir;

//-------------------------
// 共通 Uniform
//-------------------------
uniform float uTime;
uniform int   uWeatherType;    // 0: Simple, 1: Clear, 2: Cloudy, 3: Rain, 4: Storm, 5: Snow
uniform float uTimeOfDay;      // 0.0〜1.0（夜→昼→夜）
uniform vec3  uSunDir;         // 太陽方向（ワールド空間）
uniform vec3  uMoonDir;        // 月方向

// C++ 側から渡される「素」の空色・雲色
uniform vec3 uRawSkyColor;
uniform vec3 uRawCloudColor;

//======================================================================
// ハッシュ / ノイズ（2D）
//======================================================================
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(a, b, u.x),
               mix(c, d, u.x), u.y);
}

// fbm (2D)
float fbm(vec2 p)
{
    float value = 0.0;
    float amp   = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        value += amp * vnoise(p);
        p = mod(p * 2.0, 1024.0);
        amp *= 0.5;
    }
    return value;
}

//======================================================================
// ハッシュ / ノイズ（3D）
//======================================================================
float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float vnoise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    vec3 u = f * f * (3.0 - 2.0 * f);

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

// fbm (3D) — 全天用（継ぎ目のないノイズ）
float fbm3(vec3 p)
{
    float value = 0.0;
    float amp   = 0.5;

    for (int i = 0; i < 5; ++i)
    {
        value += amp * vnoise3(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return value;
}

//======================================================================
// メイン
//======================================================================
void main()
{
    // 視線方向（スカイドーム上の方向）
    vec3 dir = normalize(vWorldDir);

    // 上方向ほど 1.0
    float t = clamp(dir.y, 0.0, 1.0);

    //------------------------------------------------------------------
    // 昼夜ブレンド（uTimeOfDay から Day/Night Strength を算出）
    //------------------------------------------------------------------
    float dayStrength =
        smoothstep(0.15, 0.25, uTimeOfDay) *
        (1.0 - smoothstep(0.75, 0.85, uTimeOfDay));
    float nightStrength = 1.0 - dayStrength;

    //------------------------------------------------------------------
    // ベース空色＋雲色
    //  - SIMPLE/CLEAR は raw を素直に使う
    //  - 他天候は少しグレー寄りにフェード
    //------------------------------------------------------------------
    float weatherFade =
        (uWeatherType == 0 || uWeatherType == 1) ? 1.0 : 0.3;

    vec3 rawSky   = uRawSkyColor;
    vec3 rawCloud = uRawCloudColor;

    vec3 baseSky    = mix(vec3(0.4, 0.4, 0.5), rawSky,   weatherFade);
    vec3 cloudColor = mix(vec3(0.4, 0.4, 0.4), rawCloud, weatherFade);

    // 天頂方向は明るめ、地平線側は少し暗め
    vec3 skyColor = mix(baseSky * 0.6, baseSky, t);

    float cloudAlpha = 0.0;

    //==================================================================
    // 雲ノイズ：天候タイプごとの密度・色補正
    //==================================================================
    {
        vec3 p = dir * 7.0;
        p.xz += vec2(uTime * 0.03, uTime * 0.01);

        float density = fbm3(p);

        if (uWeatherType == 0)         // SIMPLE
        {
            cloudAlpha = 0.0;
        }
        else if (uWeatherType == 1)    // CLEAR
        {
            cloudAlpha = smoothstep(0.38, 0.70, density);
        }
        else if (uWeatherType == 2)    // CLOUDY
        {
            cloudAlpha = smoothstep(0.20, 0.55, density);
            cloudColor = vec3(0.6);
            skyColor   = mix(skyColor, vec3(0.3), 0.6);
        }
        else if (uWeatherType == 3)    // RAIN
        {
            cloudAlpha = smoothstep(0.15, 0.45, density);
            skyColor  *= 0.35;
            cloudColor = vec3(0.5);
        }
        else if (uWeatherType == 4)    // STORM
        {
            cloudAlpha = smoothstep(0.15, 0.45, density);
            skyColor   = vec3(0.12);
            cloudColor = vec3(0.7);
        }
        else if (uWeatherType == 5)    // SNOW
        {
            cloudAlpha = smoothstep(0.22, 0.50, density);
            skyColor   = vec3(0.30);
            cloudColor = vec3(0.9);
        }

        // 雨・嵐・雪では雲の占有率をさらに増やす
        if (uWeatherType >= 3)
        {
            cloudAlpha = min(cloudAlpha + 0.4, 1.0);
        }
    }

    //------------------------------------------------------------------
    // 雷（STORM 専用）
    //------------------------------------------------------------------
    if (uWeatherType == 4)
    {
        float flash = step(0.98, fract(sin(uTime * 12.0) * 43758.5453));
        skyColor += vec3(1.0) * flash * 0.8;
    }

    //------------------------------------------------------------------
    // 雪天（SNOW）：空色・雲のブレンド強化
    //------------------------------------------------------------------
    if (uWeatherType == 5)
    {
        skyColor   = mix(skyColor, cloudColor, 0.4);
        cloudAlpha = min(cloudAlpha + 0.2, 1.0);
    }

    //------------------------------------------------------------------
    // 雲と空の最終ベース合成
    //------------------------------------------------------------------
    vec3 finalColor = mix(skyColor, cloudColor, cloudAlpha * 0.8);

    //==================================================================
    // ★ 夜空の星
    //  - SIMPLE と CLEAR のみ
    //  - SIMPLE は少し控えめ
    //==================================================================
    if (nightStrength > 0.01 && (uWeatherType == 0 || uWeatherType == 1))
    {
        float up     = clamp(dir.y, 0.0, 1.0);
        float upFade = smoothstep(0.0, 0.4, up);

        vec2 starUV = dir.xz * 40.0;

        float seed = hash12(starUV);

        // SIMPLE は少し星を増やす
        float starThreshold = (uWeatherType == 0) ? 0.9978 : 0.9985;
        float starMask      = smoothstep(starThreshold, 1.0, seed);

        float cloudBlock = 1.0 - cloudAlpha;

        vec3  starColor        = vec3(1.0, 0.97, 0.92);
        float starWeatherScale = (uWeatherType == 0) ? 0.45 : 1.0;
        float starIntensity    = starMask * nightStrength * upFade * cloudBlock * starWeatherScale;

        finalColor += starColor * starIntensity;
    }

    //==================================================================
    // ★ 月 (Moon)
    //  - CLEAR のみ
    //==================================================================
    if (nightStrength > 0.01 && uWeatherType == 1)
    {
        vec3 moonDir = normalize(uMoonDir);

        float m = clamp(dot(dir, -moonDir), 0.0, 1.0);

        float moonDisk = smoothstep(0.985, 1.0, m);
        float moonGlow = pow(m, 7680.0);
        float halo     = pow(m, 60.0);

        vec3 moonColor   = vec3(1.2, 1.15, 1.0);
        float cloudBlock = 1.0 - cloudAlpha;

        float moonIntensity =
            (moonDisk * 1.0 +
             moonGlow * 0.6 +
             halo     * 0.4) *
             nightStrength * cloudBlock;

        finalColor += moonColor * moonIntensity;
    }

    //==================================================================
    // ★ 天の川 (Milky Way)
    //  - CLEAR のみ
    //==================================================================
    if (nightStrength > 0.01 && uWeatherType == 1)
    {
        vec3 bandDir = normalize(vec3(0.5, -0.2, 0.0));

        float milky = abs(dot(dir, bandDir));
        float band  = smoothstep(0.5, 0.2, milky);

        float noise = fbm3(dir * 4.0 + vec3(0.0, uTime * 0.02, 0.0));

        float milkyMask = band * noise * nightStrength * (1.0 - cloudAlpha);

        vec3 milkyColor = vec3(0.75, 0.8, 1.0);
        milkyColor = mix(milkyColor, vec3(1.0, 0.8, 0.9), noise * 0.3);

        finalColor += milkyColor * milkyMask * 0.7;
    }

    //==================================================================
    // ★ 太陽ハイライト（サンディスク＆グロー）
    //  - CLEAR のみ
    //==================================================================
    if (uWeatherType == 1)
    {
        float hour = uTimeOfDay * 24.0;

        const float sunriseHour  = 6.0;
        const float sunsetHour   = 18.5;
        const float dawnSpanHour = 1.0;
        const float duskSpanHour = 1.5;

        float sunVisibility = 0.0;

        if (hour >= sunriseHour - dawnSpanHour && hour < sunriseHour + dawnSpanHour)
        {
            float tt = (hour - (sunriseHour - dawnSpanHour)) / (dawnSpanHour * 2.0);
            sunVisibility = clamp(tt, 0.0, 1.0);
        }
        else if (hour >= sunriseHour + dawnSpanHour && hour <= sunsetHour - duskSpanHour)
        {
            sunVisibility = 1.0;
        }
        else if (hour > sunsetHour - duskSpanHour && hour <= sunsetHour + duskSpanHour)
        {
            float tt = (sunsetHour + duskSpanHour - hour) / (duskSpanHour * 2.0);
            sunVisibility = clamp(tt, 0.0, 1.0);
        }

        if (sunVisibility > 0.001)
        {
            float sunAmount = clamp(dot(dir, -normalize(uSunDir)), 0.0, 1.0);

            float sunCore = pow(sunAmount, 4096.0);
            float sunHalo = pow(sunAmount, 100.0);

            vec3 sunCoreColor = vec3(1.3, 1.1, 0.8);
            vec3 sunHaloColor = vec3(1.1, 0.9, 0.7);
            vec3 sunGlow = sunCoreColor * sunCore + sunHaloColor * sunHalo;

            float cloudFactor = 1.0 - cloudAlpha;
            sunGlow *= sunVisibility * cloudFactor;

            finalColor += sunGlow;
        }
    }

    //------------------------------------------------------------------
    // 最終出力
    //------------------------------------------------------------------
    FragColor = vec4(finalColor, 1.0);
}
