// Render/VK/VKRenderer.cpp
#include "Render/VK/VKRenderer.h"

#include "Engine/Core/Application.h"

#include <iostream>
#include <cstring>
#include <algorithm>

namespace toy
{

//==============================================================================
// Debug utils (optional)
//==============================================================================
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             type,
    const VkDebugUtilsMessengerCallbackDataEXT* cb,
    void*                                       user)
{
    (void)type;
    (void)user;

    const char* sev = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) sev = "WARN";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   sev = "ERROR";

    std::cerr << "[VK][" << sev << "] " << (cb && cb->pMessage ? cb->pMessage : "(null)") << "\n";
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* ci,
    const VkAllocationCallbacks* alloc,
    VkDebugUtilsMessengerEXT* out)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, ci, alloc, out);
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT msgr,
    const VkAllocationCallbacks* alloc)
{
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (!fn) return;
    fn(instance, msgr, alloc);
}

//==============================================================================
// Helpers: Queue families / Swapchain support
//==============================================================================
struct QueueFamilyIndices
{
    bool     hasGraphics = false;
    bool     hasPresent  = false;
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily  = 0;

    bool IsComplete() const
    {
        return hasGraphics && hasPresent;
    }
};

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    QueueFamilyIndices out {};

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    if (count == 0) return out;

    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (!out.hasGraphics && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            out.hasGraphics = true;
            out.graphicsFamily = i;
        }

        if (!out.hasPresent)
        {
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &supported);
            if (supported)
            {
                out.hasPresent = true;
                out.presentFamily = i;
            }
        }

        if (out.IsComplete())
        {
            break;
        }
    }

    return out;
}

static bool CheckDeviceExtensionSupport(VkPhysicalDevice dev)
{
    const char* requiredExts[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    if (count == 0) return false;

    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());

    for (const char* req : requiredExts)
    {
        bool found = false;
        for (const auto& e : exts)
        {
            if (std::strcmp(req, e.extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

struct SwapchainSupport
{
    VkSurfaceCapabilitiesKHR        caps {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;

    bool IsUsable() const
    {
        return !formats.empty() && !presentModes.empty();
    }
};

static SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    SwapchainSupport out {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &out.caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
    out.formats.resize(fmtCount);
    if (fmtCount > 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, out.formats.data());
    }

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, nullptr);
    out.presentModes.resize(pmCount);
    if (pmCount > 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, out.presentModes.data());
    }

    return out;
}

static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    // Prefer SRGB if available
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    return formats[0];
}

static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    // Prefer MAILBOX, fallback FIFO (always supported)
    for (auto m : modes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window)
{
    if (caps.currentExtent.width != UINT32_MAX)
    {
        return caps.currentExtent;
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    VkExtent2D e {};
    e.width  = (uint32_t)std::clamp(w, (int)caps.minImageExtent.width,  (int)caps.maxImageExtent.width);
    e.height = (uint32_t)std::clamp(h, (int)caps.minImageExtent.height, (int)caps.maxImageExtent.height);
    return e;
}

static int ScoreDevice(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    const auto q = FindQueueFamilies(dev, surface);
    if (!q.IsComplete()) return -1;

    if (!CheckDeviceExtensionSupport(dev)) return -1;

    const auto sw = QuerySwapchainSupport(dev, surface);
    if (!sw.IsUsable()) return -1;

    VkPhysicalDeviceProperties prop {};
    vkGetPhysicalDeviceProperties(dev, &prop);

    int score = 0;
    if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;

    score += (int)prop.limits.maxImageDimension2D / 1024;
    return score;
}

//==============================================================================
// VKRenderer
//==============================================================================
VKRenderer::VKRenderer()
{
}

VKRenderer::~VKRenderer()
{
    Shutdown();
}

bool VKRenderer::Initialize(const Application* app)
{
    if (!app)
    {
        return false;
    }

    mSDLWindow = app->GetSDLWindow();
    if (!mSDLWindow)
    {
        std::cerr << "[VK] SDL_Window is null.\n";
        return false;
    }

    // screen size init (for IRenderer helpers)
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(mSDLWindow, &w, &h);
    mScreenWidth  = (float)w;
    mScreenHeight = (float)h;

    if (!CreateInstance(mSDLWindow)) return false;
    if (!CreateDebugMessenger()) return false;
    if (!CreateSurface(mSDLWindow)) return false;

    if (!PickPhysicalDevice()) return false;
    if (!CreateDeviceAndQueues()) return false;

    if (!CreateSwapchain()) return false;
    if (!CreateImageViews()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;

    if (!CreateCommandPool()) return false;
    if (!CreateCommandBuffers()) return false;

    if (!CreateSyncObjects()) return false;

    std::cerr << "[VK] Initialize OK (clear-only).\n";
    return true;
}

void VKRenderer::Shutdown()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }

    // sync
    for (auto f : mInFlightFences)
    {
        if (f) vkDestroyFence(mDevice, f, nullptr);
    }
    for (auto s : mImageAvailable)
    {
        if (s) vkDestroySemaphore(mDevice, s, nullptr);
    }
    for (auto s : mRenderFinished)
    {
        if (s) vkDestroySemaphore(mDevice, s, nullptr);
    }
    mInFlightFences.clear();
    mImageAvailable.clear();
    mRenderFinished.clear();
    mImagesInFlight.clear();

    // commands
    if (mCmdPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(mDevice, mCmdPool, nullptr);
        mCmdPool = VK_NULL_HANDLE;
    }
    mCmdBuffers.clear();

    CleanupSwapchain();

    DestroyDeviceRelated();
    DestroyInstanceRelated();

    mSDLWindow = nullptr;
}

void VKRenderer::WaitIdle()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    (void)name;
    // Clear-only sample: no pipelines yet
    return PipelineHandle {};
}

void VKRenderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0) return;

    mScreenWidth  = (float)pixelW;
    mScreenHeight = (float)pixelH;

    mFramebufferResized = true;
}

