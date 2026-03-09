#include "Graphics/Effect/VKParticleBackend.h"

#include "Graphics/VisualComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

#include "Render/IRenderer.h"
#include "Render/VK/VKRenderer.h"
#include "Render/VK/Pipeline/VKPipelinePresets.h"
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

struct VKParticleUpdatePC
{
    float deltaTime;
    float time;
    float lifeMax;
    int   mode;
    
    float emitterPos[4];
    float misc0[4]; // gravity, lift, spread, spawnRate
    float misc1[4]; // spawnRampSec, maxParticles, reserved, reserved
};
static_assert(sizeof(VKParticleUpdatePC) == 64, "VKParticleUpdatePC must be 64 bytes");


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

            // ★ バッファを作り直したので descriptor set も張り直す
            if (mUseComputeUpdate)
            {
                if (!CreateUpdateDescriptorSets())
                {
                    std::cerr << "[VKParticleBackend] Update: CreateUpdateDescriptorSets failed after hard reset\n";
                    mUseComputeUpdate = false;
                }
            }
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

    if (mUseComputeUpdate)
    {
        mNeedsComputeDispatch = true;
        mPendingComputeDeltaTime = deltaTime;
    }
    else
    {
        UpdateParticlesCPU(deltaTime);
    }
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
    it.blend      = BlendMode::Additive;   // ★まずは固定で加算合成
    it.cull       = CullMode::None;        // ★billboard は cull しない
    it.frontFace  = FrontFace::CCW;
    
    it.pipeline    = renderer->GetPipelineHandle(mRenderPipelineName);
    it.texture.ptr = host.GetTexture().get();
    it.textureUnit = 0;
    
    it.world    = Matrix4::Identity;
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();
    
    // 既存の SpriteQuad を使う
    it.geometry = renderer->GetSpriteQuadHandle();
    
    it.gpuInstanceVB = CurrentSrcBuffer();
    
    it.instanceCount = static_cast<int>(mDesc.maxParticles);
    it.topology      = PrimitiveTopology::Triangles;
    it.indexCount    = 6;
    
    it.payloadIndex = payloadIndex;
    
    outQueue.Push(it);
    
    if (mNeedsComputeDispatch)
    {
        auto* vkRenderer = static_cast<VKRenderer*>(renderer);
        vkRenderer->EnqueueParticleCompute(this, mPendingComputeDeltaTime);
        mNeedsComputeDispatch = false;
    }
}

void VKParticleBackend::InitIfNeeded()
{
    if (mInitialized)
    {
        return;
    }
    
    InitParticleBuffers(mDesc.warmStart);
    
    mUseComputeUpdate = false;
    
    if (CreateUpdatePipeline())
    {
        if (CreateUpdateDescriptorSets())
        {
            mUseComputeUpdate = true;
        }
    }
    
    mInitialized = true;
    mPingPong = false;
}

