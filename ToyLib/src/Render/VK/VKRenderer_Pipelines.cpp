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

    //==========================================================
    // 2) Shadow(RenderPass/Extent) 用パイプライン
    //   - ここが今回のキモ：renderPass と extent を “Shadow用” にする
    //==========================================================
    // Shadow resources がまだ無い/無効ならスキップ可
    // （CreateShadowResources() が成功していれば、mShadowRenderPass が有効な想定）
    if (mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowExtent.width > 0 && mShadowExtent.height > 0)
    {
        // Shadow_Mesh
        {
            VKPipelineDesc sd = toy::VKPipelinePresets::MakeShadowMesh(base);

            // 影は基本 “両面” にしたいなら cull none
            // まずは GL に合わせて back cull でもOK
            sd.cullMode   = VK_CULL_MODE_BACK_BIT;
            sd.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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

            sd.cullMode   = VK_CULL_MODE_BACK_BIT;
            sd.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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
