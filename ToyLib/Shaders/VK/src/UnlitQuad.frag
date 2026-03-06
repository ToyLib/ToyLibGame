#version 450

//======================================================================
// UnlitQuad.frag
//  - テクスチャ * tint / alpha
//  - Lighting / Shadow は使わない
//======================================================================

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

//--------------------------------------------------------------------
// set=1 : BaseMap
//--------------------------------------------------------------------
layout(set = 1, binding = 0) uniform sampler2D uBaseMap;

//--------------------------------------------------------------------
// Push Constants
//--------------------------------------------------------------------
layout(push_constant) uniform UnlitQuadPC
{
    mat4 world;
    vec4 tintAlpha; // xyz=tint, w=alpha
} pc;

void main()
{
    vec4 base = texture(uBaseMap, vTexCoord);

    base.rgb *= pc.tintAlpha.rgb;
    base.a   *= pc.tintAlpha.a;

    outColor = base;
}
