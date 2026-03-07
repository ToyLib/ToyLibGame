#version 450

//======================================================================
// RenderSurface.frag (Vulkan)
//======================================================================

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

//======================================================================
// set1 : Surface Texture
//======================================================================
layout(set = 1, binding = 0) uniform sampler2D uSurfaceTex;

//======================================================================
// Push Constants
//  world          : frag側では未使用だが layout を vert と一致させる
//  tintOpacity    : rgb=tint, a=opacity
//  params0        : x=flipX, y=flipY, z=mode, w=scanlineStrength
//  params1        : x=time, y=distortStrength, z=fresnel, w=fresnelPow
//  params2        : x=waveSpeed, y=swayStrength, z=sparkleStrength, w=reserved
//======================================================================
layout(push_constant) uniform SurfacePC
{
    mat4 world;
    vec4 tintOpacity;
    vec4 params0;
    vec4 params1;
    vec4 params2;
} pc;

//--------------------------------------------------------------
// Helpers
//--------------------------------------------------------------
vec2 ApplyFlip(vec2 uv)
{
    if (pc.params0.x > 0.5) uv.x = 1.0 - uv.x; // flipX
    if (pc.params0.y > 0.5) uv.y = 1.0 - uv.y; // flipY
    return uv;
}

float softHighlight(vec2 uv, float t, float wave)
{
    float crest = smoothstep(0.55, 0.95, wave);

    float band = sin(uv.x * 6.0 + t * 0.6) * 0.5 + 0.5;
    band *= sin(uv.y * 5.0 - t * 0.5) * 0.5 + 0.5;

    band = smoothstep(0.45, 0.85, band);

    float fw = fwidth(band);
    band = smoothstep(0.45 - fw, 0.85 + fw, band);

    return crest * band;
}

vec4 SampleSurface(vec2 uv)
{
    uv = clamp(uv, vec2(0.001), vec2(0.999));
    return texture(uSurfaceTex, uv);
}

void main()
{
    vec2 uv = ApplyFlip(vUV);

    vec3  uTint             = pc.tintOpacity.rgb;
    float uOpacity          = pc.tintOpacity.a;

    int   uMode             = int(pc.params0.z + 0.5);
    float uScanlineStrength = pc.params0.w;

    float uTime             = pc.params1.x;
    float uDistortStrength  = pc.params1.y;
    float uFresnel          = pc.params1.z;
    float uFresnelPow       = pc.params1.w;

    float uWaveSpeed        = pc.params2.x;
    float uSwayStrength     = pc.params2.y;
    float uSparkleStrength  = pc.params2.z;
    

    // ============================================================
    // 0) Plain
    // ============================================================
    if (uMode == 0)
    {
        vec4 c = SampleSurface(uv);
        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 1) Monitor
    // ============================================================
    if (uMode == 1)
    {
        vec4 c = SampleSurface(uv);

        float sl = sin((uv.y + uTime * 0.4) * 800.0) * 0.5 + 0.5;
        float k  = mix(1.0 - uScanlineStrength, 1.0, sl);
        c.rgb *= k;

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 2) Mirror
    // ============================================================
    if (uMode == 2)
    {
        float n = sin(uv.x * 30.0 + uTime * 1.2) * sin(uv.y * 25.0 - uTime * 1.0);
        vec2 duv = uv + vec2(n, -n) * (uDistortStrength * 0.3);

        vec4 c = SampleSurface(duv);

        float f = clamp(uFresnel, 0.0, 1.0);
        float edge = pow(f, max(uFresnelPow, 0.0001));

        c.rgb = mix(c.rgb * 0.85, c.rgb * 1.15, edge);

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 3) Water
    // ============================================================
    if (uMode == 3)
    {
        float t = uTime * 0.5;

        float swell =
            sin(vUV.x * 1.5 + t * 0.4) +
            cos(vUV.y * 1.2 - t * 0.3);
        swell = swell * 0.5 + 0.5;

        vec2 uv2 = ApplyFlip(vUV);

        uv2 += vec2(
            sin(t * 0.3 + uv2.y * 2.0),
            cos(t * 0.25 + uv2.x * 1.8)
        ) * 0.412;

        uv2 += vec2(
            swell - 0.5,
            (1.0 - swell) - 0.5
        ) * 0.115;

        uv2 = fract(uv2);

        vec2 ofs = vec2(0.206, 0.206) * (0.7 + 0.6 * swell);

        vec4 c = texture(uSurfaceTex, uv2);
        c += texture(uSurfaceTex, fract(uv2 + ofs));
        c += texture(uSurfaceTex, fract(uv2 - ofs));
        c += texture(uSurfaceTex, fract(uv2 + vec2(-ofs.x, ofs.y)));
        c += texture(uSurfaceTex, fract(uv2 + vec2(ofs.x, -ofs.y)));
        c *= 0.2;

        float lightWave = smoothstep(0.25, 0.85, swell);
        c.rgb *= mix(0.95, 1.05, lightWave);

        vec3 waterTint = vec3(0.85, 0.95, 1.05);
        c.rgb = mix(c.rgb, c.rgb * waterTint, 0.35);

        c.rgb *= uTint;
        c.a   *= uOpacity;

        outColor = c;
        return;
    }

    // fallback
    vec4 c = SampleSurface(uv);
    c.rgb *= uTint;
    c.a   *= uOpacity;
    outColor = c;
}
