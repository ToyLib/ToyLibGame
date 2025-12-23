#version 410 core

in vec2 vTex;
out vec4 outColor;

uniform sampler2D uSceneTex;

uniform int   uPostType;    // 0=None 1=Sepia 2=CRT
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

// CRT-ish warp
vec2 barrelDistort(vec2 uv, float k)
{
    // uv: 0..1 -> -1..1
    vec2 p = uv * 2.0 - 1.0;
    float r2 = dot(p, p);
    p *= (1.0 + k * r2);
    return p * 0.5 + 0.5;
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
}