void VKParticleBackend::ReleaseVK()
{
    auto backend = static_cast<VKRenderer*>(GetOwner()->GetApp()->GetRenderer());
    VkDevice device = backend->GetVKDevice();
    
    mUpdatePipeline.reset();
    mUpdateSetLayout = VK_NULL_HANDLE;
    mUpdateSetAtoB = VK_NULL_HANDLE;
    mUpdateSetBtoA = VK_NULL_HANDLE;
    
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
    auto* backend  = static_cast<VKRenderer*>(renderer);
    if (!backend)
    {
        return;
    }

    VkDevice device = backend->GetVKDevice();
    VkPhysicalDevice physicalDevice = backend->GetVKPhysicalDevice();
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE)
    {
        return;
    }

    //----------------------------------------------------------
    // 既存バッファ破棄
    //----------------------------------------------------------
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

    //----------------------------------------------------------
    // 初期データ作成
    //----------------------------------------------------------
    std::vector<ParticleGPU> init(mDesc.maxParticles);

    const float lifeMax = std::max(0.01f, mDesc.particleLife);

    Vector3 emitterPos = mDesc.emitterOffset + owner->GetPosition();

    auto hash11 = [](float n) -> float
    {
        float x = std::sin(n) * 43758.5453123f;
        return x - std::floor(x);
    };

    auto hash31 = [&](float n) -> Vector3
    {
        return Vector3(
            hash11(n + 1.0f),
            hash11(n + 2.0f),
            hash11(n + 3.0f)
        );
    };

    auto randomDir = [&](int id, float t) -> Vector3
    {
        float n = static_cast<float>(id) * 12.9898f + t * 78.233f;
        Vector3 r = hash31(n) * 2.0f - Vector3(1.0f, 1.0f, 1.0f);

        float lenSq = r.LengthSq();
        if (lenSq < 1.0e-6f)
        {
            return Vector3::UnitY;
        }

        r *= (1.0f / Math::Sqrt(lenSq));
        return r;
    };

    for (uint32_t i = 0; i < mDesc.maxParticles; ++i)
    {
        ParticleGPU p{};

        //======================================================
        // 基本は dead 状態で emitter に置く
        //======================================================
        p.px   = emitterPos.x;
        p.py   = emitterPos.y;
        p.pz   = emitterPos.z;
        p.life = lifeMax + 1.0f;

        p.vx  = 0.0f;
        p.vy  = 0.0f;
        p.vz  = 0.0f;
        p.pad = 0.0f;

        //======================================================
        // warmStart のときだけ alive にして初速も与える
        //======================================================
        if (warmStart && mDesc.maxParticles > 1)
        {
            p.life = lifeMax * (static_cast<float>(i) / static_cast<float>(mDesc.maxParticles - 1));

            const float x = static_cast<float>(i % 8) * 0.05f;
            const float z = static_cast<float>(i / 8) * 0.05f;
            p.px += x;
            p.pz += z;

            Vector3 dir = randomDir(static_cast<int>(i), 0.0f);

            if (mDesc.mode == ParticleMode::Water)
            {
                dir.y = -std::abs(dir.y) * 0.6f - 0.4f;
            }
            else if (mDesc.mode == ParticleMode::Smoke)
            {
                dir.y = std::abs(dir.y) * 0.6f + 0.4f;
            }

            p.vx = dir.x * mDesc.spread;
            p.vy = dir.y * mDesc.spread;
            p.vz = dir.z * mDesc.spread;
        }

        init[i] = p;
    }

    //----------------------------------------------------------
    // GPUバッファ作成
    //----------------------------------------------------------
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

