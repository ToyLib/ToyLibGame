// VKRenderer::Initialize()
//  - GLRenderer::Initialize() と同じ責務感で「起動時に必要なVK基盤を全部作る」版。
//  - SDL3 + Vulkan を前提（SDL_Vulkan_* を使う）
//  - MoltenVK(mac) も意識して portability 対応を入れてある（Windowsでも害はない）
//
// NOTE:
//  - ここでは「RenderPass/PSO/Descriptor 等」はまだ作らない。
//  - Swapchain / CommandPool / Sync までを “GLでいうInitialize完了” 相当としている。

#include "Render/VK/VKRenderer.h"
#include "Engine/Core/Application.h"

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
// 使うValidation Layer（あれば有効）
//--------------------------------------------------------------
static const char* kValidationLayers[] =
{
    "VK_LAYER_KHRONOS_validation"
};

static bool HasLayer(const char* name, const std::vector<VkLayerProperties>& layers)
{
    for (auto& l : layers)
    {
        if (std::strcmp(l.layerName, name) == 0) return true;
    }
    return false;
}

static bool HasInstanceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    for (auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

static bool HasDeviceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    for (auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

//--------------------------------------------------------------
// Debug utils messenger（任意）
//--------------------------------------------------------------
static VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* cb,
    void*)
{
    // 必要なら severity でフィルタしてもOK
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
// Queue family 探索
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
        // graphics
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            out.graphics = i;
        }

        // present
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
    // SRGB で欲しいならここを優先
    for (auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    return formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
                           : formats[0];
}

static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync)
{
    // VSync ON: FIFO（必須）を基本に
    // VSync OFF: MAILBOX/IMMEDIATE があればそれに
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

    mWindow = app->GetSDLWindow(); // 非所有
    if (!mWindow)
    {
        std::cerr << "[VKRenderer] Initialize failed: SDL window is null\n";
        return false;
    }

    // 実ピクセルサイズ（HiDPI）
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
    // SDL3 が要求する Instance 拡張を取得（SDL2 形式ではなく SDL3 形式）
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

    // 利用可能な instance ext/layer を列挙
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

    // validation 有効にするか（設定ファイル等で切替想定）
    mEnableValidation = mEnableValidation && HasLayer(kValidationLayers[0], availLayers);

    if (mEnableValidation)
    {
        // debug utils
        if (HasInstanceExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availExts))
        {
            instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    // MoltenVK / portability 対応（他OSでも入ってたら使う）
    bool hasPortabilityEnum = HasInstanceExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, availExts);
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
    appInfo.apiVersion         = VK_API_VERSION_1_2; // まずは 1.2 で

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = (uint32_t)instanceExts.size();
    ci.ppEnabledExtensionNames = instanceExts.data();

    // portability のときはフラグ必須（MoltenVKでよく必要）
    if (hasPortabilityEnum)
    {
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<const char*> layers;
    if (mEnableValidation)
    {
        layers.push_back(kValidationLayers[0]);
        ci.enabledLayerCount   = (uint32_t)layers.size();
        ci.ppEnabledLayerNames = layers.data();
    }

    // Debug messenger create info を pNext にぶら下げる（instance生成時から拾える）
    VkDebugUtilsMessengerCreateInfoEXT dbgCI{};
    if (mEnableValidation)
    {
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
    if (vr != VK_SUCCESS || !mInstance)
    {
        std::cerr << "[VKRenderer] vkCreateInstance failed: " << vr << "\n";
        return false;
    }

    if (mEnableValidation)
    {
        // debug messenger（任意）
        CreateDebugUtilsMessengerEXT(mInstance, &dbgCI, &mDebugMessenger);
    }
    //----------------------------------------------------------
    // 2) Surface (SDL)
    //----------------------------------------------------------
    if (!SDL_Vulkan_CreateSurface(mWindow, mInstance, nullptr, &mSurface))
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_CreateSurface failed: "
                  << SDL_GetError() << "\n";
        Shutdown(); // 後述の後始末（自分で用意してる想定）
        return false;
    }

    //----------------------------------------------------------
    // 3) Physical Device 選択
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

    // 必須 device extensions（最低 swapchain）
    const char* kRequiredDevExtsBase[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDevice best = VK_NULL_HANDLE;

    // 選定結果（best 決定時に確定）
    std::vector<const char*> bestDevExts;

    for (VkPhysicalDevice gpu : gpus)
    {
        // device ext 列挙
        uint32_t deCount = 0;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, nullptr);
        std::vector<VkExtensionProperties> de(deCount);
        if (deCount)
        {
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, de.data());
        }

        // 候補GPU向けに「有効化する device ext セット」を組み立て
        std::vector<const char*> devExts;
        devExts.reserve(4);

        // (A) swapchain は必須
        {
            bool ok = true;
            for (const char* req : kRequiredDevExtsBase)
            {
                if (!HasDeviceExt(req, de))
                {
                    ok = false;
                    break;
                }
                devExts.push_back(req);
            }
            if (!ok) continue;
        }

        // (B) MoltenVK 対応：portability subset が “必須” になる環境がある
        // あるなら有効化（無い環境でも問題なし）
        const bool hasPortabilitySubset =
            HasDeviceExt(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, de);
        if (hasPortabilitySubset)
        {
            devExts.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }

        // Queue family（Graphics + Present が揃ってるか）
        const auto q = FindQueueFamilies(gpu, mSurface);
        if (!q.IsComplete()) continue;

        // Swapchain support（format / present mode が取れるか）
        const auto sc = QuerySwapchainSupport(gpu, mSurface);
        if (sc.formats.empty() || sc.presentModes.empty()) continue;

        // ---- ここまで来たら “動くGPU” 候補 ----
        best = gpu;
        bestDevExts = std::move(devExts);
        mQueueFamilyGraphics = q.graphics.value();
        mQueueFamilyPresent  = q.present.value();
        break; // まずは「動く」優先で最初の適合GPUを採用
    }

    if (best == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] No suitable GPU found.\n";
        Shutdown();
        return false;
    }

    mPhysicalDevice = best;
    mDeviceExtensions = std::move(bestDevExts); // ← Device作成で使うならメンバに保存

    // デバイスプロパティ表示（ログ）
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

    // device ext（swapchain + portability subsetがあれば追加）
    // device ext (swapchain + portability subsetがあれば追加)
    {
        uint32_t deCount = 0;
        vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deCount, nullptr);

        std::vector<VkExtensionProperties> de(deCount);
        if (deCount)
        {
            vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deCount, de.data());
        }

        // まず必須
        mDeviceExtensions.clear();
        mDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // MoltenVK 対応: portability subset があれば追加
        // ※ マクロが未定義の環境もあるので「文字列で書く」のが安全
        if (HasDeviceExt("VK_KHR_portability_subset", de))
        {
            mDeviceExtensions.push_back("VK_KHR_portability_subset");
        }
    }

    VkPhysicalDeviceFeatures features{};
    /*
        最初は空でOK（必要になったら features.samplerAnisotropy = VK_TRUE; 等）
    */

    VkDeviceCreateInfo dci{};
    dci.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos    = qcis.data();

    // ★ devExts → mDeviceExtensions
    dci.enabledExtensionCount   = static_cast<uint32_t>(mDeviceExtensions.size());
    dci.ppEnabledExtensionNames = mDeviceExtensions.data();

    dci.pEnabledFeatures = &features;

    //--------------------------------------------------------------
    // 古い互換: device layer（今は不要なことが多い）
    // ※ Vulkan 1.1+ だと instance layer で十分なケースが多い
    //--------------------------------------------------------------
    std::vector<const char*> deviceLayers;
    if (mEnableValidation)
    {
        // あなたの既存設計に合わせて：kValidationLayers[0] を使う想定
        deviceLayers.push_back(kValidationLayers[0]);

        dci.enabledLayerCount   = static_cast<uint32_t>(deviceLayers.size());
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
    // 5) Swapchain 作成（ここまでで “表示できる土台”）
    //----------------------------------------------------------
    {
        // サポート情報を取得
        SwapchainSupport sc = QuerySwapchainSupport(mPhysicalDevice, mSurface);

        if (sc.formats.empty() || sc.presentModes.empty())
        {
            std::cerr << "[VKRenderer] Swapchain support incomplete.\n";
            Shutdown();
            return false;
        }

        // Surface format / present mode を選ぶ
        mSwapchainFormat = ChooseSurfaceFormat(sc.formats);
        mPresentMode     = ChoosePresentMode(sc.presentModes, /*vsync*/ true);

        // SDLで取得した pixelW/pixelH を基に extent 決定
        VkExtent2D extent = ChooseExtent(sc.caps, pixelW, pixelH);
        mSwapchainExtent  = extent;

        // 画像枚数（基本: min+1、上限があれば clamp）
        uint32_t imageCount = sc.caps.minImageCount + 1;
        if (sc.caps.maxImageCount > 0 && imageCount > sc.caps.maxImageCount)
        {
            imageCount = sc.caps.maxImageCount;
        }

        // Swapchain create
        VkSwapchainCreateInfoKHR sci{};
        sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface          = mSurface;
        sci.minImageCount    = imageCount;
        sci.imageFormat      = mSwapchainFormat.format;
        sci.imageColorSpace  = mSwapchainFormat.colorSpace;
        sci.imageExtent      = extent;
        sci.imageArrayLayers = 1;

        // 最低限: render target として使う（＋将来のコピー/ポスト用に transfer dst も付ける）
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // Queue family が分かれてる場合は CONCURRENT
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

        // 変換（window system の currentTransform をそのまま採用）
        sci.preTransform = sc.caps.currentTransform;

        // アルファ合成（一般に opaque でOK）
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        sci.presentMode  = mPresentMode;
        sci.clipped      = VK_TRUE;
        sci.oldSwapchain = VK_NULL_HANDLE;

        VkResult vr = vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain);
        if (vr != VK_SUCCESS || mSwapchain == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateSwapchainKHR failed: " << vr << "\n";
            Shutdown();
            return false;
        }

        // swapchain images
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

        // image views
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
    // 6) CommandPool / Sync（最低限）
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

        // フレーム同期（とりあえず2フレーム）
        const uint32_t kFrames = 2;
        mFrames.resize(kFrames);

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
    // 7) ここから先は「あなたのGL Initializeと同じ」ノリで TODO
    //----------------------------------------------------------
    // - Shaders (SPIR-V 読み込み/ShaderModule)
    // - RenderPass / Framebuffers
    // - Pipeline / Descriptor / Depth
    // - Common geometry（VK vertex buffer 等）
    // - Shadow mapping（VK版）
    // - OnWindowResized(pixelW,pixelH) 相当（swapchain recreate）

    std::cerr << "[Renderer] VK Init Complete. "
              << "Pixels(" << pixelW << "x" << pixelH << ") "
              << "Scale="  << mWindowDisplayScale
              << " SwapchainImages=" << (int)mSwapchainImages.size()
              << std::endl;

    return true;
}

//--------------------------------------------------------------
// 片付け（Initialize失敗時にも呼べるように）
//  ※あなたのプロジェクト側の実装に合わせて調整してOK
//--------------------------------------------------------------
void VKRenderer::Shutdown()
{
    if (mDevice)
    {
        vkDeviceWaitIdle(mDevice);
    }

    // sync
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
    }
    mFrames.clear();

    if (mDevice && mCommandPool)
    {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }

    // swapchain views
    if (mDevice)
    {
        for (auto v : mSwapchainImageViews)
        {
            if (v) vkDestroyImageView(mDevice, v, nullptr);
        }
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    if (mDevice && mSwapchain)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mSurface && mInstance)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    if (mEnableValidation && mDebugMessenger && mInstance)
    {
        DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger);
        mDebugMessenger = VK_NULL_HANDLE;
    }

    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    mPhysicalDevice = VK_NULL_HANDLE;
    mQueueGraphics  = VK_NULL_HANDLE;
    mQueuePresent   = VK_NULL_HANDLE;

    mQueueFamilyGraphics = 0;
    mQueueFamilyPresent  = 0;
}

} // namespace toy
