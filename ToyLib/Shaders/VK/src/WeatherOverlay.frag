#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform OverlayPC
{
    float time;
    float rainAmount;
    float fogAmount;
    float snowAmount;

    vec2  resolution;
    float flareIntensity;
    float _pad0;

    vec2  sunPos;
    vec2  _pad1;

    vec4  flareColor;
} pc;

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

float rainPattern(vec2 uv)
{
    uv *= vec2(300.0, 1.0);
    uv.y += pc.time * 8.0;

    float id     = floor(uv.x);
    float offset = hash12(vec2(id, 0.0));
    float y      = fract(uv.y + offset);

    return smoothstep(0.0, 0.01, y) * (1.0 - y);
}

const int SNOW_COUNT = 80;

float snowPattern(vec2 uv)
{
    float brightness = 0.0;

    for (int i = 0; i < SNOW_COUNT; ++i)
    {
        float fi = float(i);

        float x = hash1(fi * 1.3) + sin(pc.time * 0.2 + fi) * 0.01;
        float speed = 0.1 + hash1(fi * 3.2) * 0.5;
        float y = fract(hash1(fi * 2.1) - pc.time * speed);

        vec2 snowPos = vec2(x, y);

        float dist = length(uv - snowPos);
        float size = 0.01 + hash1(fi * 4.0) * 0.01;

        brightness += smoothstep(size, 0.0, dist);
    }

    return brightness;
}

float fogPattern(vec2 uv)
{
    vec2 centeredUV = (gl_FragCoord.xy - 0.5 * pc.resolution) / pc.resolution.y;
    vec2 noiseUV = centeredUV * 1.5;

    float n = fbm(noiseUV + vec2(0.0, pc.time * 0.02));
    return smoothstep(0.3, 1.0, n);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / pc.resolution;

    float alpha = 0.0;

    if (pc.rainAmount > 0.01)
    {
        alpha += rainPattern(uv) * pc.rainAmount * 0.25;
    }

    if (pc.snowAmount > 0.01)
    {
        alpha += snowPattern(uv) * pc.snowAmount * 1.2;
    }

    if (pc.fogAmount > 0.01)
    {
        alpha += fogPattern(uv) * pc.fogAmount * 0.9;
    }

    alpha = clamp(alpha, 0.0, 1.0);

    vec3 color = vec3(1.0) * alpha;
    outColor = vec4(color, alpha);
}
