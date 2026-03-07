#version 450

layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 outColor;

//======================================================================
// Textures
//  set=0, binding=0 : scene color
//  set=0, binding=1 : optional paper texture
//======================================================================
layout(set = 0, binding = 0) uniform sampler2D uSceneTex;
layout(set = 0, binding = 1) uniform sampler2D uPaperTex;

//======================================================================
// Push Constants
//  x = postType
//  y = intensity
//  z = time
//  w = flipY
//
//  params1.x = usePaperTex
//======================================================================
layout(push_constant) uniform PostPC
{
    vec4 params0; // x=postType, y=intensity, z=time, w=flipY
    vec4 params1; // x=usePaperTex
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

void main()
{
    vec2 uv = vTex;

    int   uPostType   = int(pc.params0.x + 0.5);
    float uIntensity  = pc.params0.y;
    float uTime       = pc.params0.z;
    int   uFlipY      = int(pc.params0.w + 0.5);
    int   uUsePaperTex = int(pc.params1.x + 0.5);

    if (uFlipY != 0)
    {
        uv.y = 1.0 - uv.y;
    }

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
        vec2 warped = barrelDistort(uv, 0.10 * I);

        if (warped.x < 0.0 || warped.x > 1.0 || warped.y < 0.0 || warped.y > 1.0)
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        float jitter = (hash12(vec2(floor(warped.y * 240.0), uTime)) - 0.5) * 0.0025 * I;
        warped.x += jitter;

        float ca = 0.0015 * I;
        vec3 c;
        c.r = texture(uSceneTex, warped + vec2( ca, 0.0)).r;
        c.g = texture(uSceneTex, warped).g;
        c.b = texture(uSceneTex, warped + vec2(-ca, 0.0)).b;

        float scan = 0.85 + 0.15 * sin((warped.y * 2.0 - 1.0) * 800.0);
        c *= mix(1.0, scan, 0.8 * I);

        float mask = 0.90 + 0.10 * sin(warped.x * 1200.0);
        c *= mix(1.0, mask, 0.6 * I);

        vec2 p = warped * 2.0 - 1.0;
        float vig = 1.0 - 0.35 * I * dot(p, p);
        c *= clamp(vig, 0.0, 1.0);

        float n = hash12(warped * vec2(640.0, 360.0) + uTime * 10.0);
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
        vec2 uv2 = uv;

        float sky = smoothstep(0.35, 0.85, uv2.y);
        sky = pow(sky, 1.35);

        float warpStrength = I * mix(0.25, 1.0, sky);
        uv2 = dreamyWarp(uv2, uTime, warpStrength);

        vec3 c = texture(uSceneTex, uv2).rgb;

        c = pow(c, vec3(mix(1.0, 0.88, I)));
        c = adjustSaturation(c, mix(1.0, 1.15, I));

        vec3 fogTint = vec3(0.85, 0.80, 0.90);
        float fogAmt = (0.10 + 0.40 * sky) * I;
        c = mix(c, fogTint, fogAmt);

        vec2 p = uv2 * 2.0 - 1.0;
        float r2 = dot(p, p);
        float vig = 1.0 - (0.10 * I) * r2;
        c *= clamp(vig, 0.0, 1.0);

        float sp = step(0.987, hash12(uv2 * vec2(520.0, 300.0) + uTime * 0.6));
        c += sp * vec3(1.0, 0.95, 0.85) * (0.08 * I * sky);

        outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // ------------------------------------------------------------
    // Watercolor
    // ------------------------------------------------------------
    if (uPostType == 4)
    {
        vec3 c = texture(uSceneTex, uv).rgb;

        c = adjustSaturation(c, mix(1.0, 0.95, I));
        c = pow(c, vec3(mix(1.0, 0.90, I)));
        c = clamp(c, 0.0, 1.0);

        float p = 1.0;
        if (uUsePaperTex != 0)
        {
            vec3 pt = texture(uPaperTex, uv).rgb;
            p = dot(pt, vec3(0.3333333));
        }
        else
        {
            p = paperNoise(uv, uTime);
        }

        float grainStrength = 0.5 * I;
        c *= mix(1.0, p + 0.15, grainStrength);

        float n = hash12(uv * vec2(700.0, 500.0) + uTime * 0.2) - 0.5;
        c += n * (0.03 * I);

        outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // fallback
    outColor = texture(uSceneTex, uv);
}

/*
#version 450

layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uSceneTex;
layout(set = 0, binding = 1) uniform sampler2D uPaperTex;

layout(push_constant) uniform PostPC
{
    vec4 params0;
    vec4 params1;
} pc;

void main()
{
    vec2 uv = vTex;
    if (int(pc.params0.w + 0.5) != 0)
    {
        uv.y = 1.0 - uv.y;
    }

    outColor = texture(uSceneTex, uv);
}
*/
