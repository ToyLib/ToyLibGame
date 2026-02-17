#version 450

// set=0 : texture sampler（FS が uTexture を使う想定でも、binding一致が本質）
layout(set = 0, binding = 0) uniform sampler2D uTexture;

//------------------------------------------------------------
// set=1 : WorldCommon（Mesh_Phong.vert/frag と一致させる）
//------------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
{
    mat4 uViewProj;

    vec3 uCameraPos;     float _pad0;
    vec3 uAmbientLight;  float _pad1;

    float uFogMaxDist;
    float uFogMinDist;
    vec2  _pad2;

    vec3  uFogColor;
    float _pad3;

    mat4  uLightViewProj0;
    mat4  uLightViewProj1;

    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;
    int   uUseToon;
    float _pad4;
} sc;

//------------------------------------------------------------
// set=2 : BonePalette（Skinned 専用）
//------------------------------------------------------------
layout(set = 2, binding = 0, std140, row_major) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

//------------------------------------------------------------
// Push constants（★Mesh_Phong.frag と完全一致させる）
//------------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;

    vec4 pcDiffuse;    // xyz = diffuse
    vec4 pcUniform;    // xyz = override color
    vec4 pcFlagsSpec;  // x=useTex, y=overrideColor, z=specPower, w=unused
} pc;

//------------------------------------------------------------
// Attributes
//------------------------------------------------------------
layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

//------------------------------------------------------------
// Varyings（Mesh_Phong.frag と整合）
//------------------------------------------------------------
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
/*
void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    mat4 skinMat =
          bp.uMatrixPalette[inSkinBones.x] * inSkinWeights.x
        + bp.uMatrixPalette[inSkinBones.y] * inSkinWeights.y
        + bp.uMatrixPalette[inSkinBones.z] * inSkinWeights.z
        + bp.uMatrixPalette[inSkinBones.w] * inSkinWeights.w;

    // row-vector : v * M
    vec4 worldPos = (pos * skinMat) * pc.pcWorld;

    fragWorldPos = worldPos.xyz;
    gl_Position  = worldPos * sc.uViewProj;

    // normal
    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.pcWorld;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
*/

/*
void main()
{
    vec4 wpos = vec4(inPosition, 1.0) * pc.pcWorld;
    gl_Position = wpos * sc.uViewProj;
}
*/

void main()
{
    vec4 pos  = vec4(inPosition, 1.0);
    
    // bone 0 だけ適用（weightは 1.0 固定）
    mat4 skin = bp.uMatrixPalette[inSkinBones.x];
    
    vec4 wpos = (pos * skin) * pc.pcWorld;
    gl_Position = wpos * sc.uViewProj;
}
