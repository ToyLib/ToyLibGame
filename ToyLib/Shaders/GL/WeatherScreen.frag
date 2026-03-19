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

//==================================================
// WeatherScreen.frag
// 画面全体に重ねる「前景エフェクト」用シェーダ
// - 雨スジ
// - 雪
// - もやっとした前景フォグ
// - レンズフレア（ゴースト）
//
// 方針:
//   - WeatherType は知らない
//   - 渡された parameter に応じて描画するだけ
//   - 全パラメータが 0 なら何も出ない
//==================================================

out vec4 FragColor;

//------------------------------
// Uniforms
//------------------------------
uniform float uTime;           // 経過時間（アニメーション用）
uniform vec2  uResolution;     // 画面サイズ
uniform float uRainAmount;     // 雨の強さ  0.0〜1.0
uniform float uSnowAmount;     // 雪の強さ  0.0〜1.0
uniform float uFogAmount;      // フォグの強さ 0.0〜1.0

// レンズフレア用
uniform vec2  uSunPos;         // 画面上の太陽位置 (0〜1, 左上原点)
uniform float uFlareIntensity; // フレアの強さ 0.0〜1.0
uniform vec3  uFlareColor;     // 太陽の基本色

const int SNOW_COUNT = 80;

//==================================================
// 共通：ハッシュ＆ノイズ
//==================================================
float hash(float x)
{
    return fract(sin(x) * 43758.5453123);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(a, b, u.x),
               mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amp   = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        value += amp * noise(p);
        p     *= 2.0;
        amp   *= 0.5;
    }
    return value;
}

//==================================================
// 雨エフェクト
//==================================================
float rainPattern(vec2 uv)
{
    uv *= vec2(300.0, 1.0);
    uv.y += uTime * 8.0;

    float id     = floor(uv.x);
    float offset = hash(vec2(id, 0.0));

    float y = fract(uv.y + offset);

    float shape = smoothstep(0.0, 0.01, y) * (1.0 - y);
    return shape;
}

//==================================================
// 雪エフェクト
//==================================================
float snowPattern(vec2 uv)
{
    float brightness = 0.0;

    for (int i = 0; i < SNOW_COUNT; ++i)
    {
        float fi = float(i);

        float x = hash(fi * 1.3) + sin(uTime * 0.2 + fi) * 0.01;
        float speed = 0.1 + hash(fi * 3.2) * 0.5;
        float y = fract(hash(fi * 2.1) - uTime * speed);

        vec2 snowPos = vec2(x, y);

        float dist = length(uv - snowPos);
        float size = 0.01 + hash(fi * 4.0) * 0.01;

        brightness += smoothstep(size, 0.0, dist);
    }

    return brightness;
}

//==================================================
// 前景フォグエフェクト
//==================================================
float fogPattern(vec2 uv)
{
    vec2 centeredUV = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec2 noiseUV = centeredUV * 1.5;

    float n = fbm(noiseUV + vec2(0.0, uTime * 0.02));
    return smoothstep(0.3, 1.0, n);
}

