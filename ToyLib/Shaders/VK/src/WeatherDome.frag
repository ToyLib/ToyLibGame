#version 450

//======================================================================
// WeatherDome.frag (Vulkan)
//  - GL版 SIMPLE/CLEAR 対応版に合わせたもの
//======================================================================

layout(location = 0) in vec3 vWorldDir;
layout(location = 0) out vec4 FragColor;

//--------------------------------------------------------------
// Scene UBO (set=0)
//--------------------------------------------------------------
struct VKPointLight
{
    vec4 position_radius;
    vec4 color_intensity;
    vec4 atten;
};

layout(std140, set = 0, binding = 0) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambient;
    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;

    vec4 fogColor;
    vec4 fogParams;

    ivec4 numPointLights_pad;
    VKPointLight pointLights[8];

    mat4 shadowVP0;
    mat4 shadowVP1;
    vec4 shadowParams;
} uScene;

//--------------------------------------------------------------
// Sky params UBO (set=1)
//--------------------------------------------------------------
layout(std140, set = 1, binding = 0) uniform SkyDomeParamsUBO
{
    mat4 world;

    vec4 timeParams;
    // x = uTime
    // y = uTimeOfDay
    // z = uWeatherType
    // w = reserved

    vec4 sunDir;
    vec4 moonDir;
    vec4 rawSkyColor;
    vec4 rawCloudColor;
} uSky;

//--------------------------------------------------------------
// Access helpers
//--------------------------------------------------------------
#define uTime          (uSky.timeParams.x)
#define uTimeOfDay     (uSky.timeParams.y)
#define uWeatherType   (int(uSky.timeParams.z + 0.5))
#define uSunDir        (uSky.sunDir.xyz)
#define uMoonDir       (uSky.moonDir.xyz)
#define uRawSkyColor   (uSky.rawSkyColor.xyz)
#define uRawCloudColor (uSky.rawCloudColor.xyz)

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

// 未使用だが GL 版との揃えで残す
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
// main
//======================================================================
void main()
{
    vec3 dir = normalize(vWorldDir);
    float t  = clamp(dir.y, 0.0, 1.0);

    float dayStrength =
        smoothstep(0.15, 0.25, uTimeOfDay) *
        (1.0 - smoothstep(0.75, 0.85, uTimeOfDay));
    float nightStrength = 1.0 - dayStrength;

    // SIMPLE/CLEAR は raw を素直に使う
    float weatherFade =
        (uWeatherType == 0 || uWeatherType == 1) ? 1.0 : 0.3;

    vec3 rawSky   = uRawSkyColor;
    vec3 rawCloud = uRawCloudColor;

    vec3 baseSky    = mix(vec3(0.4, 0.4, 0.5), rawSky,   weatherFade);
    vec3 cloudColor = mix(vec3(0.4, 0.4, 0.4), rawCloud, weatherFade);

    vec3 skyColor = mix(baseSky * 0.6, baseSky, t);

    float cloudAlpha = 0.0;

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

        if (uWeatherType >= 3)
        {
            cloudAlpha = min(cloudAlpha + 0.4, 1.0);
        }
    }

    if (uWeatherType == 4) // STORM
    {
        float flash = step(0.98, fract(sin(uTime * 12.0) * 43758.5453));
        skyColor += vec3(1.0) * flash * 0.8;
    }

    if (uWeatherType == 5) // SNOW
    {
        skyColor   = mix(skyColor, cloudColor, 0.4);
        cloudAlpha = min(cloudAlpha + 0.2, 1.0);
    }

    vec3 finalColor = mix(skyColor, cloudColor, cloudAlpha * 0.8);

    // 星: SIMPLE と CLEAR
    if (nightStrength > 0.01 && (uWeatherType == 0 || uWeatherType == 1))
    {
        float up     = clamp(dir.y, 0.0, 1.0);
        float upFade = smoothstep(0.0, 0.4, up);

        vec2 starUV = dir.xz * 40.0;

        float seed = hash12(starUV);

        float starThreshold = (uWeatherType == 0) ? 0.9978 : 0.9985;
        float starMask      = smoothstep(starThreshold, 1.0, seed);

        float cloudBlock = 1.0 - cloudAlpha;

        vec3  starColor        = vec3(1.0, 0.97, 0.92);
        float starWeatherScale = (uWeatherType == 0) ? 0.45 : 1.0;
        float starIntensity    = starMask * nightStrength * upFade * cloudBlock * starWeatherScale;

        finalColor += starColor * starIntensity;
    }

    // 月: CLEAR のみ
    if (nightStrength > 0.01 && uWeatherType == 1)
    {
        vec3 moonDirN = normalize(uMoonDir);

        float m = clamp(dot(dir, -moonDirN), 0.0, 1.0);

        float moonDisk = smoothstep(0.985, 1.0, m);
        float moonGlow = pow(m, 7680.0);
        float halo     = pow(m, 60.0);

        vec3  moonColor  = vec3(1.2, 1.15, 1.0);
        float cloudBlock = 1.0 - cloudAlpha;

        float moonIntensity =
            (moonDisk * 1.0 +
             moonGlow * 0.6 +
             halo     * 0.4) *
             nightStrength * cloudBlock;

        finalColor += moonColor * moonIntensity;
    }

    // 天の川: CLEAR のみ
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

    // 太陽: CLEAR のみ
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

    FragColor = vec4(finalColor, 1.0);
}
