#version 410

// ===== Phong と共通 =====
uniform mat4 uWorldTransform;
uniform mat4 uViewProj;

// （使わないが互換のため）
uniform mat4 uLightSpaceMatrix;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord;

out vec2 fragTexCoord;

void main()
{
    vec4 worldPos = vec4(inPosition, 1.0) * uWorldTransform;
    gl_Position   = worldPos * uViewProj;
    fragTexCoord  = inTexCoord;
}