//==================================================
// レンズフレア（ゴースト）
//==================================================
vec3 computeLensFlare()
{
    if (uFlareIntensity <= 0.0)
    {
        return vec3(0.0);
    }

    vec2 centerUV = vec2(0.5, 0.5);
    vec2 centerPx = centerUV * uResolution;
    vec2 sunPx    = uSunPos * uResolution;

    vec2 axis     = centerPx - sunPx;
    float axisLen = length(axis);
    if (axisLen < 1e-4)
    {
        axisLen = 1e-4;
    }
    vec2 dir = axis / axisLen;

    float distCenter = axisLen / length(vec2(uResolution.x, uResolution.y));
    float edgeFade   = 1.0 - smoothstep(0.3, 0.9, distCenter);
    if (edgeFade <= 0.0)
    {
        return vec3(0.0);
    }

    float minDim = min(uResolution.x, uResolution.y);
    vec3  color  = vec3(0.0);

    // 太陽まわりのハロー
    {
        vec2  d    = gl_FragCoord.xy - sunPx;
        float dist = length(d) / minDim;

        float outerGlow = smoothstep(0.5, 0.0, dist);
        float innerGlow = smoothstep(0.20, 0.0, dist);

        float halo = outerGlow * 0.4 + innerGlow * 0.2;
        color += uFlareColor * halo * 0.25 * uFlareIntensity * edgeFade;
    }

    // ゴースト
    const int GHOST_COUNT = 30;

    float baseRadius[GHOST_COUNT] = float[](
        0.02, 0.022, 0.025, 0.028, 0.030,
        0.033, 0.036, 0.040, 0.043, 0.046,
        0.050, 0.053, 0.057, 0.060, 0.064,
        0.067, 0.071, 0.074, 0.078, 0.082,
        0.086, 0.090, 0.095, 0.10,  0.11,
        0.12, 0.13,  0.14,  0.15,  0.16
    );

    vec3 ghostColor[GHOST_COUNT] = vec3[](
        vec3(1.00, 0.35, 0.35),
        vec3(1.00, 0.50, 0.30),
        vec3(1.00, 0.65, 0.25),
        vec3(1.00, 0.80, 0.30),
        vec3(0.95, 0.95, 0.35),
        vec3(0.80, 1.00, 0.40),
        vec3(0.60, 1.00, 0.50),
        vec3(0.45, 1.00, 0.65),
        vec3(0.35, 1.00, 0.80),
        vec3(0.30, 1.00, 1.00),
        vec3(0.30, 0.80, 1.00),
        vec3(0.35, 0.60, 1.00),
        vec3(0.45, 0.45, 1.00),
        vec3(0.60, 0.35, 1.00),
        vec3(0.80, 0.30, 1.00),
        vec3(1.00, 0.30, 1.00),
        vec3(1.00, 0.30, 0.80),
        vec3(1.00, 0.30, 0.60),
        vec3(1.00, 0.30, 0.45),
        vec3(1.00, 0.40, 0.40),
        vec3(1.00, 0.55, 0.30),
        vec3(1.00, 0.70, 0.25),
        vec3(1.00, 0.85, 0.30),
        vec3(0.95, 0.95, 0.35),
        vec3(0.75, 1.00, 0.45),
        vec3(0.55, 1.00, 0.60),
        vec3(0.40, 1.00, 0.80),
        vec3(0.30, 0.90, 1.00),
        vec3(0.35, 0.60, 1.00),
        vec3(0.45, 0.45, 1.00)
    );

    float span = length(uResolution);

    for (int i = 0; i < GHOST_COUNT; ++i)
    {
        float t = float(i) / float(GHOST_COUNT - 1);

        float baseR = baseRadius[i];

        float freq = 1.5;
        float wave = 0.5 + 0.5 * sin(6.28318 * (t * freq + 0.25));

        float bigMask = smoothstep(0.6, 0.9, wave);

        float sizeFactor = mix(0.7, 1.6, bigMask);
        float r = baseR * sizeFactor;

        float s = mix(-span * 0.6, span * 0.6, t);
        vec2 gCenterPx = centerPx + dir * s;

        vec2  d    = gl_FragCoord.xy - gCenterPx;
        float dist = length(d) / minDim;

        float soft  = r * 0.6;
        float edge0 = max(r - soft, 0.0);
        float edge1 = r;

        float disk = 1.0 - smoothstep(edge0, edge1, dist);
        disk = pow(disk, 1.4);

        vec3 gCol = ghostColor[i] * disk
                    * uFlareIntensity * edgeFade * 0.5;

        color += gCol;
    }

    color *= 0.3;
    return color;
}

//==================================================
// メイン
//==================================================
void main()
{
    vec2 uv = gl_FragCoord.xy / uResolution;

    float alpha = 0.0;

    if (uRainAmount > 0.01)
    {
        alpha += rainPattern(uv) * uRainAmount * 0.25;
    }

    if (uSnowAmount > 0.01)
    {
        alpha += snowPattern(uv) * uSnowAmount * 1.2;
    }

    if (uFogAmount > 0.01)
    {
        alpha += fogPattern(uv) * uFogAmount * 0.9;
    }

    alpha = clamp(alpha, 0.0, 1.0);

    vec3 overlay = vec3(1.0) * alpha;
    vec3 flare   = computeLensFlare();

    float flareLuma  = clamp(max(flare.r, max(flare.g, flare.b)), 0.0, 1.0);
    float flareAlpha = flareLuma * 0.7;

    float finalAlpha = clamp(alpha + flareAlpha, 0.0, 1.0);
    vec3  finalColor = overlay + flare;

    if (finalAlpha <= 0.0001)
    {
        FragColor = vec4(0.0);
    }
    else
    {
        FragColor = vec4(finalColor, finalAlpha);
    }
}
