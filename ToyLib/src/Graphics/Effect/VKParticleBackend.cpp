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

namespace toy
{

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

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    Vector3 camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
    Vector3 camUp   (view.mat[0][1], view.mat[1][1], view.mat[2][1]);
    camRight.Normalize();
    camUp.Normalize();

    ParticlePayload pp {};
    pp.cameraRight     = camRight;
    pp.cameraUp        = camUp;
    pp.particleLifeMax = mDesc.particleLife;
    pp.particleSize    = mDesc.size;

    const uint32_t payloadIndex = outQueue.PushParticlePayload(pp);

    RenderItem it {};
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
    it.viewProj = viewProj;

    // ここは後で VK 用の quad/instance buffer 参照に置き換える
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
    //auto& backend = RenderBackendState::Get();
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
    (void)warmStart;
    // ここは後で CreateBufferHostVisible / 初期データ upload を実装
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
