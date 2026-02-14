// Render/VK/VKUtil.cpp
#include "Render/VK/VKUtil.h"

#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace toy::vkutil
{

//--------------------------------------------------------------
// ReadFileBinary
//--------------------------------------------------------------
bool ReadFileBinary(const std::string& path, std::vector<uint8_t>& out)
{
    out.clear();

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        std::cerr << "[VKUtil] Failed to open file: " << path << "\n";
        return false;
    }

    const std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        std::cerr << "[VKUtil] File empty: " << path << "\n";
        return false;
    }

    out.resize((size_t)size);
    ifs.seekg(0, std::ios::beg);

    if (!ifs.read(reinterpret_cast<char*>(out.data()), size))
    {
        std::cerr << "[VKUtil] Failed to read file: " << path << "\n";
        out.clear();
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// CreateShaderModule
//--------------------------------------------------------------
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code)
{
    if (!device || code.empty() || (code.size() % 4) != 0)
    {
        std::cerr << "[VKUtil] CreateShaderModule: invalid code (empty or not multiple of 4)\n";
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    const VkResult r = vkCreateShaderModule(device, &ci, nullptr, &mod);
    if (r != VK_SUCCESS || mod == VK_NULL_HANDLE)
    {
        std::cerr << "[VKUtil] vkCreateShaderModule failed: " << r << "\n";
        return VK_NULL_HANDLE;
    }
    return mod;
}

//--------------------------------------------------------------
// Enumerations: instance layers/exts, device exts
//--------------------------------------------------------------
std::vector<VkLayerProperties> GetInstanceLayers()
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    std::vector<VkLayerProperties> out(count);
    if (count)
    {
        vkEnumerateInstanceLayerProperties(&count, out.data());
    }
    return out;
}

std::vector<VkExtensionProperties> GetInstanceExts()
{
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> out(count);
    if (count)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &count, out.data());
    }
    return out;
}

std::vector<VkExtensionProperties> GetDeviceExts(VkPhysicalDevice gpu)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> out(count);
    if (count)
    {
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, out.data());
    }
    return out;
}

//--------------------------------------------------------------
// HasX
//--------------------------------------------------------------
bool HasLayer(const char* name, const std::vector<VkLayerProperties>& layers)
{
    if (!name) return false;
    for (const auto& l : layers)
    {
        if (std::strcmp(l.layerName, name) == 0) return true;
    }
    return false;
}

