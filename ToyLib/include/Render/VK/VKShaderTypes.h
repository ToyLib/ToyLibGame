#pragma once

namespace toy
{

//==============================================================
// Scene UBO layout (Shader 側 set=0 binding=0)
//==============================================================
struct VKPointLight
{
    float position_radius[4]; // xyz=pos, w=radius
    float color_intensity[4]; // xyz=color, w=intensity
    float atten[4];           // x=constant, y=linear, z=quadratic, w=pad
};

struct VKSceneUBO
{
    float viewProj[16];
    float cameraPos[4];
    float ambient[4];
    float dirDir[4];
    float dirDiffuse[4];
    float dirSpecular[4];

    float fogColor[4];   // xyz
    float fogParams[4];  // x=minDist, y=maxDist

    // --------------------------
    // PointLights (GL互換: max 8)
    // std140 を強く意識して 16byte に揃える
    // --------------------------
    int   numPointLights;
    int   _plPad0;
    int   _plPad1;
    int   _plPad2;

    VKPointLight pointLights[8];

    // shadow (Step3)
    float shadowVP0[16];
    float shadowVP1[16];
    float shadowParams[4];
};

// SkyDome
struct VKSkyUBO
{
    float world[16];
    float timeParams[4];
    float sunDir[4];
    float moonDir[4];
    float rawSkyColor[4];
    float rawCloudColor[4];
};

// OverlayScreen / WeatherOverlay
struct VKOverlayUBO
{
    float time[4];        // x = uTime
    float resolution[4];  // x = width, y = height
    float weather[4];     // x = rain, y = snow, z = fog, w = reserved

    float sunPos[4];      // x = uSunPos.x, y = uSunPos.y
    float flare[4];       // x = flareIntensity
    float flareColor[4];  // xyz = flareColor
};



} // namespace toy
