#include "Graphics/Effect/VKParticleBackend.h"

#include "Graphics/VisualComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

#include "Render/IRenderer.h"
#include "Render/VK/VKRenderer.h"
#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"
#include "Render/RenderItemPayloads.h"
#include "Render/RenderBackendState.h"

#include "Asset/Material/Texture.h"
#include "Utils/JsonHelper.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace toy
{

namespace
{
    uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            const bool typeOk = (typeFilter & (1u << i)) != 0;
            const bool propOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
            if (typeOk && propOk)
            {
                return i;
            }
        }

        return UINT32_MAX;
    }

    bool CreateSimpleBuffer(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props,
                            VkBuffer& outBuffer,
                            VkDeviceMemory& outMemory)
    {
        outBuffer = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;

        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size  = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bci, nullptr, &outBuffer) != VK_SUCCESS)
        {
            return false;
        }

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, outBuffer, &req);

        const uint32_t memType =
            FindMemoryTypeIndex(physicalDevice, req.memoryTypeBits, props);
        if (memType == UINT32_MAX)
        {
            vkDestroyBuffer(device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        if (vkAllocateMemory(device, &mai, nullptr, &outMemory) != VK_SUCCESS)
        {
            vkDestroyBuffer(device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            outMemory = VK_NULL_HANDLE;
            return false;
        }

        if (vkBindBufferMemory(device, outBuffer, outMemory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(device, outMemory, nullptr);
            vkDestroyBuffer(device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            outMemory = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool UploadWholeBuffer(VkDevice device,
                           VkDeviceMemory memory,
                           const void* srcData,
                           VkDeviceSize size)
    {
        void* dst = nullptr;
        if (vkMapMemory(device, memory, 0, size, 0, &dst) != VK_SUCCESS)
        {
            return false;
        }

        std::memcpy(dst, srcData, static_cast<size_t>(size));
        vkUnmapMemory(device, memory);
        return true;
    }
}



VKParticleBackend::VKParticleBackend(Actor* owner)
    : IParticleBackend(owner)
{
}

VKParticleBackend::~VKParticleBackend()
{
    ReleaseVK();
}

void VKParticleBackend::Init(const ParticleDesc& desc)
{
    mDesc = desc;

    mDesc.maxParticles    = std::max<uint32_t>(1, mDesc.maxParticles);
    mDesc.particleLife    = std::max(0.01f, mDesc.particleLife);
    mDesc.size            = std::max(0.01f, mDesc.size);
    mDesc.spawnRatePerSec = std::max(0.0f,  mDesc.spawnRatePerSec);
    mDesc.spawnRampSec    = std::max(0.0f,  mDesc.spawnRampSec);

    mRunning = true;
    mTimeAcc = 0.0f;
    mComponentLifeAcc = 0.0f;
    mPendingHardReset = true;
    mSkipDrawFrames   = 2;

    InitIfNeeded();
}

bool VKParticleBackend::InitFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json data;
    file >> data;

    ParticleDesc desc {};

    if (data.contains("mode"))
    {
        std::string modeStr;
        JsonHelper::GetString(data, "mode", modeStr);

        if (modeStr == "Water" || modeStr == "water")
        {
            desc.mode = ParticleMode::Water;
        }
        else if (modeStr == "Smoke" || modeStr == "smoke")
        {
            desc.mode = ParticleMode::Smoke;
        }
        else
        {
            desc.mode = ParticleMode::Spark;
        }
    }

    JsonHelper::GetInt  (data, "maxParticles",    reinterpret_cast<int&>(desc.maxParticles));
    JsonHelper::GetFloat(data, "componentLife",   desc.componentLife);
    JsonHelper::GetFloat(data, "particleLife",    desc.particleLife);
    JsonHelper::GetFloat(data, "size",            desc.size);
    JsonHelper::GetFloat(data, "spawnRatePerSec", desc.spawnRatePerSec);
    JsonHelper::GetFloat(data, "spawnRampSec",    desc.spawnRampSec);
    JsonHelper::GetFloat(data, "spread",          desc.spread);
    JsonHelper::GetFloat(data, "gravity",         desc.gravity);
    JsonHelper::GetFloat(data, "lift",            desc.lift);

    JsonHelper::GetBool(data, "additiveBlend", desc.additiveBlend);
    JsonHelper::GetBool(data, "warmStart",     desc.warmStart);

    Init(desc);
    return true;
}

void VKParticleBackend::Start()
{
    mRunning = true;
}

void VKParticleBackend::Stop()
{
    mRunning = false;
}

void VKParticleBackend::Reset()
{
    mPendingHardReset = true;
    mSkipDrawFrames   = 2;
    mRunning          = false;
}

void VKParticleBackend::Update(float deltaTime)
{
    if (mPendingHardReset)
    {
        mPendingHardReset = false;
        mTimeAcc = 0.0f;
        mComponentLifeAcc = 0.0f;
        mSkipDrawFrames = std::max(mSkipDrawFrames, 1);

        if (mInitialized)
        {
            InitParticleBuffers(false);
        }
        return;
    }

    if (!mRunning || !mInitialized)
    {
        return;
    }

    if (mDesc.componentLife > 0.0f)
    {
        mComponentLifeAcc += deltaTime;
        if (mComponentLifeAcc >= mDesc.componentLife)
        {
            mRunning = false;
            return;
        }
    }

    mTimeAcc += deltaTime;

    // ここは後で compute dispatch を実装
    UpdateParticlesGPU(deltaTime);
}

void VKParticleBackend::GatherRenderItems(RenderQueue& outQueue,
                                          const VisualComponent& host)
{
    if (mSkipDrawFrames > 0)
    {
        --mSkipDrawFrames;
        return;
    }

    if (!host.IsVisible() || !mRunning)
    {
        return;
    }

    if (!mInitialized || !host.GetTexture())
    {
        return;
    }

    auto* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    auto* app = owner->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    ParticlePayload pp{};
    pp.cameraRight     = renderer->GetInvViewMatrix().GetXAxis();
    pp.cameraUp        = renderer->GetInvViewMatrix().GetYAxis();
    pp.particleLifeMax = mDesc.particleLife;
    pp.particleSize    = mDesc.size;

    const uint32_t payloadIndex = outQueue.PushParticlePayload(pp);

    RenderItem it{};
    it.pass      = RenderPass::World;
    it.layer     = host.GetLayer();
    it.drawOrder = host.GetDrawOrder();

    it.type     = RenderItemType::Particle;
    it.dispatch = GetDispatch(it.type);

    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = mDesc.additiveBlend ? BlendMode::Additive : BlendMode::Alpha;
    it.cull       = CullMode::Back;
    it.frontFace  = FrontFace::CCW;

    it.pipeline    = renderer->GetPipelineHandle(mRenderPipelineName);
    it.texture.ptr = host.GetTexture().get();
    it.textureUnit = 0;

    it.world    = Matrix4::Identity;
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();

    // ここがポイント：既存の SpriteQuad を使う
    it.geometry = renderer->GetSpriteQuadHandle();

    it.gpuInstanceVB = CurrentSrcBuffer();

    it.instanceCount = static_cast<int>(mDesc.maxParticles);
    it.topology      = PrimitiveTopology::Triangles;
    it.indexCount    = 6;

    it.payloadIndex = payloadIndex;

    outQueue.Push(it);
}

void VKParticleBackend::InitIfNeeded()
{
    if (mInitialized)
    {
        return;
    }

    InitParticleBuffers(mDesc.warmStart);
    mInitialized = true;
    mPingPong = false;
}

void VKParticleBackend::ReleaseVK()
{
    auto backend = static_cast<VKRenderer*>(GetOwner()->GetApp()->GetRenderer());
    VkDevice device = backend->GetVKDevice();

    if (device == VK_NULL_HANDLE)
    {
        mParticleBufferA = VK_NULL_HANDLE;
        mParticleBufferB = VK_NULL_HANDLE;
        mParticleMemoryA = VK_NULL_HANDLE;
        mParticleMemoryB = VK_NULL_HANDLE;
        mInitialized = false;
        return;
    }

    if (mParticleBufferA != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mParticleBufferA, nullptr);
        mParticleBufferA = VK_NULL_HANDLE;
    }
    if (mParticleMemoryA != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mParticleMemoryA, nullptr);
        mParticleMemoryA = VK_NULL_HANDLE;
    }

    if (mParticleBufferB != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mParticleBufferB, nullptr);
        mParticleBufferB = VK_NULL_HANDLE;
    }
    if (mParticleMemoryB != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mParticleMemoryB, nullptr);
        mParticleMemoryB = VK_NULL_HANDLE;
    }

    mInitialized = false;
    mPingPong = false;
}