//==============================================================================
// IRenderer phases
//==============================================================================
bool VKRenderer::BeginFrame()
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE)
    {
        return false;
    }

    if (mFramebufferResized)
    {
        if (!RecreateSwapchain())
        {
            return false;
        }
        mFramebufferResized = false;
    }

    const uint32_t cur = mFrameIndex;

    vkWaitForFences(mDevice, 1, &mInFlightFences[cur], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(
        mDevice,
        mSwapchain,
        UINT64_MAX,
        mImageAvailable[cur],
        VK_NULL_HANDLE,
        &mImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return RecreateSwapchain();
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
    {
        std::cerr << "[VK] vkAcquireNextImageKHR failed: " << res << "\n";
        return false;
    }

    // If a previous frame is using this image, wait for it
    if (mImagesInFlight[mImageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(mDevice, 1, &mImagesInFlight[mImageIndex], VK_TRUE, UINT64_MAX);
    }
    mImagesInFlight[mImageIndex] = mInFlightFences[cur];

    vkResetFences(mDevice, 1, &mInFlightFences[cur]);

    mFrameBegan = true;
    return true;
}

void VKRenderer::DrawShadowPass()
{
    // clear-only sample: none
}

void VKRenderer::RestoreAfterShadowPass()
{
    // clear-only sample: none
}

void VKRenderer::DrawSkyPass()
{
    // clear-only sample: none
}

void VKRenderer::DrawWorldPass()
{
    // Here: record command buffer that clears the screen
    if (!mFrameBegan) return;

    if (!RecordCommandBuffer(mImageIndex))
    {
        std::cerr << "[VK] RecordCommandBuffer failed.\n";
        return;
    }
}

void VKRenderer::DrawOverlayScreenPass()
{
    // clear-only sample: none
}

void VKRenderer::DrawFadePass()
{
    // clear-only sample: none
}

void VKRenderer::DrawPostEffectPass()
{
    // clear-only sample: none
}

void VKRenderer::DrawUIPass()
{
    // clear-only sample: none
}

void VKRenderer::EndFrame()
{
    if (!mFrameBegan) return;

    if (!Present(mImageIndex))
    {
        // Present can trigger swapchain recreate
    }

    mFrameIndex = (mFrameIndex + 1) % kMaxFramesInFlight;
    mFrameBegan = false;
}

//==============================================================================
// Record/Submit/Present
//==============================================================================
bool VKRenderer::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer cmd = mCmdBuffers[imageIndex];

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
    {
        return false;
    }

    // Clear color uses IRenderer's clear color
    VkClearValue clear {};
    clear.color.float32[0] = mClearColor.x;
    clear.color.float32[1] = mClearColor.y;
    clear.color.float32[2] = mClearColor.z;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = mRenderPass;
    rp.framebuffer = mFramebuffers[imageIndex];
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = mSwapchainExtent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // Nothing drawn. Just clear.

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
    {
        return false;
    }

    // Submit
    const uint32_t cur = mFrameIndex;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &mImageAvailable[cur];
    si.pWaitDstStageMask = &waitStage;

    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &mRenderFinished[cur];

    VkResult res = vkQueueSubmit(mGraphicsQueue, 1, &si, mInFlightFences[cur]);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkQueueSubmit failed: " << res << "\n";
        return false;
    }

    // debug counter
    AddDrawCall();
    return true;
}

