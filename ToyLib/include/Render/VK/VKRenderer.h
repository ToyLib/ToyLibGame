#pragma once

#include "Render/IRenderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace toy
{

//==============================================================
// FrameSync（最低限のフレーム同期）
//==============================================================
struct FrameSync
{
    VkSemaphore imageAvailable { VK_NULL_HANDLE };
    VkSemaphore renderFinished { VK_NULL_HANDLE };
    VkFence     inFlight       { VK_NULL_HANDLE };

    // ここから先で command buffer も持たせたくなる（後でOK）
    // VkCommandBuffer cmd { VK_NULL_HANDLE };
};

class VKRenderer : public IRenderer
{
public:
    VKRenderer() {};
    virtual ~VKRenderer() {};
    
    
    //--------------------------------------------------------------------------
    // Initialize / Shutdown
    //--------------------------------------------------------------------------
    bool Initialize(const class Application* app) override;
    void Shutdown() override;
    void UnloadData() override {};
    
    void OnWindowResized(int pixelW, int pixelH) override {};
    
    
    std::shared_ptr<class Shader> GetShader(const std::string& name) {return nullptr;};
    
    PipelineHandle GetPipelineHandle(const std::string& name) override {PipelineHandle h; return h;};
    
    void SetClearColor(const Vector3& color) override {};
    
    std::shared_ptr<RenderTarget> CreateRenderTarget() override {return nullptr;};
    
protected:
    void ApplyState(const RenderItem& it) override {};
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override {};
    
    bool InitializeShadowMapping() override {return false;};
    
    void DrawToRenderTarget(const struct SceneCaptureRequest& req) override {};
    
    void BeginFrame() override {};
    void EndFrame() override {};
    
    void DrawShadowPass() override {};
    void RestoreAfterShadowPass() override {};
    
    void DrawSkyPass() override {};
    void DrawWorldPass() override {};
    void DrawOverlayScreenPass() override {};
    void DrawFadePass() override {};
    void DrawPostEffectPass() override {};
    void DrawUIPass() override {};
    
private:
    SDL_Window*   mWindow                    { nullptr };
    bool mEnableValidation                   { true };
    
    VkInstance               mInstance       { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT mDebugMessenger { VK_NULL_HANDLE };
    VkSurfaceKHR             mSurface        { VK_NULL_HANDLE };
    
    uint32_t mQueueFamilyGraphics { UINT32_MAX };
    uint32_t mQueueFamilyPresent  { UINT32_MAX };
    
    VkPhysicalDevice mPhysicalDevice { VK_NULL_HANDLE };
    std::vector<const char*> mDeviceExtensions;
    
    VkDevice mDevice        { VK_NULL_HANDLE };
    VkQueue  mQueueGraphics { VK_NULL_HANDLE };
    VkQueue  mQueuePresent  { VK_NULL_HANDLE };
    VkExtent2D mSwapchainExtent {};
    VkSwapchainKHR mSwapchain { VK_NULL_HANDLE };
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    

    VkCommandPool mCommandPool { VK_NULL_HANDLE };
    // フレーム同期（例: 2フレーム）
    std::vector<FrameSync> mFrames;
    uint32_t mFrameIndex { 0 }; // Draw/Present で回す用（後で使う）
    
    
    VkSurfaceFormatKHR mSwapchainFormat{};
    VkPresentModeKHR mPresentMode { VK_PRESENT_MODE_FIFO_KHR };
    
    std::unordered_map<std::string, std::shared_ptr<class Shader>> mShaders;
    bool LoadShaders() {return false;};

};

} // namespace toy
