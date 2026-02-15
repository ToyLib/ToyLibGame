//======================================================================
// VKRenderer_WorldUBO.cpp
//  - Linker undefined symbols fix (helpers + UBO updates)
//  - Matches your current VKRenderer.h signatures
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKPipeline.h"
#include "Render/RenderItem.h"          // RenderItem
#include "Asset/Material/Material.h"    // (if you have it; otherwise remove include)
#include "Asset/Material/Texture.h"     // Texture, TextureHandle
#include "Render/VK/VKTextureGPU.h"     // VKTextureGPU (bridge)
#include "Utils/MathUtil.h"             // Matrix4, Vector3

#include <vulkan/vulkan.h>
#include <cstring>
#include <iostream>

namespace toy
{

//------------------------------------------------------------
// std140 friendly POD blocks
//------------------------------------------------------------
struct UBO_WorldCommon
{
    Matrix4 uViewProj;

    alignas(16) float uCameraPos[4];     // xyz + pad
    alignas(16) float uAmbientLight[4];  // xyz + pad

    alignas(16) float uFogParams[4];     // max, min, pad, pad
    alignas(16) float uFogColor[4];      // rgb + pad

    // keep compatibility (unused now)
    Matrix4 uLightViewProj0;
    Matrix4 uLightViewProj1;

    alignas(16) float uShadowParams0[4]; // split0, blend, bias, (pad)
    alignas(16) int   uShadowParams1[4]; // useShadow, useToon, pad, pad
};

struct UBO_MaterialParams
{
    alignas(16) float uDiffuseColor[4];   // rgb + uUseTexture (as int bit-cast is messy, so store float then write int separately)
    alignas(16) float uUniformColor[4];   // rgb + uOverrideColor
    alignas(16) float uSpecPower[4];      // specPower + pad...
    // NOTE: std140 wants 16-byte multiples anyway.
};

struct UBO_DirLight
{
    alignas(16) float dir[4];     // xyz + pad
    alignas(16) float diff[4];    // rgb + pad
    alignas(16) float spec[4];    // rgb + pad
};

struct UBO_PointLight
{
    alignas(16) float position[4];   // xyz + intensity
    alignas(16) float color[4];      // rgb + constant
    alignas(16) float params[4];     // linear, quadratic, radius, pad
};

struct UBO_PointLightBlock
{
    alignas(16) int header[4]; // numPointLights, pad, pad, pad
    UBO_PointLight lights[8];
};

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

    // minimal set (enough for staging upload)
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
// UBO updates (declared in VKRenderer.h)
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

    // y' = -y
    c.mat[1][1] = -1.0f;

    // z' = 0.5*z + 0.5*w
    c.mat[2][2] = 0.5f;
    c.mat[3][2] = 0.5f;

    // w' = w
    c.mat[3][3] = 1.0f;

    return c;
}

void VKRenderer::UpdateWorldCommonUBO(uint32_t /*imageIndex*/)
{
    if (!mWorldCommonUBOMem) return;

    UBO_WorldCommon u{};

    // row-vector: worldPos * (View*Proj*Corr)
    const Matrix4 vpGL = GetViewMatrix() * GetProjectionMatrix();
    const Matrix4 corr = MakeGLtoVK_ClipCorrection_RowVector();

    u.uViewProj = vpGL * corr;

    const Vector3 cam = GetCameraPosition();
    u.uCameraPos[0] = cam.x; u.uCameraPos[1] = cam.y; u.uCameraPos[2] = cam.z; u.uCameraPos[3] = 0.0f;

    // defaults
    u.uAmbientLight[0] = 0.2f; u.uAmbientLight[1] = 0.2f; u.uAmbientLight[2] = 0.2f; u.uAmbientLight[3] = 0.0f;

    u.uFogParams[0] = 999999.0f;
    u.uFogParams[1] = 999998.0f;
    u.uFogParams[2] = 0.0f;
    u.uFogParams[3] = 0.0f;

    u.uFogColor[0] = 0.0f; u.uFogColor[1] = 0.0f; u.uFogColor[2] = 0.0f; u.uFogColor[3] = 0.0f;

    u.uLightViewProj0 = Matrix4::Identity;
    u.uLightViewProj1 = Matrix4::Identity;

    u.uShadowParams0[0] = 0.0f;
    u.uShadowParams0[1] = 0.0f;
    u.uShadowParams0[2] = 0.0f;
    u.uShadowParams0[3] = 0.0f;

    u.uShadowParams1[0] = 0; // useShadow
    u.uShadowParams1[1] = 0; // useToon
    u.uShadowParams1[2] = 0;
    u.uShadowParams1[3] = 0;

    WriteUBO(mDevice, mWorldCommonUBOMem, &u, sizeof(u));
}

void VKRenderer::UpdateMaterialParamsUBO(const RenderItem& it)
{
    if (!mMaterialParamsUBOMem) return;

    // defaults
    Vector3 diffuse(1.0f, 1.0f, 1.0f);
    Vector3 ucol(0.0f, 0.0f, 0.0f);

    int useTex = 0;
    int overrideCol = 0;
    float specPower = 32.0f;

    TextureHandle texH{};
    if (it.material.ptr)
    {
        // your earlier comment implies this exists:
        texH = it.material.ptr->GetDiffuseTextureHandle();
        if (texH.IsValid()) useTex = 1;
    }

    if (it.overrideColor)
    {
        overrideCol = 1;
        ucol = it.overrideColorValue;
    }

    UBO_MaterialParams u{};
    u.uDiffuseColor[0] = diffuse.x;
    u.uDiffuseColor[1] = diffuse.y;
    u.uDiffuseColor[2] = diffuse.z;
    u.uDiffuseColor[3] = *(float*)&useTex; // store bit pattern (matches std140 int slot)

    u.uUniformColor[0] = ucol.x;
    u.uUniformColor[1] = ucol.y;
    u.uUniformColor[2] = ucol.z;
    u.uUniformColor[3] = *(float*)&overrideCol;

    u.uSpecPower[0] = specPower;
    u.uSpecPower[1] = 0.0f;
    u.uSpecPower[2] = 0.0f;
    u.uSpecPower[3] = 0.0f;

    WriteUBO(mDevice, mMaterialParamsUBOMem, &u, sizeof(u));
}

void VKRenderer::UpdateDirLightUBO()
{
    if (!mDirLightUBOMem) return;

    // defaults: top-left-ish sun
    Vector3 dir(-0.4f, -1.0f, -0.2f);
    Vector3 diff(1.0f, 1.0f, 1.0f);
    Vector3 spec(1.0f, 1.0f, 1.0f);

    UBO_DirLight u{};
    u.dir[0] = dir.x; u.dir[1] = dir.y; u.dir[2] = dir.z; u.dir[3] = 0.0f;
    u.diff[0] = diff.x; u.diff[1] = diff.y; u.diff[2] = diff.z; u.diff[3] = 0.0f;
    u.spec[0] = spec.x; u.spec[1] = spec.y; u.spec[2] = spec.z; u.spec[3] = 0.0f;

    WriteUBO(mDevice, mDirLightUBOMem, &u, sizeof(u));
}

void VKRenderer::UpdatePointLightUBO()
{
    if (!mPointLightUBOMem) return;

    UBO_PointLightBlock u{};
    u.header[0] = 0; // numPointLights
    u.header[1] = 0;
    u.header[2] = 0;
    u.header[3] = 0;

    WriteUBO(mDevice, mPointLightUBOMem, &u, sizeof(u));
}

} // namespace toy