bool VKRenderer::Present(uint32_t imageIndex)
{
    const uint32_t cur = mFrameIndex;

    VkPresentInfoKHR pi {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &mRenderFinished[cur];

    pi.swapchainCount = 1;
    pi.pSwapchains = &mSwapchain;
    pi.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(mPresentQueue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || mFramebufferResized)
    {
        mFramebufferResized = false;
        return RecreateSwapchain();
    }

    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkQueuePresentKHR failed: " << res << "\n";
        return false;
    }

    return true;
}

//==============================================================================
// Swapchain recreate
//==============================================================================
bool VKRenderer::RecreateSwapchain()
{
    if (!mSDLWindow) return false;

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(mSDLWindow, &w, &h);
    if (w <= 0 || h <= 0)
    {
        // minimized etc.
        return true;
    }

    vkDeviceWaitIdle(mDevice);

    CleanupSwapchain();

    if (!CreateSwapchain()) return false;
    if (!CreateImageViews()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;

    if (!CreateCommandBuffers()) return false;

    // images-in-flight size changed
    mImagesInFlight.assign(mSwapchainImages.size(), VK_NULL_HANDLE);

    return true;
}

void VKRenderer::CleanupSwapchain()
{
    if (mDevice == VK_NULL_HANDLE) return;

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    if (mRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    for (auto iv : mSwapchainImageViews)
    {
        if (iv) vkDestroyImageView(mDevice, iv, nullptr);
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    if (mSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

//------------------------------------------------------------
// helper : layer存在チェック
//------------------------------------------------------------
static bool HasLayer(const char* name)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    std::vector<VkLayerProperties> props(count);
    vkEnumerateInstanceLayerProperties(&count, props.data());

    for (const auto& p : props)
    {
        if (std::strcmp(p.layerName, name) == 0)
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------
// CreateInstance
//------------------------------------------------------------
bool VKRenderer::CreateInstance(SDL_Window* window)
{
    if (!window)
    {
        std::cerr << "[VK] CreateInstance: window null\n";
        return false;
    }

    //--------------------------------------------------------
    // App info
    //--------------------------------------------------------
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ToyLib";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "ToyLib";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    //--------------------------------------------------------
    // Extensions (SDL)
    //--------------------------------------------------------
    uint32_t extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr))
    {
        std::cerr << "[VK] SDL_Vulkan_GetInstanceExtensions(count) failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    std::vector<const char*> exts(extCount);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &extCount, exts.data()))
    {
        std::cerr << "[VK] SDL_Vulkan_GetInstanceExtensions(list) failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    //--------------------------------------------------------
    // mac / MoltenVK portability
    //--------------------------------------------------------
#ifdef __APPLE__
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    exts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    //--------------------------------------------------------
    // Validation layer (存在チェック)
    //--------------------------------------------------------
    std::vector<const char*> layers;

#if !defined(NDEBUG)
    const char* kValidation = "VK_LAYER_KHRONOS_validation";
    if (HasLayer(kValidation))
    {
        layers.push_back(kValidation);
    }
    else
    {
        std::cerr << "[VK] Validation layer not found -> continue without it\n";
    }
#endif

    //--------------------------------------------------------
    // Instance create info
    //--------------------------------------------------------
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;

    ci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();

    ci.enabledLayerCount   = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

#ifdef __APPLE__
    ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    //--------------------------------------------------------
    // Create
    //--------------------------------------------------------
    VkResult res = vkCreateInstance(&ci, nullptr, &mInstance);

    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkCreateInstance failed: " << res << "\n";

        std::cerr << "[VK] enabled layers:\n";
        for (auto* s : layers) std::cerr << "  " << s << "\n";

        std::cerr << "[VK] enabled extensions:\n";
        for (auto* s : exts) std::cerr << "  " << s << "\n";

        return false;
    }

    std::cout << "[VK] Instance created\n";
    return true;
}

bool VKRenderer::CreateDebugMessenger()
{
#if defined(NDEBUG)
    return true;
#else
    if (mInstance == VK_NULL_HANDLE) return false;

    VkDebugUtilsMessengerCreateInfoEXT ci {};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = DebugCallback;

    VkResult res = CreateDebugUtilsMessengerEXT(mInstance, &ci, nullptr, &mDebugMsgr);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] CreateDebugUtilsMessengerEXT failed: " << res << "\n";
        // Not fatal
        mDebugMsgr = VK_NULL_HANDLE;
        return true;
    }

    return true;
#endif
}

bool VKRenderer::CreateSurface(SDL_Window* window)
{
    if (mInstance == VK_NULL_HANDLE) return false;

    if (!SDL_Vulkan_CreateSurface(window, mInstance, nullptr, &mSurface))
    {
        std::cerr << "[VK] SDL_Vulkan_CreateSurface failed.\n";
        return false;
    }

    return true;
}

//==============================================================================
// Device selection / create
//==============================================================================
bool VKRenderer::PickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    if (count == 0)
    {
        std::cerr << "[VK] No physical devices.\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(mInstance, &count, devs.data());

    VkPhysicalDevice best = VK_NULL_HANDLE;
    int bestScore = -1;

    for (auto d : devs)
    {
        int score = ScoreDevice(d, mSurface);
        if (score > bestScore)
        {
            bestScore = score;
            best = d;
        }
    }

    if (best == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] No suitable GPU found.\n";
        return false;
    }

    mPhysicalDevice = best;

    const auto q = FindQueueFamilies(mPhysicalDevice, mSurface);
    mGraphicsFamily = q.graphicsFamily;
    mPresentFamily  = q.presentFamily;

    VkPhysicalDeviceProperties prop {};
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &prop);
    std::cerr << "[VK] Selected GPU: " << prop.deviceName << "\n";

    return true;
}

bool VKRenderer::CreateDeviceAndQueues()
{
    const float priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.reserve(2);

    // graphics
    {
        VkDeviceQueueCreateInfo qci {};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = mGraphicsFamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    // present if separate
    if (mPresentFamily != mGraphicsFamily)
    {
        VkDeviceQueueCreateInfo qci {};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = mPresentFamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    const char* devExts[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures features {};

    VkDeviceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)qcis.size();
    ci.pQueueCreateInfos = qcis.data();
    ci.enabledExtensionCount = (uint32_t)std::size(devExts);
    ci.ppEnabledExtensionNames = devExts;
    ci.pEnabledFeatures = &features;

    VkResult res = vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkCreateDevice failed: " << res << "\n";
        return false;
    }

    vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, mPresentFamily,  0, &mPresentQueue);

    return (mGraphicsQueue != VK_NULL_HANDLE && mPresentQueue != VK_NULL_HANDLE);
}

//==============================================================================
// Swapchain / RenderPass / FB
//==============================================================================
bool VKRenderer::CreateSwapchain()
{
    SwapchainSupport sw = QuerySwapchainSupport(mPhysicalDevice, mSurface);
    if (!sw.IsUsable())
    {
        std::cerr << "[VK] Swapchain support not usable.\n";
        return false;
    }

    VkSurfaceFormatKHR fmt = ChooseSurfaceFormat(sw.formats);
    VkPresentModeKHR   pm  = ChoosePresentMode(sw.presentModes);
    VkExtent2D         ext = ChooseExtent(sw.caps, mSDLWindow);

    uint32_t imageCount = sw.caps.minImageCount + 1;
    if (sw.caps.maxImageCount > 0 && imageCount > sw.caps.maxImageCount)
    {
        imageCount = sw.caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci {};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = mSurface;
    ci.minImageCount = imageCount;
    ci.imageFormat = fmt.format;
    ci.imageColorSpace = fmt.colorSpace;
    ci.imageExtent = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t qFamilyIndices[] = { mGraphicsFamily, mPresentFamily };

    if (mGraphicsFamily != mPresentFamily)
    {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = qFamilyIndices;
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = nullptr;
    }

    ci.preTransform = sw.caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = pm;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(mDevice, &ci, nullptr, &mSwapchain);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkCreateSwapchainKHR failed: " << res << "\n";
        return false;
    }

    // images
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &count, nullptr);
    mSwapchainImages.resize(count);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &count, mSwapchainImages.data());

    mSwapchainFormat = fmt.format;
    mSwapchainExtent = ext;

    // images-in-flight init here if first time
    mImagesInFlight.assign(mSwapchainImages.size(), VK_NULL_HANDLE);

    return true;
}