bool HasInstanceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    if (!name) return false;
    for (const auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

bool HasDeviceExt(const char* name, const std::vector<VkExtensionProperties>& exts)
{
    if (!name) return false;
    for (const auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

//--------------------------------------------------------------
// FindQueueFamilies
//--------------------------------------------------------------
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    QueueFamilyIndices out;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);

    std::vector<VkQueueFamilyProperties> props(count);
    if (count)
    {
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        if (props[i].queueCount > 0 && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
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
SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    SwapchainSupport out;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &out.caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, nullptr);
    out.formats.resize(fmtCount);
    if (fmtCount)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, out.formats.data());
    }

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, nullptr);
    out.presentModes.resize(pmCount);
    if (pmCount)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, out.presentModes.data());
    }

    return out;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
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
    fallback.format     = VK_FORMAT_B8G8R8A8_UNORM;
    fallback.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return fallback;
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync)
{
    if (!vsync)
    {
        for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR)   return m;
        for (auto m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, int pixelW, int pixelH)
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
// FindMemoryType
//--------------------------------------------------------------
uint32_t FindMemoryType(VkPhysicalDevice phys,
                        uint32_t typeBits,
                        VkMemoryPropertyFlags required)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        const bool typeOK = (typeBits & (1u << i)) != 0;
        const bool propOK = (memProps.memoryTypes[i].propertyFlags & required) == required;
        if (typeOK && propOK)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

//--------------------------------------------------------------
// CreateBuffer_HostVisible
//--------------------------------------------------------------
bool CreateBuffer_HostVisible(VkPhysicalDevice phys,
                              VkDevice device,
                              VkDeviceSize sizeBytes,
                              VkBufferUsageFlags usage,
                              VkBuffer& outBuf,
                              VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!phys || !device || sizeBytes == 0)
    {
        return false;
    }

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = sizeBytes;
    bci.usage       = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, outBuf, &req);

    const uint32_t memType =
        FindMemoryType(phys, req.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX)
    {
        vkDestroyBuffer(device, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(device, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(device, outBuf, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(device, outMem, nullptr);
        vkDestroyBuffer(device, outBuf, nullptr);
        outMem = VK_NULL_HANDLE;
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// CreateImage2D
//--------------------------------------------------------------
bool CreateImage2D(VkPhysicalDevice phys,
                   VkDevice device,
                   uint32_t w,
                   uint32_t h,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VkMemoryPropertyFlags memProps,
                   VkImage& outImg,
                   VkDeviceMemory& outMem,
                   VkImageLayout initialLayout)
{
    outImg = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!phys || !device || w == 0 || h == 0)
    {
        return false;
    }

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = format;
    ici.extent        = { w, h, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = tiling;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = initialLayout;

    if (vkCreateImage(device, &ici, nullptr, &outImg) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, outImg, &req);

    const uint32_t memType = FindMemoryType(phys, req.memoryTypeBits, memProps);
    if (memType == UINT32_MAX)
    {
        vkDestroyImage(device, outImg, nullptr);
        outImg = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyImage(device, outImg, nullptr);
        outImg = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindImageMemory(device, outImg, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(device, outMem, nullptr);
        vkDestroyImage(device, outImg, nullptr);
        outMem = VK_NULL_HANDLE;
        outImg = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// CreateImageView2D
//--------------------------------------------------------------
VkImageView CreateImageView2D(VkDevice device,
                              VkImage img,
                              VkFormat format,
                              VkImageAspectFlags aspect)
{
    if (!device || !img)
    {
        return VK_NULL_HANDLE;
    }

    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = format;

    vci.subresourceRange.aspectMask     = aspect;
    vci.subresourceRange.baseMipLevel   = 0;
    vci.subresourceRange.levelCount     = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &vci, nullptr, &view) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return view;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
// CmdTransitionImageLayout
//--------------------------------------------------------------
void CmdTransitionImageLayout(VkCommandBuffer cmd,
                              VkImage img,
                              VkImageAspectFlags aspect,
                              VkImageLayout oldLayout,
                              VkImageLayout newLayout,
                              VkPipelineStageFlags srcStage,
                              VkPipelineStageFlags dstStage,
                              VkAccessFlags srcAccess,
                              VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier bar{};
    bar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.oldLayout           = oldLayout;
    bar.newLayout           = newLayout;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image               = img;

    bar.subresourceRange.aspectMask     = aspect;
    bar.subresourceRange.baseMipLevel   = 0;
    bar.subresourceRange.levelCount     = 1;
    bar.subresourceRange.baseArrayLayer = 0;
    bar.subresourceRange.layerCount     = 1;

    bar.srcAccessMask = srcAccess;
    bar.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &bar);
}

// ----------------------------------------------------------
// Debug messenger
// ----------------------------------------------------------
static const char* SeverityToStr(VkDebugUtilsMessageSeverityFlagBitsEXT s)
{
    if (s & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   return "ERROR";
    if (s & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) return "WARN";
    if (s & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    return "INFO";
    return "VERBOSE";
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* cb,
    void*)
{
    const char* t =
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) ? "VALIDATION" :
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) ? "PERF" :
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) ? "GENERAL" : "UNKNOWN";

    std::cerr << "[VK][" << t << "][" << SeverityToStr(severity) << "] "
              << (cb && cb->pMessage ? cb->pMessage : "(null)") << "\n";
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* out)
{
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, createInfo, nullptr, out);
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger)
{
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn && messenger) fn(instance, messenger, nullptr);
}

bool CreateBuffer_DeviceLocal(VkPhysicalDevice phys,
                              VkDevice device,
                              VkDeviceSize sizeBytes,
                              VkBufferUsageFlags usage,
                              VkBuffer& outBuf,
                              VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = sizeBytes;
    bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT; // staging copy想定
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, outBuf, &req);

    const uint32_t typeIndex = FindMemoryType(
        phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (typeIndex == UINT32_MAX)
    {
        vkDestroyBuffer(device, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(device, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(device, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(device, outBuf, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(device, outMem, nullptr);
        vkDestroyBuffer(device, outBuf, nullptr);
        outMem = VK_NULL_HANDLE;
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void CmdCopyBuffer(VkCommandBuffer cmd,
                   VkBuffer src,
                   VkBuffer dst,
                   VkDeviceSize sizeBytes)
{
    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size      = sizeBytes;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
}

bool UploadBuffer_Staging(VkPhysicalDevice phys,
                          VkDevice device,
                          VkCommandBuffer cmd,
                          const void* srcData,
                          VkDeviceSize sizeBytes,
                          VkBuffer dstDeviceLocal,
                          VkDeviceSize dstOffsetBytes)
{
    if (!srcData || sizeBytes == 0 || !dstDeviceLocal) return false;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (!CreateBuffer_HostVisible(
            phys, device, sizeBytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            staging, stagingMem))
    {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device, stagingMem, 0, sizeBytes, 0, &mapped) != VK_SUCCESS)
    {
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }

    std::memcpy(mapped, srcData, (size_t)sizeBytes);
    vkUnmapMemory(device, stagingMem);

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = dstOffsetBytes;
    copy.size      = sizeBytes;
    vkCmdCopyBuffer(cmd, staging, dstDeviceLocal, 1, &copy);

    // staging は「GPUが使い終わった後」に破棄する必要があるが、
    // ここでは「その cmd を submit して wait する」運用を前提に簡易化。
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
    return true;
}

VkDescriptorSetLayout CreateSetLayout_CombinedImageSampler(VkDevice device,
                                                           uint32_t binding,
                                                           VkShaderStageFlags stages)
{
    VkDescriptorSetLayoutBinding b{};
    b.binding            = binding;
    b.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount    = 1;
    b.stageFlags         = stages;
    b.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings    = &b;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return layout;
}

void UpdateDescriptorSet_CombinedImageSampler(VkDevice device,
                                              VkDescriptorSet set,
                                              uint32_t binding,
                                              VkImageView view,
                                              VkSampler sampler,
                                              VkImageLayout layout)
{
    VkDescriptorImageInfo img{};
    img.imageView   = view;
    img.sampler     = sampler;
    img.imageLayout = layout;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = set;
    w.dstBinding      = binding;
    w.dstArrayElement = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &img;

    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
}

VkDescriptorSetLayout CreateDescriptorSetLayout(
    VkDevice device,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = (uint32_t)bindings.size();
    ci.pBindings    = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return layout;
}
} // namespace toy::vkutil
