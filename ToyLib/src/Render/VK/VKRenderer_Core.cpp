//======================================================================
// VKRenderer.cpp
//  - SDL3 + Vulkan
//  - “まずはウィンドウが出て、clearしてpresentできる土台” まで
//  - RenderPass / Framebuffers は Swapchain 依存なので Initialize で作る
//  - BeginFrame では Acquire -> CmdBegin -> RenderPassBegin（clear）まで
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Engine/Core/Application.h"
#include "Render/RenderBackendState.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

namespace toy {

//--------------------------------------------------------------
// Validation Layer
//--------------------------------------------------------------
static const char* kValidationLayers[] =
{
    "VK_LAYER_KHRONOS_validation"
};

static bool HasLayer(const char* name, const std::vector<VkLayerProperties>& layers)
{
    for (const auto& l : layers)
    {
        if (std::strcmp(l.layerName, name) == 0) return true;
    }
    return false;
}

static bool HasInstanceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    for (const auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

static bool HasDeviceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    for (const auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

//--------------------------------------------------------------
// Debug messenger
//--------------------------------------------------------------
static VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* cb,
    void*)
{
    (void)severity;
    std::cerr << "[VK] " << (cb && cb->pMessage ? cb->pMessage : "(null)") << "\n";
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* out)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, createInfo, nullptr, out);
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn && messenger) fn(instance, messenger, nullptr);
}

//--------------------------------------------------------------
// Queue families
//--------------------------------------------------------------
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    QueueFamilyIndices out;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            out.graphics = i;
        }

        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supported);
        if (supported)
        {
            out.present = i;
        }

        if (out.IsComplete()) break;
    }

    return out;
}

//--------------------------------------------------------------
// Swapchain support
//--------------------------------------------------------------
struct SwapchainSupport
{
    VkSurfaceCapabilitiesKHR        caps{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

static SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    SwapchainSupport out;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &out.caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, nullptr);
    out.formats.resize(fmtCount);
    if (fmtCount) vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, out.formats.data());

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, nullptr);
    out.presentModes.resize(pmCount);
    if (pmCount) vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, out.presentModes.data());

    return out;
}

static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }

    if (!formats.empty()) return formats[0];

    VkSurfaceFormatKHR fallback{};
    fallback.format = VK_FORMAT_B8G8R8A8_UNORM;
    fallback.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return fallback;
}

