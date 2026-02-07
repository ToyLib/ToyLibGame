// Render/RenderQueue.h
#pragma once

#include "Render/RenderItem.h"
#include "Render/RenderItemPayloads.h"

#include <vector>
#include <cstdint>
#include <SDL3/SDL.h>

namespace toy
{

//==============================================================================
// RenderQueue
//  - 1フレーム分の RenderItem と、その type 別 Payload を保持する
//==============================================================================
class RenderQueue
{
public:
    //--------------------------------------------------------------------------
    // Item management
    //--------------------------------------------------------------------------

    uint32_t Push(const RenderItem& item)
    {
        mItems.emplace_back(item);
        return static_cast<uint32_t>(mItems.size() - 1);
    }

    //--------------------------------------------------------------------------
    // Payload pushers
    //--------------------------------------------------------------------------

    uint32_t PushSpritePayload(const SpritePayload& p)
    {
        mSpritePayloads.emplace_back(p);
        return static_cast<uint32_t>(mSpritePayloads.size() - 1);
    }

    uint32_t PushMeshPayload(const MeshPayload& p)
    {
        mMeshPayloads.emplace_back(p);
        return static_cast<uint32_t>(mMeshPayloads.size() - 1);
    }

    uint32_t PushSkinnedMeshPayload(const SkinnedMeshPayload& p)
    {
        mSkinnedMeshPayloads.emplace_back(p);
        return static_cast<uint32_t>(mSkinnedMeshPayloads.size() - 1);
    }

    uint32_t PushBillboardPayload(const BillboardPayload& p)
    {
        mBillboardPayloads.emplace_back(p);
        return static_cast<uint32_t>(mBillboardPayloads.size() - 1);
    }

    uint32_t PushParticlePayload(const ParticlePayload& p)
    {
        mParticlePayloads.emplace_back(p);
        return static_cast<uint32_t>(mParticlePayloads.size() - 1);
    }

    uint32_t PushSkyDomePayload(const SkyDomePayload& p)
    {
        mSkyDomePayloads.emplace_back(p);
        return static_cast<uint32_t>(mSkyDomePayloads.size() - 1);
    }

    uint32_t PushOverlayPayload(const OverlayPayload& p)
    {
        mOverlayPayloads.emplace_back(p);
        return static_cast<uint32_t>(mOverlayPayloads.size() - 1);
    }

    uint32_t PushDebugPayload(const DebugPayload& p)
    {
        mDebugPayloads.emplace_back(p);
        return static_cast<uint32_t>(mDebugPayloads.size() - 1);
    }

    uint32_t PushSurfacePayload(const SurfacePayload& p)
    {
        mSurfacePayloads.emplace_back(p);
        return static_cast<uint32_t>(mSurfacePayloads.size() - 1);
    }

    void Clear()
    {
        mItems.clear();

        mSpritePayloads.clear();
        mMeshPayloads.clear();
        mSkinnedMeshPayloads.clear();
        mBillboardPayloads.clear();
        mParticlePayloads.clear();
        mSkyDomePayloads.clear();
        mOverlayPayloads.clear();
        mDebugPayloads.clear();
        mSurfacePayloads.clear();
    }

    //--------------------------------------------------------------------------
    // Accessors: items
    //--------------------------------------------------------------------------

    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>&       Items()       { return mItems; }

    //--------------------------------------------------------------------------
    // Accessors: payloads
    //--------------------------------------------------------------------------

    const SpritePayload& GetSpritePayload(uint32_t idx) const
    {
        SDL_assert(idx < mSpritePayloads.size());
        return mSpritePayloads[idx];
    }

    const MeshPayload& GetMeshPayload(uint32_t idx) const
    {
        SDL_assert(idx < mMeshPayloads.size());
        return mMeshPayloads[idx];
    }

    const SkinnedMeshPayload& GetSkinnedMeshPayload(uint32_t idx) const
    {
        SDL_assert(idx < mSkinnedMeshPayloads.size());
        return mSkinnedMeshPayloads[idx];
    }

    const BillboardPayload& GetBillboardPayload(uint32_t idx) const
    {
        SDL_assert(idx < mBillboardPayloads.size());
        return mBillboardPayloads[idx];
    }

    const ParticlePayload& GetParticlePayload(uint32_t idx) const
    {
        SDL_assert(idx < mParticlePayloads.size());
        return mParticlePayloads[idx];
    }

    const SkyDomePayload& GetSkyDomePayload(uint32_t idx) const
    {
        SDL_assert(idx < mSkyDomePayloads.size());
        return mSkyDomePayloads[idx];
    }

    const OverlayPayload& GetOverlayPayload(uint32_t idx) const
    {
        SDL_assert(idx < mOverlayPayloads.size());
        return mOverlayPayloads[idx];
    }

    const DebugPayload& GetDebugPayload(uint32_t idx) const
    {
        SDL_assert(idx < mDebugPayloads.size());
        return mDebugPayloads[idx];
    }

    const SurfacePayload& GetSurfacePayload(uint32_t idx) const
    {
        SDL_assert(idx < mSurfacePayloads.size());
        return mSurfacePayloads[idx];
    }

    //--------------------------------------------------------------------------
    // Sorting
    //--------------------------------------------------------------------------

    void Sort();

private:
    // Render items
    std::vector<RenderItem> mItems;

    // Type-specific payload pools
    std::vector<SpritePayload>      mSpritePayloads;
    std::vector<MeshPayload>        mMeshPayloads;
    std::vector<SkinnedMeshPayload> mSkinnedMeshPayloads;
    std::vector<BillboardPayload>   mBillboardPayloads;
    std::vector<ParticlePayload>    mParticlePayloads;
    std::vector<SkyDomePayload>     mSkyDomePayloads;
    std::vector<OverlayPayload>     mOverlayPayloads;
    std::vector<DebugPayload>       mDebugPayloads;
    std::vector<SurfacePayload>     mSurfacePayloads;
};

} // namespace toy
