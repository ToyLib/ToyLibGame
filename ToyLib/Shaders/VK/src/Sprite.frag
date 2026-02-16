#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// set=0 binding=0 : texture
layout(set = 0, binding = 0) uniform sampler2D uTexture;

// set=1 binding=0 : SpriteCommon（使わないが set を揃えるため残す）
layout(set = 1, binding = 0, std140) uniform SpriteCommon
{
    mat4 uViewProj;
} sc;

// push (VSと完全一致)
layout(push_constant) uniform Push
{
    mat4 pcWorld;
    vec4 pcColorAlpha; // rgb + a
} pc;

void main()
{
    vec4 tex = texture(uTexture, fragTexCoord);
    outColor = vec4(tex.rgb * pc.pcColorAlpha.rgb, tex.a * pc.pcColorAlpha.a);
}
