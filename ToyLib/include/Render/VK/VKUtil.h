// Render/VK/VKUtil.h
#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace toy::vkutil
{
    // ----------------------------------------------------------
    // File
    // ----------------------------------------------------------
    bool ReadFileBinary(const std::string& path, std::vector<uint8_t>& out);

    // ----------------------------------------------------------
    // Shader
    // ----------------------------------------------------------
    VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code);

    // ----------------------------------------------------------
    // Instance / Device capability helpers
    // ----------------------------------------------------------
    std::vector<VkLayerProperties>     GetInstanceLayers();
    std::vector<VkExtensionProperties> GetInstanceExts();
    std::vector<VkExtensionProperties> GetDeviceExts(VkPhysicalDevice gpu);

    bool HasLayer(const char* name, const std::vector<VkLayerProperties>& layers);
    bool HasInstanceExt(const char* name, const std::vector<VkExtensionProperties>& exts);
    bool HasDeviceExt(const char* name, const std::vector<VkExtensionProperties>& exts);

    // ----------------------------------------------------------
    // Queue families
    // ----------------------------------------------------------
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool IsComplete() const
        {
            return graphics.has_value() && present.has_value();
        }
    };

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface);

    // ----------------------------------------------------------
    // Swapchain support
    // ----------------------------------------------------------
    struct SwapchainSupport
    {
        VkSurfaceCapabilitiesKHR        caps{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    SwapchainSupport   QuerySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync);
    VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, int pixelW, int pixelH);

    // ----------------------------------------------------------
    // Memory
    // ----------------------------------------------------------
    uint32_t FindMemoryType(VkPhysicalDevice phys,
                            uint32_t typeBits,
                            VkMemoryPropertyFlags required);

    // ----------------------------------------------------------
    // Buffer (最短：HostVisible)
    // ----------------------------------------------------------
    bool CreateBuffer_HostVisible(VkPhysicalDevice phys,
                                  VkDevice device,
                                  VkDeviceSize sizeBytes,
                                  VkBufferUsageFlags usage,
                                  VkBuffer& outBuf,
                                  VkDeviceMemory& outMem);

    // ----------------------------------------------------------
    // Image
    // ----------------------------------------------------------
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
                       VkImageLayout initialLayout);

    VkImageView CreateImageView2D(VkDevice device,
                                  VkImage img,
                                  VkFormat format,
                                  VkImageAspectFlags aspect);

    // ----------------------------------------------------------
    // Barrier
    // ----------------------------------------------------------
    void CmdTransitionImageLayout(VkCommandBuffer cmd,
                                  VkImage img,
                                  VkImageAspectFlags aspect,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage,
                                  VkAccessFlags srcAccess,
                                  VkAccessFlags dstAccess);

    // ----------------------------------------------------------
    // Debug messenger
    // ----------------------------------------------------------
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* cb,
        void* userData);

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
        VkDebugUtilsMessengerEXT* out);

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger);

    // ----------------------------------------------------------
    // Buffer: device-local (staging expected)
    // ----------------------------------------------------------
    bool CreateBuffer_DeviceLocal(VkPhysicalDevice phys,
                                  VkDevice device,
                                  VkDeviceSize sizeBytes,
                                  VkBufferUsageFlags usage,
                                  VkBuffer& outBuf,
                                  VkDeviceMemory& outMem);

    void CmdCopyBuffer(VkCommandBuffer cmd,
                       VkBuffer src,
                       VkBuffer dst,
                       VkDeviceSize sizeBytes);

    bool UploadBuffer_Staging(VkPhysicalDevice phys,
                              VkDevice device,
                              VkCommandBuffer cmd,
                              const void* srcData,
                              VkDeviceSize sizeBytes,
                              VkBuffer dstDeviceLocal,
                              VkDeviceSize dstOffsetBytes = 0);

    // ----------------------------------------------------------
    // Descriptor (legacy helpers you already use)
    // ----------------------------------------------------------
    VkDescriptorSetLayout CreateSetLayout_CombinedImageSampler(VkDevice device,
                                                               uint32_t binding,
                                                               VkShaderStageFlags stages);

    void UpdateDescriptorSet_CombinedImageSampler(VkDevice device,
                                                  VkDescriptorSet set,
                                                  uint32_t binding,
                                                  VkImageView view,
                                                  VkSampler sampler,
                                                  VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // ----------------------------------------------------------
    // Descriptor helpers (Layouts / Bindings)  ※統一版
    // ----------------------------------------------------------
    VkDescriptorSetLayoutBinding MakeBinding_UBO(
        uint32_t binding,
        VkShaderStageFlags stages,
        uint32_t count = 1);

    VkDescriptorSetLayoutBinding MakeBinding_CombinedImageSampler(
        uint32_t binding,
        VkShaderStageFlags stages,
        uint32_t count = 1);

    VkDescriptorSetLayout CreateDescriptorSetLayout(
        VkDevice device,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    // Optional: descriptor set write helpers
    void WriteDesc_UBO(
        VkDevice device,
        VkDescriptorSet set,
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize range,
        VkDeviceSize offset = 0);

    void WriteDesc_CombinedImageSampler(
        VkDevice device,
        VkDescriptorSet set,
        uint32_t binding,
        VkImageView view,
        VkSampler sampler,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Depth format selection
    VkFormat ChooseDepthFormat(VkPhysicalDevice phys);
    bool HasStencilComponent(VkFormat format);

} // namespace toy::vkutil
