#version 450

// Vertex layout: pos3 + normal3(dummy) + uv2
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vUV;

layout(push_constant) uniform Push
{
    mat4 uWorld;
    mat4 uViewProj;
    vec4 uColorAlpha; // rgb + alpha
} pc;

void main()
{
    vUV = inTexCoord;

    // GL版は row-vector っぽい書き方だったけど、
    // Vulkan(=GLSL) の標準は column-vector なので (P*V*W*pos) が自然。
    // ToyLib側の行列仕様が v*M なら、CPUで転置する or ここを合わせる必要あり。
    // いまは「一般的な列ベクトル」前提で組む：
    //gl_Position = pc.uViewProj * pc.uWorld * vec4(inPosition, 1.0);
    gl_Position = vec4(inPosition, 1.0) * pc.uWorld * pc.uViewProj;

}
