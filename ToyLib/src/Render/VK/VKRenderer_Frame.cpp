//======================================================================
// Render/VK/VKRenderer_Core.cpp
//  - SDL3 + Vulkan (MoltenVK)
//  - Init / Shutdown / Swapchain / Depth / RenderPass / Cmd / Sync
//  - DescriptorPool / SceneUBO / SceneSet
//
// 方針（確定）:
//  - SceneUBO は World/UI 分離（mSceneUBO / mSceneUBO_UI）
//  - BeginFrame() で World UBO を更新（UpdateSceneUBO_World）
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
bool VKRenderer::BeginFrame()
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mFrames.empty())
    {
        return false;
    }

    if (mNeedRecreateSwapchain)
    {
        if (!RecreateSwapchain())
        {
            return false;
        }
        mNeedRecreateSwapchain = false;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    vkWaitForFences(mDevice, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &frame.inFlight);

    VkResult ar = vkAcquireNextImageKHR(
        mDevice, mSwapchain, UINT64_MAX,
        frame.imageAvailable, VK_NULL_HANDLE,
        &mImageIndex);

    if (ar == VK_ERROR_OUT_OF_DATE_KHR)
    {
        mNeedRecreateSwapchain = true;
        return false;
    }
    // ★SUBOPTIMAL は “成功扱い” にする（戻す）
    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
    {
        std::cerr << "[VKRenderer] Acquire failed: " << ar << "\n";
        return false;
    }

    vkResetCommandBuffer(frame.cmd, 0);

    // per-frame skinned slot cursor reset（サイズ保証込みで）
    if (mSkinnedSlotCursor.size() != mFrames.size())
    {
        mSkinnedSlotCursor.resize(mFrames.size(), 0);
    }
    mSkinnedSlotCursor[mFrameIndex] = 0;

    // Catureカウンターリセット
    mCaptureSlotCursor = 0;
    mActiveCaptureSlot = -1;
    
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frame.cmd, &bi);

    // ★重要：ここで World UBO 更新（Shadow用にも必要）
    UpdateSceneUBO_World();

    // ★重要：BaseMap(set=1) キャッシュは毎フレーム捨てる
    mBaseMapSetCache.clear();

    // renderpassはここでは開始しない
    mIsInRenderPass = false;

    return true;
}

void VKRenderer::EndFrame()
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    // 念のため：render pass を開いたままなら閉じる
    EndSwapchainRenderPassIfNeeded();

    VkResult er = vkEndCommandBuffer(frame.cmd);
    if (er != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEndCommandBuffer failed: " << er << "\n";
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &frame.imageAvailable;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &frame.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &frame.renderFinished;

    VkResult sr = vkQueueSubmit(mQueueGraphics, 1, &si, frame.inFlight);
    if (sr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueueSubmit failed: " << sr << "\n";
        return;
    }

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &frame.renderFinished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &mSwapchain;
    pi.pImageIndices      = &mImageIndex;

    VkResult pr = vkQueuePresentKHR(mQueuePresent, &pi);

    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        mNeedRecreateSwapchain = true;
    }
    else if (pr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueuePresentKHR failed: " << pr << "\n";
    }

    mFrameIndex = (mFrameIndex + 1) % (uint32_t)mFrames.size();
}

//======================================================================
// Vulkan init steps
//======================================================================
bool VKRenderer::CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pci{};
    pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = mQueueFamilyGraphics;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult vr = vkCreateCommandPool(mDevice, &pci, nullptr, &mCommandPool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateCommandPool failed: " << vr << "\n";
        return false;
    }

    const uint32_t kFrames = 2;
    mFrames.resize(kFrames);

    std::vector<VkCommandBuffer> cmds(kFrames, VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = mCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kFrames;

    vr = vkAllocateCommandBuffers(mDevice, &ai, cmds.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkAllocateCommandBuffers failed: " << vr << "\n";
        return false;
    }

    for (uint32_t i = 0; i < kFrames; ++i)
    {
        mFrames[i].cmd = cmds[i];
    }

    return true;
}

bool VKRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& f : mFrames)
    {
        if (vkCreateSemaphore(mDevice, &sci, nullptr, &f.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &sci, nullptr, &f.renderFinished) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fci, nullptr, &f.inFlight) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] Create sync objects failed.\n";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------
// RecreateSwapchain
//--------------------------------------------------------------
bool VKRenderer::RecreateSwapchain()
{
    if (mDevice == VK_NULL_HANDLE) return false;

    vkDeviceWaitIdle(mDevice);

    // swapchain dependent resources
    CleanupSwapchain();

    if (!CreateSwapchainAndViews())
        return false;

    // ★重要：depth は swapchain に依存するので必ず作り直す
    if (!CreateDepthForSwapchain())
        return false;

    if (!CreateRenderPass())
        return false;

    if (!CreateFramebuffers())
        return false;

    // pipeline は renderpass/extent に依存
    if (!BuildDefaultPipelines())
        return false;

    // shadow resources は extent に依存（CreateShadowResources の中で必要な物を再生成する前提）
    // ※DestroyShadowResources() は CleanupSwapchain では呼んでいないので、
    //   ここで「再生成が必要」なら先に破棄して作り直す。
    DestroyShadowResources();
    if (!CreateShadowResources())
        return false;

    return true;
}

void VKRenderer::CleanupSwapchain()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    if (mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    DestroyDepthForSwapchain();

    for (auto v : mSwapchainImageViews)
    {
        if (v) vkDestroyImageView(mDevice, v, nullptr);
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    if (mSwapchain)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

//--------------------------------------------------------------
// One-time command helpers
//--------------------------------------------------------------
VkCommandBuffer VKRenderer::BeginOneTimeCommands()
{
    if (!mDevice || !mCommandPool)
    {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = mCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(mDevice, &ai, &cmd) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VKRenderer::EndOneTimeCommands(VkCommandBuffer cmd)
{
    if (!cmd) return;

    vkEndCommandBuffer(cmd);

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(mDevice, &fci, nullptr, &fence);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(mQueueGraphics, 1, &si, fence);

    vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX);

    if (fence) vkDestroyFence(mDevice, fence, nullptr);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
}

} // namespace toy
