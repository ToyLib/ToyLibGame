#version 410 core

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

in vec2 vTex;
out vec4 outColor;

uniform sampler2D uSceneTex;

uniform int   uPostType;    // 0=None 1=Sepia 2=CRT 3=FairyLand 4=Noisy 5=Grayscale 6=Monochrome 7=Watercolor 8=OldFilm
uniform float uIntensity;   // 0..1
uniform float uTime;        // seconds (optional but recommended)
uniform int   uFlipY;       // 0/1

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

        // 範囲外は縁のピクセルに丸める（黒で打ち切らない）。
        // 四隅の見た目は下の rounded corners マスクだけに任せることで、
        // 低intensity時にここでうっすら四角く切れて見える問題を防ぐ。
        warped = clamp(warped, 0.0, 1.0);

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

        // 6) noise / grain
        float n = hash12(warped * vec2(640.0, 360.0) + uTime * 10.0);
        c += (n - 0.5) * 0.06 * I;

        // 7) slight gamma / contrast
        c = clamp(c, 0.0, 1.0);
        c = mix(c, pow(c, vec3(1.05)), 0.35 * I);

        // 8) rounded corners（四隅を丸める。アスペクト比補正して真円に近い丸みにする）
        //    上下左右の縁がうっすら暗くなる一般的なvignetteは廃止し、
        //    暗くなるのは丸角の範囲だけにする。
        //    ここより前で加算されるgrainノイズなどが黒い縁の外にはみ出て
        //    「色が流れて見える」ことがないよう、最後に一番最後に掛ける。
        vec2 crtRes    = vec2(textureSize(uSceneTex, 0));
        float crtAspect = crtRes.x / max(crtRes.y, 1.0);
        // 上下左右にも黒縁がちゃんと出るよう、四隅だけでなく全辺を内側に寄せる
        vec2 cornerHalf = vec2(0.5 * crtAspect, 0.5) - 0.05;
        float cornerRad = 0.09;
        vec2 cp = (warped - 0.5) * vec2(crtAspect, 1.0);
        vec2 cq = abs(cp) - cornerHalf + vec2(cornerRad);
        float cornerDist = length(max(cq, 0.0)) + min(max(cq.x, cq.y), 0.0) - cornerRad;
        // 縁の手前からなだらかにフェードさせる（0.01幅だと硬い黒縁になって濃く見えるため）
        float cornerMask = 1.0 - smoothstep(-0.03, 0.02, cornerDist);
        c *= mix(1.0, cornerMask, I);

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
    // Noisy: pure procedural noise overlay, strength = uIntensity
    // ------------------------------------------------------------
    if (uPostType == 4)
    {
        vec3 c = texture(uSceneTex, uv).rgb;

        // grain: procedural (no texture input)
        float p = paperNoise(uv, uTime);
        float grainStrength = 0.8 * I;
        c *= mix(1.0, p + 0.15, grainStrength);

        // fine additive noise
        float n = hash12(uv * vec2(700.0, 500.0) + uTime * 0.2) - 0.5;
        c += n * (0.14 * I);

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

    // ------------------------------------------------------------
    // Watercolor: edge-pooled pigment + wet-on-wet bleed + flat washes
    //   (no paper texture input — everything is derived from uSceneTex)
    // ------------------------------------------------------------
    if (uPostType == 7)
    {
        // I=0 でエフェクト完全オフになるよう、内部は常に「フル強度」で計算し
        // 最後に元画像と I でクロスフェードする（内部係数を I で個別に薄めない）
        vec3 orig = texture(uSceneTex, uv).rgb;

        vec2 texel = 1.0 / vec2(textureSize(uSceneTex, 0));

        // --- wet-on-wet bleed: warp the sample UV with slow low-freq noise ---
        float wn1 = hash12(uv * 5.0 + uTime * 0.05) - 0.5;
        float wn2 = hash12(uv * 11.0 - uTime * 0.07) - 0.5;
        vec2 warpUV = uv + vec2(wn1, wn2) * (4.0 * texel);

        // --- soft multi-tap sample: emulate pigment diffusion in wet paper ---
        vec3 c  = texture(uSceneTex, warpUV).rgb * 0.4;
        c += texture(uSceneTex, warpUV + vec2( texel.x,  texel.y) * 1.5).rgb * 0.15;
        c += texture(uSceneTex, warpUV + vec2(-texel.x,  texel.y) * 1.5).rgb * 0.15;
        c += texture(uSceneTex, warpUV + vec2( texel.x, -texel.y) * 1.5).rgb * 0.15;
        c += texture(uSceneTex, warpUV + vec2(-texel.x, -texel.y) * 1.5).rgb * 0.15;

        // --- edge detect on the un-warped scene: pigment pools at boundaries ---
        float lL = dot(texture(uSceneTex, uv - vec2(texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
        float lR = dot(texture(uSceneTex, uv + vec2(texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
        float lU = dot(texture(uSceneTex, uv - vec2(0.0, texel.y)).rgb, vec3(0.299, 0.587, 0.114));
        float lD = dot(texture(uSceneTex, uv + vec2(0.0, texel.y)).rgb, vec3(0.299, 0.587, 0.114));
        float edge = smoothstep(0.03, 0.30, abs(lL - lR) + abs(lU - lD));

        c *= mix(1.0, 0.55, edge);

        // --- gentle color shaping ---
        c = adjustSaturation(c, 0.90);
        c = pow(c, vec3(0.92));
        c = clamp(c, 0.0, 1.0);

        // --- posterize into flat pigment washes ---
        c = floor(c * 12.0 + 0.5) / 12.0;

        // --- procedural granulation (pigment settling, no paper photo needed) ---
        float grain = paperNoise(uv, uTime);
        c *= (0.90 + 0.20 * grain);

        // --- fine dry-brush speckle ---
        float speck = hash12(uv * vec2(760.0, 620.0) + uTime * 0.15) - 0.5;
        c += speck * 0.02;

        c = clamp(c, 0.0, 1.0);

        outColor = vec4(mix(orig, c, I), 1.0);
        return;
    }

    // ------------------------------------------------------------
    // OldFilm: sepia tint + flicker + vertical scratch + grain + vignette
    // ------------------------------------------------------------
    if (uPostType == 8)
    {
        vec3 orig = texture(uSceneTex, uv).rgb;

        // 低フレームレート風に時間を量子化(スクラッチ/グレインが「コマ落ち」する)
        float frame = floor(uTime * 12.0);

        vec3 c = orig;

        // --- aged sepia tint ---
        c = mix(c, applySepia(c), 0.6);
        c = adjustSaturation(c, 0.7);

        // --- global brightness flicker ---
        float flicker = 0.90 + 0.10 * hash12(vec2(frame, 3.7));
        c *= flicker;

        // --- vertical scratch: a thin line that jumps every "frame" ---
        float scratchX = hash12(vec2(frame, 11.0));
        float scratchDist = abs(uv.x - scratchX);
        float scratchOn = step(0.85, hash12(vec2(frame, 22.0)));
        c *= mix(1.0, 0.1, smoothstep(0.0006, 0.0, scratchDist) * scratchOn);

        // --- film grain (re-seeded per frame) ---
        float grain = hash12(uv * vec2(800.0, 600.0) + frame * 13.0) - 0.5;
        c += grain * 0.12;

        // --- vignette ---
        c *= mix(0.55, 1.0, softVignette(uv));

        // --- slight contrast lift ---
        c = clamp(c, 0.0, 1.0);
        c = mix(c, pow(c, vec3(1.15)), 0.4);

        outColor = vec4(mix(orig, clamp(c, 0.0, 1.0), I), 1.0);
        return;
    }

    // fallback
    outColor = texture(uSceneTex, uv);
}