static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync)
{
    if (!vsync)
    {
        for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR)   return m;
        for (auto m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, int pixelW, int pixelH)
{
    if (caps.currentExtent.width != 0xFFFFFFFF)
    {
        return caps.currentExtent;
    }

    VkExtent2D e{};
    e.width  = (uint32_t)std::clamp(pixelW, (int)caps.minImageExtent.width,  (int)caps.maxImageExtent.width);
    e.height = (uint32_t)std::clamp(pixelH, (int)caps.minImageExtent.height, (int)caps.maxImageExtent.height);
    return e;
}

//--------------------------------------------------------------
// VKRenderer::Initialize
//--------------------------------------------------------------
bool VKRenderer::Initialize(const Application* app)
{
    if (!app)
    {
        std::cerr << "[VKRenderer] Initialize failed: app is null\n";
        return false;
    }

    mWindow = app->GetSDLWindow(); // non-owning
    if (!mWindow)
    {
        std::cerr << "[VKRenderer] Initialize failed: SDL window is null\n";
        return false;
    }

    // Pixel size (HiDPI)
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);
    mScreenWidth  = (float)pixelW;
    mScreenHeight = (float)pixelH;

    // DPI scale
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f) mWindowDisplayScale = 1.0f;

    //----------------------------------------------------------
    // 1) Instance
    //----------------------------------------------------------
    Uint32 sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts || sdlExtCount == 0)
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_GetInstanceExtensions failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    std::vector<const char*> instanceExts;
    instanceExts.reserve((size_t)sdlExtCount + 8);
    for (Uint32 i = 0; i < sdlExtCount; ++i)
    {
        instanceExts.push_back(sdlExts[i]);
    }

    // available instance extensions/layers
    uint32_t availExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, nullptr);
    std::vector<VkExtensionProperties> availExts(availExtCount);
    if (availExtCount)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, availExts.data());
    }

    uint32_t availLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availLayerCount, nullptr);
    std::vector<VkLayerProperties> availLayers(availLayerCount);
    if (availLayerCount)
    {
        vkEnumerateInstanceLayerProperties(&availLayerCount, availLayers.data());
    }

    // validation on/off
    mEnableValidation = mEnableValidation && HasLayer(kValidationLayers[0], availLayers);

    if (mEnableValidation)
    {
        if (HasInstanceExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availExts))
        {
            instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    // portability enumeration (MoltenVK/macOS)
    const bool hasPortabilityEnum =
        HasInstanceExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, availExts);
    if (hasPortabilityEnum)
    {
        instanceExts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "ToyLibGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "ToyLib";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = (uint32_t)instanceExts.size();
    ci.ppEnabledExtensionNames = instanceExts.data();

    if (hasPortabilityEnum)
    {
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<const char*> layers;
    VkDebugUtilsMessengerCreateInfoEXT dbgCI{};
    if (mEnableValidation)
    {
        layers.push_back(kValidationLayers[0]);
        ci.enabledLayerCount   = (uint32_t)layers.size();
        ci.ppEnabledLayerNames = layers.data();

        dbgCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCI.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCI.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCI.pfnUserCallback = VkDebugCallback;

        ci.pNext = &dbgCI;
    }

    VkResult vr = vkCreateInstance(&ci, nullptr, &mInstance);
    if (vr != VK_SUCCESS || mInstance == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateInstance failed: " << vr << "\n";
        return false;
    }

    if (mEnableValidation)
    {
        CreateDebugUtilsMessengerEXT(mInstance, &dbgCI, &mDebugMessenger);
    }

    //----------------------------------------------------------
    // 2) Surface (SDL)
    //----------------------------------------------------------
    if (!SDL_Vulkan_CreateSurface(mWindow, mInstance, nullptr, &mSurface))
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_CreateSurface failed: "
                  << SDL_GetError() << "\n";
        Shutdown();
        return false;
    }

    //----------------------------------------------------------
    // 3) Physical Device selection
    //----------------------------------------------------------
    uint32_t gpuCount = 0;
    vr = vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr);
    if (vr != VK_SUCCESS || gpuCount == 0)
    {
        std::cerr << "[VKRenderer] No Vulkan physical devices found. vr=" << vr << "\n";
        Shutdown();
        return false;
    }

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vr = vkEnumeratePhysicalDevices(mInstance, &gpuCount, gpus.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEnumeratePhysicalDevices(list) failed. vr=" << vr << "\n";
        Shutdown();
        return false;
    }

    VkPhysicalDevice best = VK_NULL_HANDLE;
    std::vector<const char*> bestDevExts;

    for (VkPhysicalDevice gpu : gpus)
    {
        // enumerate device extensions
        uint32_t deCount = 0;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, nullptr);
        std::vector<VkExtensionProperties> de(deCount);
        if (deCount)
        {
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, de.data());
        }

        // required
        if (!HasDeviceExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME, de))
        {
            continue;
        }

        std::vector<const char*> devExts;
        devExts.reserve(4);
        devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // portability subset (MoltenVK)
        if (HasDeviceExt(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, de))
        {
            devExts.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }

        // queue families
        const auto q = FindQueueFamilies(gpu, mSurface);
        if (!q.IsComplete()) continue;

        // swapchain support
        const auto sc = QuerySwapchainSupport(gpu, mSurface);
        if (sc.formats.empty() || sc.presentModes.empty()) continue;

        // choose this GPU (first suitable)
        best = gpu;
        bestDevExts = std::move(devExts);
        mQueueFamilyGraphics = q.graphics.value();
        mQueueFamilyPresent  = q.present.value();
        break;
    }

    if (best == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] No suitable GPU found.\n";
        Shutdown();
        return false;
    }

    mPhysicalDevice = best;
    mDeviceExtensions = std::move(bestDevExts);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
    std::cerr << "[VKRenderer] GPU: " << props.deviceName << "\n";

    //----------------------------------------------------------
    // 4) Logical Device + Queues
    //----------------------------------------------------------
    std::set<uint32_t> uniqueFamilies = { mQueueFamilyGraphics, mQueueFamilyPresent };
    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.reserve(uniqueFamilies.size());

    float qprio = 1.0f;
    for (uint32_t fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &qprio;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = (uint32_t)qcis.size();
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = (uint32_t)mDeviceExtensions.size();
    dci.ppEnabledExtensionNames = mDeviceExtensions.data();
    dci.pEnabledFeatures        = &features;

    // old compatibility: device layers (usually not needed)
    std::vector<const char*> deviceLayers;
    if (mEnableValidation)
    {
        deviceLayers.push_back(kValidationLayers[0]);
        dci.enabledLayerCount   = (uint32_t)deviceLayers.size();
        dci.ppEnabledLayerNames = deviceLayers.data();
    }

    vr = vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice);
    if (vr != VK_SUCCESS || mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateDevice failed: " << vr << "\n";
        Shutdown();
        return false;
    }

    vkGetDeviceQueue(mDevice, mQueueFamilyGraphics, 0, &mQueueGraphics);
    vkGetDeviceQueue(mDevice, mQueueFamilyPresent,  0, &mQueuePresent);

    //----------------------------------------------------------
    // 5) Swapchain + Images + ImageViews
    //----------------------------------------------------------
    {
        SwapchainSupport sc = QuerySwapchainSupport(mPhysicalDevice, mSurface);
        if (sc.formats.empty() || sc.presentModes.empty())
        {
            std::cerr << "[VKRenderer] Swapchain support incomplete.\n";
            Shutdown();
            return false;
        }

        mSwapchainFormat = ChooseSurfaceFormat(sc.formats);
        mPresentMode     = ChoosePresentMode(sc.presentModes, /*vsync*/ true);

        VkExtent2D extent = ChooseExtent(sc.caps, pixelW, pixelH);
        mSwapchainExtent  = extent;

        uint32_t imageCount = sc.caps.minImageCount + 1;
        if (sc.caps.maxImageCount > 0 && imageCount > sc.caps.maxImageCount)
        {
            imageCount = sc.caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR sci{};
        sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface          = mSurface;
        sci.minImageCount    = imageCount;
        sci.imageFormat      = mSwapchainFormat.format;
        sci.imageColorSpace  = mSwapchainFormat.colorSpace;
        sci.imageExtent      = extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t queueFamilyIndices[] = { mQueueFamilyGraphics, mQueueFamilyPresent };
        if (mQueueFamilyGraphics != mQueueFamilyPresent)
        {
            sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices   = queueFamilyIndices;
        }
        else
        {
            sci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            sci.queueFamilyIndexCount = 0;
            sci.pQueueFamilyIndices   = nullptr;
        }

        sci.preTransform   = sc.caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode    = mPresentMode;
        sci.clipped        = VK_TRUE;
        sci.oldSwapchain   = VK_NULL_HANDLE;

        vr = vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain);
        if (vr != VK_SUCCESS || mSwapchain == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateSwapchainKHR failed: " << vr << "\n";
            Shutdown();
            return false;
        }

        uint32_t scImgCount = 0;
        vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, nullptr);
        if (vr != VK_SUCCESS || scImgCount == 0)
        {
            std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(count) failed: " << vr << "\n";
            Shutdown();
            return false;
        }

        mSwapchainImages.resize(scImgCount, VK_NULL_HANDLE);
        vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, mSwapchainImages.data());
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(list) failed: " << vr << "\n";
            Shutdown();
            return false;
        }

        mSwapchainImageViews.resize(scImgCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < scImgCount; ++i)
        {
            VkImageViewCreateInfo iv{};
            iv.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            iv.image    = mSwapchainImages[i];
            iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv.format   = mSwapchainFormat.format;

            iv.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            iv.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            iv.subresourceRange.baseMipLevel   = 0;
            iv.subresourceRange.levelCount     = 1;
            iv.subresourceRange.baseArrayLayer = 0;
            iv.subresourceRange.layerCount     = 1;

            vr = vkCreateImageView(mDevice, &iv, nullptr, &mSwapchainImageViews[i]);
            if (vr != VK_SUCCESS || mSwapchainImageViews[i] == VK_NULL_HANDLE)
            {
                std::cerr << "[VKRenderer] vkCreateImageView failed: " << vr << "\n";
                Shutdown();
                return false;
            }
        }

        std::cerr << "[VKRenderer] Swapchain created: "
                  << scImgCount << " images, extent="
                  << mSwapchainExtent.width << "x" << mSwapchainExtent.height
                  << "\n";
    }

    //----------------------------------------------------------
    // 6) CommandPool + CommandBuffers + Sync
    //----------------------------------------------------------
    {
        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.queueFamilyIndex = mQueueFamilyGraphics;
        pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vr = vkCreateCommandPool(mDevice, &pci, nullptr, &mCommandPool);
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateCommandPool failed: " << vr << "\n";
            Shutdown();
            return false;
        }

        const uint32_t kFrames = 2;
        mFrames.resize(kFrames);

        // command buffers: 1 per frame
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
            Shutdown();
            return false;
        }

        for (uint32_t i = 0; i < kFrames; ++i)
        {
            mFrames[i].cmd = cmds[i];
        }

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
                Shutdown();
                return false;
            }
        }
    }

    //----------------------------------------------------------
    // 7) RenderPass + Framebuffers (Swapchain dependent)
    //----------------------------------------------------------
    if (!CreateRenderPass())
    {
        Shutdown();
        return false;
    }

    if (!CreateFramebuffers())
    {
        Shutdown();
        return false;
    }
    
    
    // Sprite Pipeline
    if (!CreateSpritePipeline() )
    {
        Shutdown();
        return false;
    };
    
    RenderBackendState::Get().SetVKPhysicalDevice(mPhysicalDevice);
    RenderBackendState::Get().SetVKDevice(mDevice);
    RenderBackendState::Get().SetVKGraphicsQueue(mQueueGraphics);
    RenderBackendState::Get().SetVKCommandPool(mCommandPool);
    
    
    std::cerr << "[Renderer] VK Init Complete. "
              << "Pixels(" << pixelW << "x" << pixelH << ") "
              << "Scale="  << mWindowDisplayScale
              << " SwapchainImages=" << (int)mSwapchainImages.size()
              << std::endl;

    return true;
}

