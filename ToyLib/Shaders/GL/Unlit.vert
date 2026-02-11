//======================================================================
//  Unlit.vert
//  - 3D空間の板ポリ／看板／足元サイン用（完全Unlit）
//  - Phong/Mesh と uniform 名を揃えて互換性を維持
//  - Billboard/TextBillboard/FootSprite/ShadowSprite で共通利用可
//
//  注意：ToyLib は row-vector 前提（pos * World * ViewProj）
//======================================================================
#version 410 core

// ===== Phong/Mesh と共通（互換） =====
uniform mat4 uWorldTransform;
uniform mat4 uViewProj;

// （影用：使わないが互換のため宣言だけ）
uniform mat4 uLightSpaceMatrix;

// ===== Attributes（Sprite/Mesh 互換）=====
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // 未使用（互換用）
layout(location = 2) in vec2 inTexCoord;

out vec2 fragTexCoord;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);
    gl_Position  = pos * uWorldTransform * uViewProj;
    fragTexCoord = inTexCoord;
}
