#version 410 core

//==============================================================
// RenderSurface.vert
//  - 3D空間に置く “映像面(板ポリ)” 用
//  - RTT/Texture を貼って表示する（ビルボード無し）
//  - ToyLib 規約：vec4 * mat4（右掛け）
//==============================================================

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // 現状未使用（将来用）
layout(location = 2) in vec2 aUV;

uniform mat4 uWorld;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = vec4(aPos, 1.0) * uWorld * uView * uProj;
}