void VKParticleBackend::InitParticleBuffers(bool warmStart)
{

    auto backend = static_cast<VKRenderer*>(GetOwner()->GetApp()->GetRenderer());
    VkDevice device = backend->GetVKDevice();
    VkPhysicalDevice physicalDevice = backend->GetVKPhysicalDevice();
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE)
    {
        return;
    }

    // 既存を破棄して作り直し
    if (mParticleBufferA != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mParticleBufferA, nullptr);
        mParticleBufferA = VK_NULL_HANDLE;
    }
    if (mParticleMemoryA != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mParticleMemoryA, nullptr);
        mParticleMemoryA = VK_NULL_HANDLE;
    }

    if (mParticleBufferB != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mParticleBufferB, nullptr);
        mParticleBufferB = VK_NULL_HANDLE;
    }
    if (mParticleMemoryB != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mParticleMemoryB, nullptr);
        mParticleMemoryB = VK_NULL_HANDLE;
    }

    std::vector<ParticleGPU> init;
    init.resize(mDesc.maxParticles);

    const float lifeMax = std::max(0.01f, mDesc.particleLife);

    for (uint32_t i = 0; i < mDesc.maxParticles; ++i)
    {
        ParticleGPU p{};

        // とりあえず見えるように少し散らす
        const float x = static_cast<float>(i % 8) * 0.15f;
        const float z = static_cast<float>(i / 8) * 0.15f;

        p.px = mDesc.emitterOffset.x + x;
        p.py = mDesc.emitterOffset.y;
        p.pz = mDesc.emitterOffset.z + z;
        
        if (warmStart && mDesc.maxParticles > 1)
        {
            p.life = lifeMax * (static_cast<float>(i) / static_cast<float>(mDesc.maxParticles - 1));
        }
        else
        {
            p.life = 0.0f;
        }

        p.vx = 0.0f;
        p.vy = 0.0f;
        p.vz = 0.0f;


        p.pad = 0.0f;
        init[i] = p;
    }

    const VkDeviceSize bufSize =
        static_cast<VkDeviceSize>(sizeof(ParticleGPU) * init.size());

    const VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    const VkMemoryPropertyFlags memProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!CreateSimpleBuffer(physicalDevice, device, bufSize, usage, memProps,
                            mParticleBufferA, mParticleMemoryA))
    {
        std::cerr << "[VKParticleBackend] Create buffer A failed\n";
        return;
    }

    if (!CreateSimpleBuffer(physicalDevice, device, bufSize, usage, memProps,
                            mParticleBufferB, mParticleMemoryB))
    {
        std::cerr << "[VKParticleBackend] Create buffer B failed\n";
        return;
    }

    if (!UploadWholeBuffer(device, mParticleMemoryA, init.data(), bufSize))
    {
        std::cerr << "[VKParticleBackend] Upload buffer A failed\n";
        return;
    }

    if (!UploadWholeBuffer(device, mParticleMemoryB, init.data(), bufSize))
    {
        std::cerr << "[VKParticleBackend] Upload buffer B failed\n";
        return;
    }

    mPingPong = false;
    
}

void VKParticleBackend::UpdateParticlesGPU(float deltaTime)
{
    (void)deltaTime;
    // ここは後で compute pipeline + dispatch を実装
}

VkBuffer VKParticleBackend::CurrentSrcBuffer() const
{
    return mPingPong ? mParticleBufferB : mParticleBufferA;
}

VkBuffer VKParticleBackend::CurrentDstBuffer() const
{
    return mPingPong ? mParticleBufferA : mParticleBufferB;
}

VkDeviceMemory VKParticleBackend::CurrentSrcMemory() const
{
    return mPingPong ? mParticleMemoryB : mParticleMemoryA;
}

VkDeviceMemory VKParticleBackend::CurrentDstMemory() const
{
    return mPingPong ? mParticleMemoryA : mParticleMemoryB;
}

} // namespace toy
