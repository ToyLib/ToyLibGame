#version 450

layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 outColor;

//======================================================================
// Textures
//  set=0, binding=0 : scene color
//======================================================================
layout(set = 0, binding = 0) uniform sampler2D uSceneTex;

//======================================================================
// Push Constants
//  x = postType
//  y = intensity
//  z = time
//  w = flipY
//======================================================================
layout(push_constant) uniform PostPC
{
    vec4 params0; // x=postType, y=intensity, z=time, w=flipY
} pc;

// ------------------------------------------------------------
// utilities
// ------------------------------------------------------------
float hash12(vec2 p)
{
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

vec2 barrelDistort(vec2 uv, float k)
{
    vec2 p = uv * 2.0 - 1.0;
    float r2 = dot(p, p);
    p *= (1.0 + k * r2);
    return p * 0.5 + 0.5;
}

vec3 adjustSaturation(vec3 c, float s)
{
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
    float w = sin(t * 0.003 + uv.y * 0.06);
    vec2 dir = vec2(0.45, 1.0);
    uv += dir * w * (0.0042 * strength);
    return uv;
}

float paperNoise(vec2 uv, float t)
{
    float n1 = hash12(uv * vec2(900.0, 700.0) + t * 0.3);
    float n2 = hash12(uv * vec2(240.0, 180.0) - t * 0.2);
    return 0.6 * n1 + 0.4 * n2;
}

vec2 ApplyFlip(vec2 inUV, int flipY)
{
    vec2 outUV = inUV;
    if (flipY != 0)
    {
        outUV.y = 1.0 - outUV.y;
    }
    return outUV;
}

void main()
{
    // fxUV : 画面効果計算用（上下反転しない）
    // uv   : テクスチャ参照用（必要なら上下反転する）
    vec2 fxUV = vTex;

    int   uPostType    = int(pc.params0.x + 0.5);
    float uIntensity   = pc.params0.y;
    float uTime        = pc.params0.z;
    int   uFlipY       = int(pc.params0.w + 0.5);

    vec2 uv = ApplyFlip(fxUV, uFlipY);

    // ------------------------------------------------------------
    // None
    // ------------------------------------------------------------
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
        // 効果計算は非反転 UV で行う
        vec2 warpedFX = barrelDistort(fxUV, 0.10 * I);

        if (warpedFX.x < 0.0 || warpedFX.x > 1.0 || warpedFX.y < 0.0 || warpedFX.y > 1.0)
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        // サンプル用 UV のみ flip を適用
        vec2 warped = ApplyFlip(warpedFX, uFlipY);

        float jitter = (hash12(vec2(floor(warpedFX.y * 240.0), uTime)) - 0.5) * 0.0025 * I;
        warped.x += jitter;

        float ca = 0.0015 * I;
        vec3 c;
        c.r = texture(uSceneTex, warped + vec2( ca, 0.0)).r;
        c.g = texture(uSceneTex, warped).g;
        c.b = texture(uSceneTex, warped + vec2(-ca, 0.0)).b;

        float scan = 0.85 + 0.15 * sin((warpedFX.y * 2.0 - 1.0) * 800.0);
        c *= mix(1.0, scan, 0.8 * I);

        float mask = 0.90 + 0.10 * sin(warpedFX.x * 1200.0);
        c *= mix(1.0, mask, 0.6 * I);

        vec2 p = warpedFX * 2.0 - 1.0;
        float vig = 1.0 - 0.35 * I * dot(p, p);
        c *= clamp(vig, 0.0, 1.0);

        float n = hash12(warpedFX * vec2(640.0, 360.0) + uTime * 10.0);
        c += (n - 0.5) * 0.06 * I;

        c = clamp(c, 0.0, 1.0);
        c = mix(c, pow(c, vec3(1.05)), 0.35 * I);

        outColor = vec4(c, 1.0);
        return;
    }

    // ------------------------------------------------------------
    // FairyLand
    // ------------------------------------------------------------
    if (uPostType == 3)
    {
        // 効果の意味付けは非反転 UV 基準
        vec2 uv2FX = fxUV;

        float sky = smoothstep(0.35, 0.85, uv2FX.y);
        sky = pow(sky, 1.35);

        float warpStrength = I * mix(0.25, 1.0, sky);
        uv2FX = dreamyWarp(uv2FX, uTime, warpStrength);

        // サンプル時だけ flip
        vec2 uv2 = ApplyFlip(uv2FX, uFlipY);

        vec3 c = texture(uSceneTex, uv2).rgb;

        c = pow(c, vec3(mix(1.0, 0.88, I)));
        c = adjustSaturation(c, mix(1.0, 1.15, I));

        vec3 fogTint = vec3(0.85, 0.80, 0.90);
        float fogAmt = (0.10 + 0.40 * sky) * I;
        c = mix(c, fogTint, fogAmt);

        vec2 p = uv2FX * 2.0 - 1.0;
        float r2 = dot(p, p);
        float vig = 1.0 - (0.10 * I) * r2;
        c *= clamp(vig, 0.0, 1.0);

        float sp = step(0.987, hash12(uv2FX * vec2(520.0, 300.0) + uTime * 0.6));
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

        float p = paperNoise(fxUV, uTime);

        float grainStrength = 0.8 * I;
        c *= mix(1.0, p + 0.15, grainStrength);

        float n = hash12(fxUV * vec2(700.0, 500.0) + uTime * 0.2) - 0.5;
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
        // 最後に元画像と I でクロスフェードする(内部係数を I で個別に薄めない)
        vec3 orig = texture(uSceneTex, uv).rgb;

        vec2 texel = 1.0 / vec2(textureSize(uSceneTex, 0));

        // --- wet-on-wet bleed: warp the sample UV with slow low-freq noise ---
        float wn1 = hash12(fxUV * 5.0 + uTime * 0.05) - 0.5;
        float wn2 = hash12(fxUV * 11.0 - uTime * 0.07) - 0.5;
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
        float grain = paperNoise(fxUV, uTime);
        c *= (0.90 + 0.20 * grain);

        // --- fine dry-brush speckle ---
        float speck = hash12(fxUV * vec2(760.0, 620.0) + uTime * 0.15) - 0.5;
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
        float scratchDist = abs(fxUV.x - scratchX);
        float scratchOn = step(0.85, hash12(vec2(frame, 22.0)));
        c *= mix(1.0, 0.1, smoothstep(0.0006, 0.0, scratchDist) * scratchOn);

        // --- film grain (re-seeded per frame) ---
        float grain = hash12(fxUV * vec2(800.0, 600.0) + frame * 13.0) - 0.5;
        c += grain * 0.12;

        // --- vignette ---
        c *= mix(0.55, 1.0, softVignette(fxUV));

        // --- slight contrast lift ---
        c = clamp(c, 0.0, 1.0);
        c = mix(c, pow(c, vec3(1.15)), 0.4);

        outColor = vec4(mix(orig, clamp(c, 0.0, 1.0), I), 1.0);
        return;
    }

    // fallback
    outColor = texture(uSceneTex, uv);
}
