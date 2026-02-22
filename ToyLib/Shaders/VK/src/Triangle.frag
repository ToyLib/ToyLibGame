#version 450

//==============================================================
// Material (set=1)
//==============================================================
layout(set = 1, binding = 0) uniform sampler2D uBaseMap;

//==============================================================
// Inputs
//==============================================================
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColorAlpha;

//==============================================================
// Output
//==============================================================
layout(location = 0) out vec4 outColor;

//==============================================================
// Main
//==============================================================
void main()
{
    vec4 tex = texture(uBaseMap, vTexCoord);

    // vColorAlpha.rgb = tint
    // vColorAlpha.a   = alpha
    outColor = vec4(tex.rgb * vColorAlpha.rgb, tex.a * vColorAlpha.a);
}
