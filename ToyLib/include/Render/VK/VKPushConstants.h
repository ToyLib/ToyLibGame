#pragma once

namespace toy {

//--------------------------------------------------------------
// PushConstants
//--------------------------------------------------------------
struct VKSpritePC
{
    float world[16];
    float colorAlpha[4];
};

struct VKMeshPC
{
    float world[16];
    float baseColor_useTex[4];  // w = useTex
    float misc[4];              // x=specPower y=toon z=overrideEnabled w=alpha
    float overrideColor[4];
};

struct VKShadowPC
{
    float world[16];
};

struct VKUnlitQuadPC
{
    float world[16];
    float tintAlpha[4]; // xyz=tint, w=alpha
};

struct VKDebugPC
{
    float world[16];
    float color[4];
    float params[4]; // x=useLight
};

struct VKSkyPC
{
    float world[16];
};

struct VKFadePC
{
    float colorAlpha[4];
};

struct VKSurfacePC
{
    float world[16];
    float tintOpacity[4];
    float params0[4]; // x=flipX, y=flipY, z=mode, w=scanlineStrength
    float params1[4]; // x=time, y=distortStrength, z=fresnel, w=fresnelPow
    float params2[4]; // x=waveSpeed, y=swayStrength, z=sparkleStrength, w=isRT
};

struct VKPostEffectPC
{
    float params0[4]; // x=postType, y=intensity, z=time, w=flipY
    float params1[4]; // x=usePaperTex, y/z/w=reserved
};


} // namespcae toy
