//======================================================================
// VKRenderer_WorldUBO.cpp
//  - helpers + UBO updates
//  - MUST match GLSL(std140) layouts exactly
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/RenderItem.h"
#include "Asset/Material/Material.h"    // あるなら
#include "Render/RenderHandles.h"       // TextureHandle, MaterialHandle
#include "Utils/MathUtil.h"             // Matrix4, Vector3
#include "Render/LightingManager.h"
#include "Render/VK/VKUBO.h"
#include "Graphics/Light/PointLightComponent.h"

#include <vulkan/vulkan.h>
#include <cstring>
#include <iostream>

namespace toy
{

//------------------------------------------------------------
// Helpers (declared in VKRenderer.h)
//------------------------------------------------------------
uint32_t VKRenderer::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) &&
            ((memProps.memoryTypes[i].propertyFlags & props) == props))
        {
            return i;
        }
    }
    return UINT32_MAX;
}

static inline WorldFrameResources* GetWorldFrame(std::vector<WorldFrameResources>& frames, uint32_t imageIndex)
{
    if (frames.empty()) return nullptr;
    if (imageIndex >= (uint32_t)frames.size()) return nullptr;
    return &frames[imageIndex];
}

bool VKRenderer::CreateBuffer(VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags props,
                              VkBuffer& outBuf,
                              VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size  = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bi, nullptr, &outBuf) != VK_SUCCESS)
    {
        std::cerr << "[VK] CreateBuffer: vkCreateBuffer failed\n";
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    const uint32_t memType = FindMemoryType(req.memoryTypeBits, props);
    if (memType == UINT32_MAX)
    {
        std::cerr << "[VK] CreateBuffer: FindMemoryType failed\n";
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = memType;

    if (vkAllocateMemory(mDevice, &ai, nullptr, &outMem) != VK_SUCCESS)
    {
        std::cerr << "[VK] CreateBuffer: vkAllocateMemory failed\n";
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(mDevice, outBuf, outMem, 0);
    return true;
}

bool VKRenderer::BeginOneShot(VkCommandBuffer& outCmd)
{
    outCmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = mCommandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(mDevice, &ai, &outCmd) != VK_SUCCESS)
    {
        std::cerr << "[VK] BeginOneShot: vkAllocateCommandBuffers failed\n";
        return false;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(outCmd, &bi) != VK_SUCCESS)
    {
        std::cerr << "[VK] BeginOneShot: vkBeginCommandBuffer failed\n";
        vkFreeCommandBuffers(mDevice, mCommandPool, 1, &outCmd);
        outCmd = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void VKRenderer::EndOneShot(VkCommandBuffer cmd)
{
    if (!cmd) return;

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(mQueueGraphics, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(mQueueGraphics);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
}

void VKRenderer::TransitionImageLayout(VkCommandBuffer cmd,
                                       VkImage image,
                                       VkImageLayout oldLayout,
                                       VkImageLayout newLayout)
{
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &b);
}

void VKRenderer::CopyBufferToImage(VkCommandBuffer cmd,
                                  VkBuffer buffer,
                                  VkImage image,
                                  uint32_t width,
                                  uint32_t height)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
}

//------------------------------------------------------------
// UBO updates
//------------------------------------------------------------
static inline void WriteUBO(VkDevice dev, VkDeviceMemory mem, const void* src, size_t bytes)
{
    void* dst = nullptr;
    if (vkMapMemory(dev, mem, 0, bytes, 0, &dst) != VK_SUCCESS)
    {
        std::cerr << "[VK] vkMapMemory failed\n";
        return;
    }
    std::memcpy(dst, src, bytes);
    vkUnmapMemory(dev, mem);
}

static Matrix4 MakeGLtoVK_ClipCorrection_RowVector()
{
    Matrix4 c = Matrix4::Identity;

    // x' = x
    c.mat[0][0] = 1.0f;

    // y' = y   ★ここを -1 にしない（viewport側で反転する）
    c.mat[1][1] = 1.0f;

    // z' = 0.5*z + 0.5*w  (GL [-1..1] -> VK [0..1])
    c.mat[2][2] = 0.5f;
    c.mat[3][2] = 0.5f;

    // w' = w
    c.mat[3][3] = 1.0f;

    return c;
}

void VKRenderer::UpdateWorldCommonUBO(uint32_t imageIndex)
{
    if (mWorldFrames.empty() || imageIndex >= (uint32_t)mWorldFrames.size())
        return;

    VkDeviceMemory mem = mWorldFrames[imageIndex].worldCommonMem;
    if (mem == VK_NULL_HANDLE)
        return;

    UBO_WorldCommon u{};

    // ViewProj（あなたの row-vector 運用 + 既に correction 済み）
    const Matrix4 vpGL = GetViewMatrix() * GetProjectionMatrix();
    const Matrix4 corr = MakeGLtoVK_ClipCorrection_RowVector();
    u.uViewProj = vpGL * corr;

    const Vector3 cam = GetCameraPosition();
    u.uCameraPos[0] = cam.x; u.uCameraPos[1] = cam.y; u.uCameraPos[2] = cam.z; u.uCameraPos[3] = 0.0f;

    // LightingManager 経由
    Vector3 amb = Vector3(0.8f, 0.8f, 0.8f);
    FogInfo fog{};
    if (mLightingManager)
    {
        amb = mLightingManager->GetAmbientColor();
        fog = mLightingManager->GetFogInfo();
    }

    u.uAmbientLight[0] = amb.x;
    u.uAmbientLight[1] = amb.y;
    u.uAmbientLight[2] = amb.z;
    u.uAmbientLight[3] = 0.0f;

    u.uFogMaxDist = fog.MaxDist;
    u.uFogMinDist = fog.MinDist;
    u._pad2[0] = 0.0f;
    u._pad2[1] = 0.0f;

    u.uFogColor[0] = fog.Color.x;
    u.uFogColor[1] = fog.Color.y;
    u.uFogColor[2] = fog.Color.z;
    u.uFogColor[3] = 0.0f;

    // 影系（今は無効運用）
    u.uLightViewProj0 = Matrix4::Identity;
    u.uLightViewProj1 = Matrix4::Identity;
    u.uCascadeSplit0 = 0.0f;
    u.uCascadeBlend  = 0.0f;
    u.uShadowBias    = 0.0f;
    u.uUseShadow     = 0;

    // toon（今は0固定ならここでOK。将来は Renderer側トグルで）
    u.uUseToon = 0;
    u._pad4[0] = 0.0f;
    u._pad4[1] = 0.0f;
    u._pad4[2] = 0.0f;

    WriteUBO(mDevice, mem, &u, sizeof(u));
}


void VKRenderer::UpdateDirLightUBO(uint32_t imageIndex)
{
    WorldFrameResources* fr = GetWorldFrame(mWorldFrames, imageIndex);
    if (!fr || fr->dirLightMem == VK_NULL_HANDLE)
        return;

    Vector3 dir  = Vector3::Normalize(Vector3(-0.4f, -1.0f, -0.2f));
    Vector3 diff = Vector3(1.0f, 1.0f, 1.0f);
    Vector3 spec = Vector3(1.0f, 1.0f, 1.0f);
    float   sunI = 1.0f;

    if (mLightingManager)
    {
        dir = mLightingManager->GetLightDirection();
        if (dir.LengthSq() <= 0.000001f)
        {
            dir = Vector3::Normalize(Vector3(-0.4f, -1.0f, -0.2f));
        }

        const DirectionalLight& dl = mLightingManager->GetDirectionalLight();
        diff = dl.DiffuseColor;
        spec = dl.SpecColor;

        sunI = mLightingManager->GetSunIntensity();
        if (sunI < 0.0f) sunI = 0.0f;
    }

    diff *= sunI;
    spec *= sunI;

    UBO_DirLight u{};
    u.mDirection[0] = dir.x;  u.mDirection[1] = dir.y;  u.mDirection[2] = dir.z;  u._p0 = 0.0f;
    u.mDiffuseColor[0] = diff.x; u.mDiffuseColor[1] = diff.y; u.mDiffuseColor[2] = diff.z; u._p1 = 0.0f;
    u.mSpecColor[0]    = spec.x; u.mSpecColor[1]    = spec.y; u.mSpecColor[2]    = spec.z; u._p2 = 0.0f;

    WriteUBO(mDevice, fr->dirLightMem, &u, sizeof(u));
}

void VKRenderer::UpdatePointLightUBO(uint32_t imageIndex)
{
    WorldFrameResources* fr = GetWorldFrame(mWorldFrames, imageIndex);
    if (!fr || fr->pointLightMem == VK_NULL_HANDLE)
        return;

    UBO_PointLightBlock u{};
    u.uNumPointLights = 0;
    u._pA = u._pB = u._pC = 0;

    if (mLightingManager)
    {
        const auto& lights = mLightingManager->GetPointLights();

        // 有効なものだけ詰める（最大8）
        int out = 0;
        for (size_t i = 0; i < lights.size() && out < 8; ++i)
        {
            const PointLightComponent* plc = lights[i];
            if (!plc) continue;
            if (!plc->IsEnabled()) continue;

            const Vector3 pos = plc->GetPosition();
            const Vector3 col = plc->GetColor();

            UBO_PointLight& d = u.uPointLights[out];

            d.position[0] = pos.x;
            d.position[1] = pos.y;
            d.position[2] = pos.z;
            d.intensity   = plc->GetIntensity();

            d.color[0] = col.x;
            d.color[1] = col.y;
            d.color[2] = col.z;
            d.constant = plc->GetConstant();

            d.linear    = plc->GetLinear();
            d.quadratic = plc->GetQuadratic();
            d.radius    = plc->GetRadius();
            d._p        = 0.0f;

            ++out;
        }

        u.uNumPointLights = out;
    }

    WriteUBO(mDevice, fr->pointLightMem, &u, sizeof(u));
}
} // namespace toy
