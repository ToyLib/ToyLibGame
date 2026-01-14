#version 410

// ===== Phong と共通 =====
uniform sampler2D uTexture;

uniform vec3 uUniformColor;
uniform bool uOverrideColor;

uniform vec3  uCameraPos;
uniform float uSpecPower;
uniform vec3  uAmbientLight;

// Fog 構造体（使わないが定義だけ）
struct FogInfo
{
    float maxDist;
    float minDist;
    vec3  color;
};
uniform FogInfo uFoginfo;

// ===== Unlit 用 =====
uniform bool uUseTexture;
uniform vec3 uDiffuseColor;

in vec2 fragTexCoord;
out vec4 outColor;

void main()
{
    vec4 baseColor;

    baseColor = texture(uTexture, fragTexCoord);

    // 完全 Unlit（フォグ・影・ライト無視）
    outColor = baseColor;
}