//--------------------------------------------------------------
// VKRenderer::Shutdown
//--------------------------------------------------------------
void VKRenderer::Shutdown()
{
    if (mDevice)
    {
        vkDeviceWaitIdle(mDevice);
    }

    // framebuffers
    if (mDevice)
    {
        for (auto fb : mFramebuffers)
        {
            if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
        }
    }
    mFramebuffers.clear();

    // UIResources
    DestroyUIResources();
    
    // render pass
    if (mDevice && mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    // sync objects
    for (auto& f : mFrames)
    {
        if (mDevice)
        {
            if (f.imageAvailable) vkDestroySemaphore(mDevice, f.imageAvailable, nullptr);
            if (f.renderFinished) vkDestroySemaphore(mDevice, f.renderFinished, nullptr);
            if (f.inFlight)       vkDestroyFence(mDevice, f.inFlight, nullptr);
        }
        f.imageAvailable = VK_NULL_HANDLE;
        f.renderFinished = VK_NULL_HANDLE;
        f.inFlight       = VK_NULL_HANDLE;
        f.cmd            = VK_NULL_HANDLE;
    }
    mFrames.clear();

    // command pool (also frees command buffers)
    if (mDevice && mCommandPool)
    {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }

    // swapchain image views
    if (mDevice)
    {
        for (auto v : mSwapchainImageViews)
        {
            if (v) vkDestroyImageView(mDevice, v, nullptr);
        }
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    // swapchain
    if (mDevice && mSwapchain)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    // device
    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    // surface
    if (mSurface && mInstance)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    // debug messenger
    if (mEnableValidation && mDebugMessenger && mInstance)
    {
        DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger);
        mDebugMessenger = VK_NULL_HANDLE;
    }

    // instance
    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    // reset misc
    mPhysicalDevice = VK_NULL_HANDLE;
    mQueueGraphics  = VK_NULL_HANDLE;
    mQueuePresent   = VK_NULL_HANDLE;
    mQueueFamilyGraphics = UINT32_MAX;
    mQueueFamilyPresent  = UINT32_MAX;
    mDeviceExtensions.clear();

    mSwapchainFormat = VkSurfaceFormatKHR{};
    mPresentMode     = VK_PRESENT_MODE_FIFO_KHR;
    mSwapchainExtent = VkExtent2D{};

    mImageIndex = 0;
    mFrameIndex = 0;
}

//--------------------------------------------------------------
// VKRenderer::BeginFrame
//--------------------------------------------------------------
bool VKRenderer::BeginFrame()
{
    if (!mDevice || !mSwapchain || mFrames.empty())
        return false;

    // 前提チェック（acquire前にやるのが安全）
    if (mRenderPass == VK_NULL_HANDLE ||
        mFramebuffers.empty())
        return false;

    FrameSync& frame = mFrames[mFrameIndex];

    vkWaitForFences(mDevice, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &frame.inFlight);

    VkResult ar = vkAcquireNextImageKHR(
        mDevice,
        mSwapchain,
        UINT64_MAX,
        frame.imageAvailable,
        VK_NULL_HANDLE,
        &mImageIndex);

    if (ar == VK_ERROR_OUT_OF_DATE_KHR)
    {
        mNeedRecreateSwapchain = true;
        return false;
    }

    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
    {
        std::cerr << "Acquire failed: " << ar << "\n";
        return false;
    }

    if (mImageIndex >= mFramebuffers.size())
        return false;

    vkResetCommandBuffer(frame.cmd, 0);

    VkCommandBufferBeginInfo cbBegin{};
    cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frame.cmd, &cbBegin);

    VkClearValue clear{};
    clear.color.float32[0] = mClearColor.x;
    clear.color.float32[1] = mClearColor.y;
    clear.color.float32[2] = mClearColor.z;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass  = mRenderPass;
    rpBegin.framebuffer = mFramebuffers[mImageIndex];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = mSwapchainExtent;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clear;

    vkCmdBeginRenderPass(frame.cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

//--------------------------------------------------------------
// VKRenderer::EndFrame
//--------------------------------------------------------------
void VKRenderer::EndFrame()
{
    if (!mDevice || !mSwapchain || mFrames.empty())
    {
        return;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    // BeginFrame() が true のときだけ呼ばれる想定なので、
    // ここでは「コマンドは録れている」前提で進める

    vkCmdEndRenderPass(frame.cmd);

    VkResult er = vkEndCommandBuffer(frame.cmd);
    if (er != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEndCommandBuffer failed: " << er << "\n";
        return;
    }

    // submit: wait imageAvailable -> signal renderFinished
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &frame.imageAvailable;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &frame.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &frame.renderFinished;

    VkResult sr = vkQueueSubmit(mQueueGraphics, 1, &submit, frame.inFlight);
    if (sr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueueSubmit failed: " << sr << "\n";
        return;
    }

    // present: wait renderFinished
    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &frame.renderFinished;
    present.swapchainCount     = 1;
    present.pSwapchains        = &mSwapchain;
    present.pImageIndices      = &mImageIndex;

    VkResult pr = vkQueuePresentKHR(mQueuePresent, &present);

    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        // ここでは作り直さない（Drawの外側でやる方が安全）
        mNeedRecreateSwapchain = true;
    }
    else if (pr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueuePresentKHR failed: " << pr << "\n";
    }

    // 次フレームへ
    mFrameIndex = (mFrameIndex + 1) % static_cast<uint32_t>(mFrames.size());
}

//--------------------------------------------------------------
// CreateRenderPass (Swapchain dependent)
//--------------------------------------------------------------
bool VKRenderer::CreateRenderPass()
{
    // guard: already created
    if (mRenderPass != VK_NULL_HANDLE)
    {
        return true;
    }

    if (!mDevice)
    {
        std::cerr << "[VKRenderer] CreateRenderPass: mDevice is null\n";
        return false;
    }

    VkAttachmentDescription color{};
    color.format         = mSwapchainFormat.format;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &color;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateRenderPass failed: " << vr << "\n";
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// CreateFramebuffers (Swapchain dependent)
//--------------------------------------------------------------
bool VKRenderer::CreateFramebuffers()
{
    if (!mDevice)
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: mDevice is null\n";
        return false;
    }
    if (mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: mRenderPass is null\n";
        return false;
    }
    if (mSwapchainImageViews.empty())
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: no swapchain image views\n";
        return false;
    }

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    mFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { mSwapchainImageViews[i] };

        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = mRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments    = attachments;
        fci.width           = mSwapchainExtent.width;
        fci.height          = mSwapchainExtent.height;
        fci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffers[i]);
        if (vr != VK_SUCCESS || mFramebuffers[i] == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateFramebuffer failed: " << vr
                      << " (i=" << i << ")\n";
            return false;
        }
    }

    return true;
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    PipelineHandle h{};
    h.backend = PipelineBackend::VK;

    auto it = mPipelines.find(name);
    if (it == mPipelines.end())
    {
        return {}; // invalid
    }

    h.ptrVKPipeline = it->second.get();
    return h;
}

} // namespace toy
