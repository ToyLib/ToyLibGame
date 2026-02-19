// Render/GL/UniformNamesGL.h
#pragma once

namespace toy::glsl
{
namespace Scene
{
inline constexpr const char* ViewProj       = "uScene.viewProj";
inline constexpr const char* CameraPos      = "uScene.cameraPos";
inline constexpr const char* AmbientLight   = "uScene.ambientLight";
inline constexpr const char* SunIntensity   = "uScene.sunIntensity";

inline constexpr const char* Dir_Direction  = "uScene.dirLight.direction";
inline constexpr const char* Dir_Diffuse    = "uScene.dirLight.diffuse";
inline constexpr const char* Dir_Specular   = "uScene.dirLight.specular";

inline constexpr const char* NumPointLights = "uScene.numPointLights";
inline constexpr const char* PointPrefix    = "uScene.pointLights[";

inline constexpr const char* Fog_MaxDist    = "uScene.fog.maxDist";
inline constexpr const char* Fog_MinDist    = "uScene.fog.minDist";
inline constexpr const char* Fog_Color      = "uScene.fog.color";

inline constexpr const char* ShadowMap0     = "uScene.shadowMap0";
inline constexpr const char* ShadowMap1     = "uScene.shadowMap1";
inline constexpr const char* LightVP0       = "uScene.lightViewProj0";
inline constexpr const char* LightVP1       = "uScene.lightViewProj1";
inline constexpr const char* CascadeSplit0  = "uScene.cascadeSplit0";
inline constexpr const char* CascadeBlend   = "uScene.cascadeBlend";
inline constexpr const char* ShadowBias     = "uScene.shadowBias";
}

namespace Object
{
inline constexpr const char* World          = "uObject.world";
}

namespace Material
{
inline constexpr const char* BaseMap        = "uMaterial.baseMap";
inline constexpr const char* BaseColor      = "uMaterial.baseColor";
inline constexpr const char* UseTexture     = "uMaterial.useTexture";
inline constexpr const char* Toon           = "uMaterial.toon";

inline constexpr const char* OverrideEnable = "uMaterial.overrideEnabled";
inline constexpr const char* OverrideColor  = "uMaterial.overrideColor";

inline constexpr const char* SpecPower      = "uMaterial.specPower";
}

namespace Skinned
{
inline constexpr const char* MatrixPalette0 = "uSkinned.matrixPalette[0]";
}

//============================================================
// PostEffect (separate contract)
//============================================================
namespace Post
{
inline constexpr const char* SceneTex     = "uSceneTex";
inline constexpr const char* PostType     = "uPostType";
inline constexpr const char* Intensity    = "uIntensity";
inline constexpr const char* Time         = "uTime";
inline constexpr const char* FlipY        = "uFlipY";
inline constexpr const char* UsePaperTex  = "uUsePaperTex";
inline constexpr const char* PaperTex     = "uPaperTex";
}

//============================================================
// (Optional) Legacy names (minimize break while migrating)
//  - 共通Uniformを消す過程で、古いshaderが残ってても落ちないように。
//  - 不要になったら削除してOK。
//============================================================
namespace Legacy
{
inline constexpr const char* ViewProj          = "uViewProj";
inline constexpr const char* WorldTransform    = "uWorldTransform";
inline constexpr const char* LightSpaceMatrix  = "uLightSpaceMatrix";
}

namespace ParticleUpdate
{
inline constexpr const char* DeltaTime    = "uDeltaTime";
inline constexpr const char* Time         = "uTime";
inline constexpr const char* LifeMax      = "uLifeMax";

inline constexpr const char* EmitterPos   = "uEmitterPos";

inline constexpr const char* Mode         = "uMode";
inline constexpr const char* Gravity      = "uGravity";
inline constexpr const char* Lift         = "uLift";
inline constexpr const char* Spread       = "uSpread";

inline constexpr const char* SpawnRate    = "uSpawnRate";
inline constexpr const char* SpawnRampSec = "uSpawnRampSec";
}


} // namespace toy::glsl