bool VKRenderer::CreateImageViews()
{
    mSwapchainImageViews.resize(mSwapchainImages.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo ci {};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = mSwapchainImages[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = mSwapchainFormat;

        ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(mDevice, &ci, nullptr, &mSwapchainImageViews[i]);
        if (res != VK_SUCCESS)
        {
            std::cerr << "[VK] vkCreateImageView failed: " << res << "\n";
            return false;
        }
    }

    return true;
}

bool VKRenderer::CreateRenderPass()
{
    // single color attachment
    VkAttachmentDescription color {};
    color.format = mSwapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub {};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;

    // basic dependency
    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &color;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    VkResult res = vkCreateRenderPass(mDevice, &ci, nullptr, &mRenderPass);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkCreateRenderPass failed: " << res << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateFramebuffers()
{
    mFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { mSwapchainImageViews[i] };

        VkFramebufferCreateInfo ci {};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = mRenderPass;
        ci.attachmentCount = 1;
        ci.pAttachments = attachments;
        ci.width  = mSwapchainExtent.width;
        ci.height = mSwapchainExtent.height;
        ci.layers = 1;

        VkResult res = vkCreateFramebuffer(mDevice, &ci, nullptr, &mFramebuffers[i]);
        if (res != VK_SUCCESS)
        {
            std::cerr << "[VK] vkCreateFramebuffer failed: " << res << "\n";
            return false;
        }
    }

    return true;
}

//==============================================================================
// Commands / Sync
//==============================================================================
bool VKRenderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = mGraphicsFamily;

    VkResult res = vkCreateCommandPool(mDevice, &ci, nullptr, &mCmdPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkCreateCommandPool failed: " << res << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateCommandBuffers()
{
    if (mCmdPool == VK_NULL_HANDLE) return false;

    // Free old buffers automatically by resetting pool or destroying pool.
    // Here: just re-allocate (pool is same)
    mCmdBuffers.resize(mSwapchainImages.size(), VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = mCmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)mCmdBuffers.size();

    VkResult res = vkAllocateCommandBuffers(mDevice, &ai, mCmdBuffers.data());
    if (res != VK_SUCCESS)
    {
        std::cerr << "[VK] vkAllocateCommandBuffers failed: " << res << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateSyncObjects()
{
    mImageAvailable.resize(kMaxFramesInFlight, VK_NULL_HANDLE);
    mRenderFinished.resize(kMaxFramesInFlight, VK_NULL_HANDLE);
    mInFlightFences.resize(kMaxFramesInFlight, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo sci {};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fci {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // first frame doesn't wait forever

    for (int i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (vkCreateSemaphore(mDevice, &sci, nullptr, &mImageAvailable[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(mDevice, &sci, nullptr, &mRenderFinished[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(mDevice, &fci, nullptr, &mInFlightFences[i]) != VK_SUCCESS) return false;
    }

    // per-image in-flight fence tracking
    mImagesInFlight.assign(mSwapchainImages.size(), VK_NULL_HANDLE);

    return true;
}

//==============================================================================
// Destroy helpers
//==============================================================================
void VKRenderer::DestroyDeviceRelated()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    mGraphicsQueue = VK_NULL_HANDLE;
    mPresentQueue  = VK_NULL_HANDLE;
    mPhysicalDevice = VK_NULL_HANDLE;
}

void VKRenderer::DestroyInstanceRelated()
{
    if (mSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

#if !defined(NDEBUG)
    if (mDebugMsgr != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessengerEXT(mInstance, mDebugMsgr, nullptr);
        mDebugMsgr = VK_NULL_HANDLE;
    }
#endif

    if (mInstance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }
}

} // namespace toy