void VKParticleBackend::UpdateParticlesCPU(float deltaTime)
{
    auto backend = static_cast<VKRenderer*>(GetOwner()->GetApp()->GetRenderer());
    VkDevice device = backend->GetVKDevice();
    if (device == VK_NULL_HANDLE)
    {
        return;
    }
    
    const uint32_t count = mDesc.maxParticles;
    if (count == 0)
    {
        return;
    }
    
    VkDeviceMemory srcMem = CurrentSrcMemory();
    VkDeviceMemory dstMem = CurrentDstMemory();
    if (srcMem == VK_NULL_HANDLE || dstMem == VK_NULL_HANDLE)
    {
        return;
    }
    
    const float lifeMax = std::max(0.01f, mDesc.particleLife);
    
    //==========================================================
    // GL版の uEmitterPos 相当
    //==========================================================
    Vector3 emitterPos = mDesc.emitterOffset;
    if (GetOwner())
    {
        emitterPos += GetOwner()->GetPosition();
    }
    
    const VkDeviceSize bufSize =
    static_cast<VkDeviceSize>(sizeof(ParticleGPU) * count);
    
    ParticleGPU* src = nullptr;
    ParticleGPU* dst = nullptr;
    
    if (vkMapMemory(device, srcMem, 0, bufSize, 0, reinterpret_cast<void**>(&src)) != VK_SUCCESS)
    {
        return;
    }
    
    if (vkMapMemory(device, dstMem, 0, bufSize, 0, reinterpret_cast<void**>(&dst)) != VK_SUCCESS)
    {
        vkUnmapMemory(device, srcMem);
        return;
    }
    
    //==========================================================
    // hash / randomDir / spawnGate
    //  - GL版の式に寄せる
    //==========================================================
    auto hash11 = [](float n) -> float
    {
        float x = std::sin(n) * 43758.5453123f;
        return x - std::floor(x);
    };
    
    auto hash31 = [&](float n) -> Vector3
    {
        return Vector3(
                       hash11(n + 1.0f),
                       hash11(n + 2.0f),
                       hash11(n + 3.0f)
                       );
    };
    
    auto randomDir = [&](int id, float t) -> Vector3
    {
        float n = static_cast<float>(id) * 12.9898f + t * 78.233f;
        Vector3 r = hash31(n) * 2.0f - Vector3(1.0f, 1.0f, 1.0f);
        
        float lenSq = r.LengthSq();
        if (lenSq < 1.0e-6f)
        {
            r = Vector3::UnitY;
        }
        else
        {
            r *= (1.0f / Math::Sqrt(lenSq));
        }
        return r;
    };
    
    auto spawnGate = [&](int id) -> bool
    {
        float ramp = (mDesc.spawnRampSec <= 0.0f)
        ? 1.0f
        : Math::Clamp(mTimeAcc / mDesc.spawnRampSec, 0.0f, 1.0f);
        
        float p = 1.0f - std::exp(-std::max(mDesc.spawnRatePerSec, 0.0f) * deltaTime);
        p *= ramp;
        
        float r = hash11(static_cast<float>(id) * 3.17f + std::floor(mTimeAcc * 60.0f) * 0.77f);
        return (r < p);
    };
    
    //==========================================================
    // update
    //==========================================================
    for (uint32_t i = 0; i < count; ++i)
    {
        ParticleGPU d = src[i];
        
        Vector3 pos(d.px, d.py, d.pz);
        Vector3 vel(d.vx, d.vy, d.vz);
        float life = d.life;
        
        const bool dead = (life >= lifeMax);
        
        if (dead)
        {
            //--------------------------------------------------
            // dead 粒：respawn するか、死体のまま維持
            //--------------------------------------------------
            if (mDesc.spawnRatePerSec > 0.0f && spawnGate(static_cast<int>(i)))
            {
                life = 0.0f;
                pos  = emitterPos;
                
                Vector3 dir = randomDir(static_cast<int>(i), mTimeAcc);
                
                if (mDesc.mode == ParticleMode::Water)
                {
                    dir.y = -std::abs(dir.y) * 0.6f - 0.4f;
                }
                else if (mDesc.mode == ParticleMode::Smoke)
                {
                    dir.y =  std::abs(dir.y) * 0.6f + 0.4f;
                }
                
                vel = dir * mDesc.spread;
            }
            else
            {
                life = lifeMax + 1.0f;
                vel  = Vector3::Zero;
            }
        }
        else
        {
            //--------------------------------------------------
            // alive 粒：GL版同様、力を加えて積分
            //--------------------------------------------------
            if (mDesc.mode == ParticleMode::Water)
            {
                vel.y -= mDesc.gravity * deltaTime;
            }
            else if (mDesc.mode == ParticleMode::Smoke)
            {
                vel.y += mDesc.lift * deltaTime;
            }
            
            pos  += vel * deltaTime;
            life += deltaTime;
            
            if (life >= lifeMax)
            {
                life = lifeMax + 1.0f;
            }
        }
        
        d.px   = pos.x;
        d.py   = pos.y;
        d.pz   = pos.z;
        d.vx   = vel.x;
        d.vy   = vel.y;
        d.vz   = vel.z;
        d.life = life;
        d.pad  = 0.0f;
        
        dst[i] = d;
    }
    
    vkUnmapMemory(device, dstMem);
    vkUnmapMemory(device, srcMem);
    
    // 次フレームは更新後を読む
    mPingPong = !mPingPong;
    
    std::cerr << "Particle Updated by CPU " << std::endl;
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

bool VKParticleBackend::CreateUpdatePipeline()
{
    auto* app = GetOwner() ? GetOwner()->GetApp() : nullptr;
    if (!app)
    {
        return false;
    }
    
    auto* renderer = app->GetRenderer();
    auto* backend = static_cast<VKRenderer*>(renderer);
    if (!backend)
    {
        return false;
    }
    
    VkDevice device = backend->GetVKDevice();
    if (device == VK_NULL_HANDLE)
    {
        return false;
    }
    
    const std::string base = backend->GetShaderPath() + "VK/spv/";
    
    mUpdatePipeline.reset();
    
    VKComputePipelineDesc desc =
    toy::VKPipelinePresets::MakeParticleUpdateCompute(base);
    
    mUpdatePipeline = std::make_unique<VKComputePipeline>();
    if (!mUpdatePipeline->Create(device, desc))
    {
        std::cerr << "[VKParticleBackend] CreateUpdatePipeline failed\n";
        mUpdatePipeline.reset();
        return false;
    }
    
    mUpdateSetLayout = mUpdatePipeline->GetSetLayout(0);
    if (mUpdateSetLayout == VK_NULL_HANDLE)
    {
        std::cerr << "[VKParticleBackend] CreateUpdatePipeline: set0 layout null\n";
        mUpdatePipeline.reset();
        return false;
    }
    
    return true;
}

bool VKParticleBackend::CreateUpdateDescriptorSets()
{
    auto* app = GetOwner() ? GetOwner()->GetApp() : nullptr;
    if (!app)
    {
        return false;
    }
    
    auto* renderer = app->GetRenderer();
    auto* backend = static_cast<VKRenderer*>(renderer);
    if (!backend)
    {
        return false;
    }
    
    VkDevice device = backend->GetVKDevice();
    VkDescriptorPool descPool = backend->GetDescriptorPool();
    
    if (device == VK_NULL_HANDLE || descPool == VK_NULL_HANDLE)
    {
        return false;
    }
    
    if (mUpdateSetLayout == VK_NULL_HANDLE)
    {
        return false;
    }
    
    mUpdateSetAtoB = VK_NULL_HANDLE;
    mUpdateSetBtoA = VK_NULL_HANDLE;
    
    VkDescriptorSetLayout layouts[2] =
    {
        mUpdateSetLayout,
        mUpdateSetLayout
    };
    
    VkDescriptorSet sets[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descPool;
    ai.descriptorSetCount = 2;
    ai.pSetLayouts = layouts;
    
    if (vkAllocateDescriptorSets(device, &ai, sets) != VK_SUCCESS)
    {
        std::cerr << "[VKParticleBackend] CreateUpdateDescriptorSets: allocate failed\n";
        return false;
    }
    
    mUpdateSetAtoB = sets[0];
    mUpdateSetBtoA = sets[1];
    
    auto writeSet = [&](VkDescriptorSet ds, VkBuffer srcBuf, VkBuffer dstBuf)
    {
        VkDescriptorBufferInfo srcInfo{};
        srcInfo.buffer = srcBuf;
        srcInfo.offset = 0;
        srcInfo.range  = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo dstInfo{};
        dstInfo.buffer = dstBuf;
        dstInfo.offset = 0;
        dstInfo.range  = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet writes[2]{};
        
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = ds;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo = &srcInfo;
        
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = ds;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &dstInfo;
        
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    };
    
    if (mParticleBufferA == VK_NULL_HANDLE || mParticleBufferB == VK_NULL_HANDLE)
    {
        std::cerr << "[VKParticleBackend] CreateUpdateDescriptorSets: particle buffers not ready\n";
        return false;
    }
    
    writeSet(mUpdateSetAtoB, mParticleBufferA, mParticleBufferB);
    writeSet(mUpdateSetBtoA, mParticleBufferB, mParticleBufferA);
    
    return true;
}

void VKParticleBackend::UpdateParticlesCompute(VkCommandBuffer cmd, float deltaTime)
{
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    if (!mUpdatePipeline || !mUpdatePipeline->IsValid())
    {
        return;
    }

    VkDescriptorSet updateSet = mPingPong ? mUpdateSetBtoA : mUpdateSetAtoB;
    if (updateSet == VK_NULL_HANDLE)
    {
        return;
    }

    const float lifeMax = std::max(0.01f, mDesc.particleLife);

    Vector3 emitterPos = mDesc.emitterOffset;
    if (GetOwner())
    {
        emitterPos += GetOwner()->GetPosition();
    }

    VKParticleUpdatePC pc{};
    pc.deltaTime = deltaTime;
    pc.time      = mTimeAcc;
    pc.lifeMax   = lifeMax;
    pc.mode      = static_cast<int>(mDesc.mode);

    pc.emitterPos[0] = emitterPos.x;
    pc.emitterPos[1] = emitterPos.y;
    pc.emitterPos[2] = emitterPos.z;
    pc.emitterPos[3] = 0.0f;

    pc.misc0[0] = mDesc.gravity;
    pc.misc0[1] = mDesc.lift;
    pc.misc0[2] = mDesc.spread;
    pc.misc0[3] = mDesc.spawnRatePerSec;

    pc.misc1[0] = mDesc.spawnRampSec;
    pc.misc1[1] = static_cast<float>(mDesc.maxParticles);
    pc.misc1[2] = 0.0f;
    pc.misc1[3] = 0.0f;

    mUpdatePipeline->Bind(cmd);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        mUpdatePipeline->GetPipelineLayout(),
        0,
        1,
        &updateSet,
        0,
        nullptr);

    vkCmdPushConstants(
        cmd,
        mUpdatePipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(VKParticleUpdatePC),
        &pc);

    constexpr uint32_t kLocalSizeX = 64;
    const uint32_t groupCountX =
        (mDesc.maxParticles + kLocalSizeX - 1) / kLocalSizeX;

    vkCmdDispatch(cmd, groupCountX, 1, 1);

    // compute 書き込み → vertex input / shader read へ
    VkBuffer barrierBuffer = CurrentDstBuffer();

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = barrierBuffer;
    barrier.offset = 0;
    barrier.size   = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    mPingPong = !mPingPong;
}

} // namespace toy
