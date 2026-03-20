//======================================================================
// Render/VK/VKRenderer_Core.cpp
//  - SDL3 + Vulkan (MoltenVK)
//  - Init / Shutdown / Swapchain / Depth / RenderPass / Cmd / Sync
//  - DescriptorPool / SceneUBO / SceneSet
//
// 方針（確定）:
//  - SceneUBO は World/UI 分離（mSceneUBO / mSceneUBO_UI）
//  - BeginFrame() で World UBO 更新（UpdateSceneUBO_World）
//  - DrawUIPass() 側で UI UBO を更新（UpdateSceneUBO_UI）
//  - Swapchain recreate 時は Pipeline → SceneSet の順で作り直す
//  - ★Skinned palette は slot pool を持ち、recreate時は DestroySkinnedSlots() で破棄
//======================================================================
#include "Render/VK/VKRenderer.h"

#include "Engine/Core/Application.h"
#include "Render/RenderBackendState.h"
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/Pipeline/VKPipelinePresets.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

namespace toy
{

//--------------------------------------------------------------
// ctor/dtor
//--------------------------------------------------------------
/*
bool VKRenderer::BuildDefaultPipelines()
{
    const std::string base = mShaderPath + "VK/spv/";

    //==========================================================
    // swapchain recreate 等で呼ばれるので、古い pipeline を確実に破棄
    //==========================================================
    mPipelines.DestroyAll();

    // pipeline を作り直したら setLayout が変わる可能性がある
    ClearBaseMapSetCache();

    

    //==========================================================
    // 1) Swapchain(RenderPass/Extent) 用パイプライン
    //==========================================================
    // Sprite
    {
        VKPipelineDesc sprite = toy::VKPipelinePresets::MakeSprite(base);
        if (!mPipelines.CreatePipeline("Sprite", mDevice, mRenderPass, mSwapchainExtent, sprite))
        {
            return false;
        }
    }

    // UnlitQuad
    {
        VKPipelineDesc uq = toy::VKPipelinePresets::MakeUnlitQuad(base);
        if (!mPipelines.CreatePipeline("UnlitQuad", mDevice, mRenderPass, mSwapchainExtent, uq))
        {
            return false;
        }
    }

    // UnlitWire
    {
        VKPipelineDesc wire = toy::VKPipelinePresets::MakeUnlitWire(base);
        if (!mPipelines.CreatePipeline("UnlitWire", mDevice, mRenderPass, mSwapchainExtent, wire))
        {
            return false;
        }
    }

    // Mesh + Mesh_CW
    {
        VKPipelineDesc mesh = toy::VKPipelinePresets::MakeMesh(base);

        mesh.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        mesh.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("Mesh", mDevice, mRenderPass, mSwapchainExtent, mesh))
        {
            return false;
        }

        VKPipelineDesc meshCW = mesh;
        meshCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("Mesh_CW", mDevice, mRenderPass, mSwapchainExtent, meshCW))
        {
            return false;
        }
    }

    // RenderSurface
    {
        VKPipelineDesc surf = toy::VKPipelinePresets::MakeRenderSurface(base);

        surf.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        surf.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("RenderSurface", mDevice, mRenderPass, mSwapchainExtent, surf))
        {
            return false;
        }

        // 必要になったら CW 版も追加可能
        // VKPipelineDesc surfCW = surf;
        // surfCW.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // if (!mPipelines.CreatePipeline("RenderSurface_CW", mDevice, mRenderPass, mSwapchainExtent, surfCW))
        // {
        //     return false;
        // }
    }

    // SkyDome
    {
        VKPipelineDesc sky = toy::VKPipelinePresets::MakeSkyDome(base);
        if (!mPipelines.CreatePipeline("SkyDome", mDevice, mRenderPass, mSwapchainExtent, sky))
        {
            return false;
        }
    }

    // ★ WeatherOverlay
    {
        VKPipelineDesc ov = toy::VKPipelinePresets::MakeWeatherOverlay(base);
        if (!mPipelines.CreatePipeline("WeatherOverlay", mDevice, mRenderPass, mSwapchainExtent, ov))
        {
            return false;
        }
    }

    // ★ WeatherOverlayAdd
    {
        VKPipelineDesc ovAdd = toy::VKPipelinePresets::MakeWeatherOverlayAdd(base);
        if (!mPipelines.CreatePipeline("WeatherOverlayAdd", mDevice, mRenderPass, mSwapchainExtent, ovAdd))
        {
            return false;
        }
    }

    // Fade
    {
        VKPipelineDesc fade = toy::VKPipelinePresets::MakeFade(base);
        if (!mPipelines.CreatePipeline("Fade", mDevice, mRenderPass, mSwapchainExtent, fade))
        {
            return false;
        }
    }
    
    
    // PostEffect
    {
        mPostEffectSets.clear();
        mPostEffectSetLayout = VK_NULL_HANDLE;
        
        VKPipelineDesc post = toy::VKPipelinePresets::MakePostEffect(base);
        if (!mPipelines.CreatePipeline("PostEffect", mDevice, mRenderPass, mSwapchainExtent, post))
        {
            return false;
        }

        VKPipeline* pipe = mPipelines.Get("PostEffect");
        if (!pipe || !pipe->IsValid())
        {
            return false;
        }

        mPostEffectSetLayout = pipe->GetSetLayout(0);
        if (mPostEffectSetLayout == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] PostEffect set0 layout null\n";
            return false;
        }
    }
    
    // SkinnedMesh + SkinnedMesh_CW
    {
        VKPipelineDesc sk = toy::VKPipelinePresets::MakeSkinnedMesh(base);

        sk.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        sk.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("SkinnedMesh", mDevice, mRenderPass, mSwapchainExtent, sk))
        {
            return false;
        }

        VKPipelineDesc skCW = sk;
        skCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("SkinnedMesh_CW", mDevice, mRenderPass, mSwapchainExtent, skCW))
        {
            return false;
        }
    }
    
    // Particle
    {
        VKPipelineDesc particle = toy::VKPipelinePresets::MakeParticle(base);
        if (!mPipelines.CreatePipeline("Particle", mDevice, mRenderPass, mSwapchainExtent, particle))
        {
            return false;
        }
    }

    //==========================================================
    // 2) Shadow(RenderPass/Extent) 用パイプライン
    //==========================================================
    if (mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowExtent.width > 0 && mShadowExtent.height > 0)
    {
        // Shadow_Mesh
        {
            VKPipelineDesc sd = toy::VKPipelinePresets::MakeShadowMesh(base);

            sd.cullMode  = VK_CULL_MODE_BACK_BIT;
            sd.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowMesh", mDevice, mShadowRenderPass, mShadowExtent, sd))
            {
                return false;
            }

            VKPipelineDesc sdCW = sd;
            sdCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowMesh_CW", mDevice, mShadowRenderPass, mShadowExtent, sdCW))
            {
                return false;
            }
        }

        // ShadowSkinned
        {
            VKPipelineDesc sd = toy::VKPipelinePresets::MakeShadowSkinnedMesh(base);

            sd.cullMode  = VK_CULL_MODE_BACK_BIT;
            sd.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowSkinned", mDevice, mShadowRenderPass, mShadowExtent, sd))
            {
                return false;
            }

            VKPipelineDesc sdCW = sd;
            sdCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowSkinned_CW", mDevice, mShadowRenderPass, mShadowExtent, sdCW))
            {
                return false;
            }
        }
    }

    return true;
}
*/
bool VKRenderer::BuildDefaultPipelines()
{
    const std::string base = mShaderPath + "VK/spv/";

    //==========================================================
    // swapchain recreate 等で呼ばれるので、古い pipeline を確実に破棄
    //==========================================================
    mPipelines.DestroyAll();

    // pipeline を作り直したら setLayout が変わる可能性がある
    ClearBaseMapSetCache();

    // PostEffect は pipeline recreate 後に layout を取り直す
    mPostEffectSets.clear();
    mPostEffectSetLayout = VK_NULL_HANDLE;

    //==========================================================
    // 1) Swapchain(RenderPass/Extent) 用パイプライン
    //==========================================================
    // Sprite
    {
        VKPipelineDesc sprite = toy::VKPipelinePresets::MakeSprite(base);
        if (!mPipelines.CreatePipeline("Sprite", mDevice, mRenderPass, mSwapchainExtent, sprite))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Sprite\n";
            return false;
        }
    }

