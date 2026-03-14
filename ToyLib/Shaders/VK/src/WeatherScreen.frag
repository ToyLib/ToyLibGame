#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

//==================================================
// WeatherOverlay.frag (Vulkan)
//  - C++側の sunUv はそのまま使う
//  - frag側で gl_FragCoord だけ左上原点系に揃える
//==================================================

layout(std140, set = 1, binding = 0) uniform OverlayUBO
{
    vec4 time;        // x = uTime
    vec4 resolution;  // x = width, y = height
    vec4 weather;     // x = rain, y = snow, z = fog
    vec4 sunPos;      // x = uSunPos.x, y = uSunPos.y （C++側で反転済み）
    vec4 flare;       // x = flareIntensity
    vec4 flareColor;  // xyz = flareColor
} uOverlay;

#define uTime           (uOverlay.time.x)
#define uResolution     (uOverlay.resolution.xy)
#define uRainAmount     (uOverlay.weather.x)
#define uSnowAmount     (uOverlay.weather.y)
#define uFogAmount      (uOverlay.weather.z)
#define uSunPos         (uOverlay.sunPos.xy)
#define uFlareIntensity (uOverlay.flare.x)
#define uFlareColor     (uOverlay.flareColor.xyz)

const int SNOW_COUNT = 80;

//==================================================
// Hash / Noise
//==================================================
float hash1(float x)
{
    return fract(sin(x) * 43758.5453123);
}

float hash12(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

float noise2(vec2 p)
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

float fbm(vec2 p)
{
    float value = 0.0;
    float amp   = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        value += amp * noise2(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return value;
}

//==================================================
// Weather patterns
//==================================================
float rainPattern(vec2 uv)
{
    uv *= vec2(300.0, 1.0);
    uv.y += uTime * 8.0;

    float id     = floor(uv.x);
    float offset = hash12(vec2(id, 0.0));
    float y      = fract(uv.y + offset);

    return smoothstep(0.0, 0.01, y) * (1.0 - y);
}

float snowPattern(vec2 uv)
{
    float brightness = 0.0;

    for (int i = 0; i < SNOW_COUNT; ++i)
    {
        float fi = float(i);

        float x     = hash1(fi * 1.3) + sin(uTime * 0.2 + fi) * 0.01;
        float speed = 0.1 + hash1(fi * 3.2) * 0.5;

        // 左上原点UVで「下向き」に落とす
        float y = fract(hash1(fi * 2.1) - uTime * speed);

        vec2 snowPos = vec2(x, y);

        float dist = length(uv - snowPos);
        float size = 0.01 + hash1(fi * 4.0) * 0.01;

        brightness += smoothstep(size, 0.0, dist);
    }

    return brightness;
}

float fogPattern(vec2 uv)
{
    vec2 centeredUV = (uv * uResolution - 0.5 * uResolution) / uResolution.y;
    vec2 noiseUV = centeredUV * 1.5;

    float n = fbm(noiseUV + vec2(0.0, uTime * 0.02));
    return smoothstep(0.3, 1.0, n);
}

//==================================================
// Lens flare
//  - uSunPos は C++側で「左上原点UV」に揃っている前提
//  - fragPxTopLeft も左上原点ピクセルに揃えて計算
//==================================================
vec3 computeLensFlare(vec2 fragPxTopLeft)
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
    if (axisLen < 1e-4) axisLen = 1e-4;

    vec2 dir = axis / axisLen;

    float distCenter = axisLen / length(vec2(uResolution.x, uResolution.y));
    float edgeFade   = 1.0 - smoothstep(0.3, 0.9, distCenter);
    if (edgeFade <= 0.0)
    {
        return vec3(0.0);
    }

    float minDim = min(uResolution.x, uResolution.y);
    vec3  color  = vec3(0.0);

    // halo around sun
    {
        vec2  d    = fragPxTopLeft - sunPx;
        float dist = length(d) / minDim;

        float outerGlow = smoothstep(0.5, 0.0, dist);
        float innerGlow = smoothstep(0.20, 0.0, dist);
        float halo      = outerGlow * 0.4 + innerGlow * 0.2;

        color += uFlareColor * halo * 0.25 * uFlareIntensity * edgeFade;
    }

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
        float freq  = 1.5;
        float wave  = 0.5 + 0.5 * sin(6.28318 * (t * freq + 0.25));
        float bigMask = smoothstep(0.6, 0.9, wave);

        float smallScale = 0.7;
        float bigScale   = 1.6;
        float sizeFactor = mix(smallScale, bigScale, bigMask);
        float r = baseR * sizeFactor;

        float s = mix(-span * 0.6, span * 0.6, t);
        vec2 gCenterPx = centerPx + dir * s;

        vec2  d    = fragPxTopLeft - gCenterPx;
        float dist = length(d) / minDim;

        float soft  = r * 0.6;
        float edge0 = max(r - soft, 0.0);
        float edge1 = r;

        float disk = 1.0 - smoothstep(edge0, edge1, dist);
        disk = pow(disk, 1.4);

        vec3 gCol = ghostColor[i] * disk * uFlareIntensity * edgeFade * 0.5;
        color += gCol;
    }

    color *= 0.3;
    return color;
}

//==================================================
// main
//==================================================
void main()
{
    // frag側でだけ左上原点に揃える
    vec2 fragPxTopLeft = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 uv            = fragPxTopLeft / uResolution;

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
    vec3 flare   = computeLensFlare(fragPxTopLeft);

    float flareLuma = max(flare.r, max(flare.g, flare.b));
    flareLuma = clamp(flareLuma, 0.0, 1.0);

    float flareAlpha = flareLuma * 0.7;
    float finalAlpha = clamp(alpha + flareAlpha, 0.0, 1.0);

    vec3 finalColor = overlay + flare;

    outColor = vec4(finalColor, finalAlpha);
}
