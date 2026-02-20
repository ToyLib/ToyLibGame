// Render/RenderQueue.h
#pragma once

#include "Render/RenderItem.h"
#include "Render/RenderItemPayloads.h"
#include <vector>
#include <cstdint>
#include <SDL3/SDL.h>

namespace toy {

class RenderQueue
{
public:
    uint32_t Push(const RenderItem& item)
    {
        mItems.emplace_back(item);
        return static_cast<uint32_t>(mItems.size() - 1);
    }

    // payload pushers
    uint32_t PushSpritePayload     (const SpritePayload& p)     { mSprite.emplace_back(p);   return (uint32_t)mSprite.size()-1; }
    uint32_t PushMeshPayload       (const MeshPayload& p)       { mMesh.emplace_back(p);     return (uint32_t)mMesh.size()-1; }
    uint32_t PushSkinnedMeshPayload(const SkinnedMeshPayload& p){ mSkinned.emplace_back(p);  return (uint32_t)mSkinned.size()-1; }
    uint32_t PushBillboardPayload  (const BillboardPayload& p)  { mBillboard.emplace_back(p);return (uint32_t)mBillboard.size()-1; }
    uint32_t PushParticlePayload   (const ParticlePayload& p)   { mParticle.emplace_back(p); return (uint32_t)mParticle.size()-1; }
    uint32_t PushSkyDomePayload    (const SkyDomePayload& p)    { mSky.emplace_back(p);      return (uint32_t)mSky.size()-1; }
    uint32_t PushOverlayPayload    (const OverlayPayload& p)    { mOverlay.emplace_back(p);  return (uint32_t)mOverlay.size()-1; }
    uint32_t PushDebugPayload      (const DebugPayload& p)      { mDebug.emplace_back(p);    return (uint32_t)mDebug.size()-1; }
    uint32_t PushSurfacePayload    (const SurfacePayload& p)    { mSurface.emplace_back(p);  return (uint32_t)mSurface.size()-1; }

    void Clear()
    {
        mItems.clear();
        mSprite.clear();
        mMesh.clear();
        mSkinned.clear();
        mBillboard.clear();
        mParticle.clear();
        mSky.clear();
        mOverlay.clear();
        mDebug.clear();
        mSurface.clear();
    }

    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>&       Items()       { return mItems; }

    // getters (今まで通り)
    const SpritePayload&      GetSpritePayload(uint32_t idx) const      { SDL_assert(idx < mSprite.size());   return mSprite[idx]; }
    const MeshPayload&        GetMeshPayload(uint32_t idx) const        { SDL_assert(idx < mMesh.size());     return mMesh[idx]; }
    const SkinnedMeshPayload& GetSkinnedMeshPayload(uint32_t idx) const { SDL_assert(idx < mSkinned.size());  return mSkinned[idx]; }
    const BillboardPayload&   GetBillboardPayload(uint32_t idx) const   { SDL_assert(idx < mBillboard.size());return mBillboard[idx]; }
    const ParticlePayload&    GetParticlePayload(uint32_t idx) const    { SDL_assert(idx < mParticle.size()); return mParticle[idx]; }
    const SkyDomePayload&     GetSkyDomePayload(uint32_t idx) const     { SDL_assert(idx < mSky.size());      return mSky[idx]; }
    const OverlayPayload&     GetOverlayPayload(uint32_t idx) const     { SDL_assert(idx < mOverlay.size());  return mOverlay[idx]; }
    const DebugPayload&       GetDebugPayload(uint32_t idx) const       { SDL_assert(idx < mDebug.size());    return mDebug[idx]; }
    const SurfacePayload&     GetSurfacePayload(uint32_t idx) const     { SDL_assert(idx < mSurface.size());  return mSurface[idx]; }

    void Sort();

private:
    std::vector<RenderItem> mItems;

    std::vector<SpritePayload>      mSprite;
    std::vector<MeshPayload>        mMesh;
    std::vector<SkinnedMeshPayload> mSkinned;
    std::vector<BillboardPayload>   mBillboard;
    std::vector<ParticlePayload>    mParticle;
    std::vector<SkyDomePayload>     mSky;
    std::vector<OverlayPayload>     mOverlay;
    std::vector<DebugPayload>       mDebug;
    std::vector<SurfacePayload>     mSurface;
};

} // namespace toy