    // UnlitQuad
    {
        VKPipelineDesc uq = toy::VKPipelinePresets::MakeUnlitQuad(base);
        if (!mPipelines.CreatePipeline("UnlitQuad", mDevice, mRenderPass, mSwapchainExtent, uq))
        {
            std::cerr << "[VKRenderer] Failed pipeline: UnlitQuad\n";
            return false;
        }
    }

    // UnlitWire
    {
        VKPipelineDesc wire = toy::VKPipelinePresets::MakeUnlitWire(base);
        if (!mPipelines.CreatePipeline("UnlitWire", mDevice, mRenderPass, mSwapchainExtent, wire))
        {
            std::cerr << "[VKRenderer] Failed pipeline: UnlitWire\n";
            return false;
        }
    }

    // Mesh + Mesh_CW
    {
        VKPipelineDesc mesh = toy::VKPipelinePresets::MakeMesh(base);

        mesh.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        mesh.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("Mesh", mDevice, mRenderPass, mSwapchainExtent, mesh))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Mesh\n";
            return false;
        }

        VKPipelineDesc meshCW = mesh;
        meshCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("Mesh_CW", mDevice, mRenderPass, mSwapchainExtent, meshCW))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Mesh_CW\n";
            return false;
        }
    }

    // RenderSurface
    {
        VKPipelineDesc surf = toy::VKPipelinePresets::MakeRenderSurface(base);

        surf.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        surf.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("RenderSurface", mDevice, mRenderPass, mSwapchainExtent, surf))
        {
            std::cerr << "[VKRenderer] Failed pipeline: RenderSurface\n";
            return false;
        }

        // 必要になったら CW 版も追加可能
        // std::cerr << "[VKRenderer] Creating pipeline: RenderSurface_CW\n";
        // VKPipelineDesc surfCW = surf;
        // surfCW.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // if (!mPipelines.CreatePipeline("RenderSurface_CW", mDevice, mRenderPass, mSwapchainExtent, surfCW))
        // {
        //     std::cerr << "[VKRenderer] Failed pipeline: RenderSurface_CW\n";
        //     return false;
        // }
    }

    // SkyDome
    {
        VKPipelineDesc sky = toy::VKPipelinePresets::MakeSkyDome(base);
        if (!mPipelines.CreatePipeline("SkyDome", mDevice, mRenderPass, mSwapchainExtent, sky))
        {
            std::cerr << "[VKRenderer] Failed pipeline: SkyDome\n";
            return false;
        }
    }

    // WeatherOverlay
    {
        VKPipelineDesc ov = toy::VKPipelinePresets::MakeWeatherOverlay(base);
        if (!mPipelines.CreatePipeline("WeatherOverlay", mDevice, mRenderPass, mSwapchainExtent, ov))
        {
            std::cerr << "[VKRenderer] Failed pipeline: WeatherOverlay\n";
            return false;
        }
    }

    // WeatherOverlayAdd
    {
        VKPipelineDesc ovAdd = toy::VKPipelinePresets::MakeWeatherOverlayAdd(base);
        if (!mPipelines.CreatePipeline("WeatherOverlayAdd", mDevice, mRenderPass, mSwapchainExtent, ovAdd))
        {
            std::cerr << "[VKRenderer] Failed pipeline: WeatherOverlayAdd\n";
            return false;
        }
    }

    // Fade
    {
        VKPipelineDesc fade = toy::VKPipelinePresets::MakeFade(base);
        if (!mPipelines.CreatePipeline("Fade", mDevice, mRenderPass, mSwapchainExtent, fade))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Fade\n";
            return false;
        }
    }

    // PostEffect
    {

        VKPipelineDesc post = toy::VKPipelinePresets::MakePostEffect(base);
        if (!mPipelines.CreatePipeline("PostEffect", mDevice, mRenderPass, mSwapchainExtent, post))
        {
            std::cerr << "[VKRenderer] Failed pipeline: PostEffect\n";
            return false;
        }

        VKPipeline* pipe = mPipelines.Get("PostEffect");
        if (!pipe || !pipe->IsValid())
        {
            std::cerr << "[VKRenderer] Invalid pipeline after create: PostEffect\n";
            return false;
        }

        mPostEffectSetLayout = pipe->GetSetLayout(0);
        if (mPostEffectSetLayout == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] PostEffect set0 layout null\n";
            return false;
        }
    }

    // SkinnedMesh + SkinnedMesh_CW
    {
        VKPipelineDesc sk = toy::VKPipelinePresets::MakeSkinnedMesh(base);

        sk.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        sk.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("SkinnedMesh", mDevice, mRenderPass, mSwapchainExtent, sk))
        {
            std::cerr << "[VKRenderer] Failed pipeline: SkinnedMesh\n";
            return false;
        }

        VKPipelineDesc skCW = sk;
        skCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("SkinnedMesh_CW", mDevice, mRenderPass, mSwapchainExtent, skCW))
        {
            std::cerr << "[VKRenderer] Failed pipeline: SkinnedMesh_CW\n";
            return false;
        }
    }

    // Particle
    {
        VKPipelineDesc particle = toy::VKPipelinePresets::MakeParticle(base);
        if (!mPipelines.CreatePipeline("Particle", mDevice, mRenderPass, mSwapchainExtent, particle))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Particle\n";
            return false;
        }
        particle.blendMode = VKPipelineDesc::BlendMode::Alpha;
        if (!mPipelines.CreatePipeline("Particle_Alpha", mDevice, mRenderPass, mSwapchainExtent, particle))
        {
            std::cerr << "[VKRenderer] Failed pipeline: Particle_Alpha\n";
            return false;
        }

    }


    //==========================================================
    // 2) Shadow(RenderPass/Extent) 用パイプライン
    //==========================================================
    if (mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowExtent.width > 0 && mShadowExtent.height > 0)
    {
        // ShadowMesh + ShadowMesh_CW
        {
            VKPipelineDesc sd = toy::VKPipelinePresets::MakeShadowMesh(base);

            sd.cullMode  = VK_CULL_MODE_BACK_BIT;
            sd.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowMesh", mDevice, mShadowRenderPass, mShadowExtent, sd))
            {
                std::cerr << "[VKRenderer] Failed pipeline: ShadowMesh\n";
                return false;
            }

            VKPipelineDesc sdCW = sd;
            sdCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowMesh_CW", mDevice, mShadowRenderPass, mShadowExtent, sdCW))
            {
                std::cerr << "[VKRenderer] Failed pipeline: ShadowMesh_CW\n";
                return false;
            }
        }

        // ShadowSkinned + ShadowSkinned_CW
        {
            VKPipelineDesc sd = toy::VKPipelinePresets::MakeShadowSkinnedMesh(base);

            sd.cullMode  = VK_CULL_MODE_BACK_BIT;
            sd.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowSkinned", mDevice, mShadowRenderPass, mShadowExtent, sd))
            {
                std::cerr << "[VKRenderer] Failed pipeline: ShadowSkinned\n";
                return false;
            }

            VKPipelineDesc sdCW = sd;
            sdCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

            if (!mPipelines.CreatePipeline("ShadowSkinned_CW", mDevice, mShadowRenderPass, mShadowExtent, sdCW))
            {
                std::cerr << "[VKRenderer] Failed pipeline: ShadowSkinned_CW\n";
                return false;
            }
        }
    }

    return true;
}

bool VKRenderer::BuildShadowPipelinesOnly()
{
    // .h に mEnableShadow が無いので、存在チェックだけで判定する
    if (mShadowRenderPass == VK_NULL_HANDLE ||
        mShadowExtent.width == 0 || mShadowExtent.height == 0)
    {
        return true; // 影がまだ無い/無効ならスキップ
    }

    const std::string base = mShaderPath + "VK/spv/"; // ★他と揃える

    // PipelineLibrary の API はあなたの実装に合わせる：
    // ここは Core.cpp 上の CreatePipeline 呼びに揃える
    if (!mPipelines.CreatePipeline(
            "ShadowMesh",
            mDevice,
            mShadowRenderPass,
            mShadowExtent,
            VKPipelinePresets::MakeShadowMesh(base)))
        return false;

    if (!mPipelines.CreatePipeline(
            "ShadowSkinned",
            mDevice,
            mShadowRenderPass,
            mShadowExtent,
            VKPipelinePresets::MakeShadowSkinnedMesh(base)))
        return false;

    return true;
}

} // namespace toy
