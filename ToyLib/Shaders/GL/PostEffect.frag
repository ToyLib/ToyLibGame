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

in vec2 vTex;
out vec4 outColor;

uniform sampler2D uSceneTex;

uniform int   uPostType;    // 0=None 1=Sepia 2=CRT 3=FairyLand 4=Watercolor 5=Grayscale
uniform float uIntensity;   // 0..1
uniform float uTime;        // seconds (optional but recommended)
uniform int   uFlipY;       // 0/1

// Watercolor option
uniform int   uUsePaperTex;  // 0/1
uniform sampler2D uPaperTex; // optional (set only if uUsePaperTex=1)

// ------------------------------------------------------------
// utilities
// ------------------------------------------------------------
float hash12(vec2 p)
{
    // cheap noise
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 applySepia(vec3 c)
{
    vec3 s;
    s.r = dot(c, vec3(0.393, 0.769, 0.189));
    s.g = dot(c, vec3(0.349, 0.686, 0.168));
    s.b = dot(c, vec3(0.272, 0.534, 0.131));
    return clamp(s, 0.0, 1.0);
}

vec3 applyGrayscale(vec3 c)
{
    float g = dot(c, vec3(0.299, 0.587, 0.114));
    return vec3(g);
}

// CRT-ish warp
vec2 barrelDistort(vec2 uv, float k)
{
    // uv: 0..1 -> -1..1
    vec2 p = uv * 2.0 - 1.0;
    float r2 = dot(p, p);
    p *= (1.0 + k * r2);
    return p * 0.5 + 0.5;
}

vec3 adjustSaturation(vec3 c, float s)
{
    // s=1.0 no change, >1 more saturated, <1 desaturated
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    return mix(vec3(l), c, s);
}

float softVignette(vec2 uv)
{
    // center=1, edge=0 (smooth)
    vec2 p = uv * 2.0 - 1.0;
    float r = length(p);
    return smoothstep(1.0, 0.25, r);
}

vec2 dreamyWarp(vec2 uv, float t, float strength)
{
    // 1周波数だけ（ゆっくり）
    float w = sin(t * 0.003 + uv.y * 0.06);

    // 上方向に寄せた流れ（固定）
    vec2 dir = vec2(0.45, 1.0);

    // 振幅を小さめに（チラつきに効く）
    uv += dir * w * (0.0042 * strength);
    return uv;
}

float paperNoise(vec2 uv, float t)
{
    // procedural “paper grain” (no texture version)
    float n1 = hash12(uv * vec2(900.0, 700.0) + t * 0.3);
    float n2 = hash12(uv * vec2(240.0, 180.0) - t * 0.2);
    return 0.6 * n1 + 0.4 * n2;
}

void main()
{
    vec2 uv = vTex;
    if (uFlipY != 0) uv.y = 1.0 - uv.y;

    // early out (none)
    if (uPostType == 0)
    {
        outColor = texture(uSceneTex, uv);
        return;
    }

    float I = clamp(uIntensity, 0.0, 1.0);

    // ------------------------------------------------------------
    // Sepia
    // ------------------------------------------------------------
    if (uPostType == 1)
    {
        vec3 c = texture(uSceneTex, uv).rgb;

        vec3 sep = applySepia(c);

        // ほんの少しコントラスト/暗部落ち（雰囲気）
        sep = mix(sep, pow(sep, vec3(0.9)), 0.25);

        vec3 outRgb = mix(c, sep, I);
        outColor = vec4(outRgb, 1.0);
        return;
    }

    // ------------------------------------------------------------
    // CRT
    // ------------------------------------------------------------
    if (uPostType == 2)
    {
        // 1) warp
        vec2 warped = barrelDistort(uv, 0.10 * I);
        
        // 画面外は黒（簡易）
        if (warped.x < 0.0 || warped.x > 1.0 || warped.y < 0.0 || warped.y > 1.0)
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        // 2) subtle horizontal jitter
        float jitter = (hash12(vec2(floor(warped.y * 240.0), uTime)) - 0.5) * 0.0025 * I;
        warped.x += jitter;

        // 3) chromatic aberration (RGB sample shift)
        float ca = 0.0015 * I;
        vec3 c;
        c.r = texture(uSceneTex, warped + vec2( ca, 0.0)).r;
        c.g = texture(uSceneTex, warped).g;
        c.b = texture(uSceneTex, warped + vec2(-ca, 0.0)).b;

        // 4) scanlines
        float scan = 0.85 + 0.15 * sin((warped.y * 2.0 - 1.0) * 800.0);
        c *= mix(1.0, scan, 0.8 * I);

        // 5) shadow mask (subpixel-ish)
        float mask = 0.90 + 0.10 * sin(warped.x * 1200.0);
        c *= mix(1.0, mask, 0.6 * I);

        // 6) vignette
        vec2 p = warped * 2.0 - 1.0;
        float vig = 1.0 - 0.35 * I * dot(p, p);
        c *= clamp(vig, 0.0, 1.0);

        // 7) noise / grain
        float n = hash12(warped * vec2(640.0, 360.0) + uTime * 10.0);
        c += (n - 0.5) * 0.06 * I;

        // 8) slight gamma / contrast
        c = clamp(c, 0.0, 1.0);
        c = mix(c, pow(c, vec3(1.05)), 0.35 * I);

        outColor = vec4(c, 1.0);
        return;
    }

    // ------------------------------------------------------------
    // FairyLand (A): pastel + sky-fog (top weighted) + gentle dreamy warp
    // ------------------------------------------------------------
    if (uPostType == 3)
    {
        // base uv (no CRT warp)
        vec2 uv2 = uv;

        // --------------------------------------------
        // 1) "sky fog" factor (top weighted)
        // --------------------------------------------
        float sky = smoothstep(0.35, 0.85, uv2.y);

        // さらに「空の上ほど濃い」感じに
        sky = pow(sky, 1.35);

        // --------------------------------------------
        // 2) dreamy warp (weaken on ground)
        // --------------------------------------------
        float warpStrength = I * mix(0.25, 1.0, sky);
        uv2 = dreamyWarp(uv2, uTime, warpStrength);

        vec3 c = texture(uSceneTex, uv2).rgb;

        // --------------------------------------------
        // 3) pastel lift (global but gentle)
        // --------------------------------------------
        c = pow(c, vec3(mix(1.0, 0.88, I)));

        // 彩度は少しだけ上げて「おとぎ感」
        c = adjustSaturation(c, mix(1.0, 1.15, I));

        // --------------------------------------------
        // 4) sky fog color (top only)
        // --------------------------------------------
        vec3 fogTint = vec3(0.85, 0.80, 0.90);
        float fogAmt = (0.10 + 0.40 * sky) * I;

        c = mix(c, fogTint, fogAmt);

        // --------------------------------------------
        // 5) vignette: keep it very mild
        // --------------------------------------------
        vec2 p = uv2 * 2.0 - 1.0;
        float r2 = dot(p, p);
        float vig = 1.0 - (0.10 * I) * r2;
        c *= clamp(vig, 0.0, 1.0);

        // --------------------------------------------
        // 6) subtle sparkle (mostly in the sky)
        // --------------------------------------------
        float sp = step(0.987, hash12(uv2 * vec2(520.0, 300.0) + uTime * 0.6));
        c += sp * vec3(1.0, 0.95, 0.85) * (0.08 * I * sky);

        outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // ------------------------------------------------------------
    // Watercolor (B): paper texture/grain + gentle color shaping
    // ------------------------------------------------------------
    if (uPostType == 4)
    {
        vec3 c = texture(uSceneTex, uv).rgb;

        // gentle “watercolor” shaping: slightly desaturate + lift
        c = adjustSaturation(c, mix(1.0, 0.95, I));
        c = pow(c, vec3(mix(1.0, 0.90, I)));
        c = clamp(c, 0.0, 1.0);

        // paper: texture version or procedural fallback
        float p = 1.0;
        if (uUsePaperTex != 0)
        {
            vec3 pt = texture(uPaperTex, uv).rgb;
            p = dot(pt, vec3(0.333));
        }
        else
        {
            p = paperNoise(uv, uTime);
        }

        // apply paper grain softly
        float grainStrength = 0.5 * I;
        c *= mix(1.0, p + 0.15, grainStrength);

        // a little edge softness by mixing tiny noise
        float n = hash12(uv * vec2(700.0, 500.0) + uTime * 0.2) - 0.5;
        c += n * (0.03 * I);

        outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // ------------------------------------------------------------
    // Grayscale
    // ------------------------------------------------------------
    if (uPostType == 5)
    {
        vec3 c = texture(uSceneTex, uv).rgb;
        vec3 g = applyGrayscale(c);

        vec3 outRgb = mix(c, g, I);
        outColor = vec4(outRgb, 1.0);
        return;
    }
    
    // ------------------------------------------------------------
    // Monochrome
    // ------------------------------------------------------------
    if (uPostType == 6)
    {
        vec3 c = texture(uSceneTex, uv).rgb;

        // 通常グレースケール
        float g = dot(c, vec3(0.299, 0.587, 0.114));
        vec3 gray = vec3(g);

        // 二値化（しきい値は中央固定でもOK）
        float threshold = 0.5;
        vec3 bw = vec3(step(threshold, g));

        // I に応じて段階的に遷移
        // 0.0 → 元画像
        // 0.5 → グレースケール
        // 1.0 → 完全白黒
        vec3 mid = mix(c, gray, clamp(I * 2.0, 0.0, 1.0));
        vec3 outRgb = mix(mid, bw, clamp((I - 0.5) * 2.0, 0.0, 1.0));

        outColor = vec4(outRgb, 1.0);
        return;
    }

    // fallback
    outColor = texture(uSceneTex, uv);
}
