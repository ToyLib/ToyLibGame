#include "Graphics/Sprite/GroundConformSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"

#include "Asset/Material/Texture.h"
#include "Physics/PhysWorld.h"
#include "Physics/GravityComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Asset/Geometry/VertexArray.h"

#include <vector>
#include <cmath>
#include <iostream>

namespace toy {

GroundConformSpriteComponent::GroundConformSpriteComponent(Actor* owner,
                                                           int drawOrder,
                                                           VisualLayer layer)
    : FootSpriteComponent(owner, drawOrder, layer)
{
}

void GroundConformSpriteComponent::PreDraw()
{
    RebuildGridIfNeeded();
}

Matrix4 GroundConformSpriteComponent::BuildWorldMatrix() const
{
    return Matrix4::Identity;
}

//------------------------------------------------------------------------------
// Ground sampling（あなたの元コードを忠実に維持）
//------------------------------------------------------------------------------
bool GroundConformSpriteComponent::SampleGroundAtXZ(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    auto* owner = GetOwner();
    auto* phys  = owner ? owner->GetApp()->GetPhysWorld() : nullptr;
    if (!owner || !phys) return false;

    float refY = mBaseY;
    const ColliderComponent* refCol = nullptr;

    if (auto* grav = owner->GetComponent<GravityComponent>())
    {
        if (grav->HasGroundPose())
        {
            const auto& gp = grav->GetGroundPose();
            refY   = gp.y;
            refCol = gp.collider;
        }
    }

    // (A) 乗ってる床 collider 優先
    if (refCol)
    {
        const auto* bv = refCol->GetBoundingVolume();
        if (bv)
        {
            auto obbPtr = bv->GetOBB();
            if (obbPtr)
            {
                const OBB& obb = *obbPtr;

                Vector3 ax = obb.axisX;
                Vector3 ay = obb.axisY;
                Vector3 az = obb.axisZ;

                if (ax.LengthSq() > Math::NearZeroEpsilon) ax.Normalize(); else ax = Vector3::UnitX;
                if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize(); else ay = Vector3::UnitY;
                if (az.LengthSq() > Math::NearZeroEpsilon) az.Normalize(); else az = Vector3::UnitZ;

                Vector3 n = ay;
                if (Vector3::Dot(n, Vector3::UnitY) < 0.0f)
                {
                    n *= -1.0f;
                }

                const Vector3 p0 = obb.pos + n * obb.radius.y;

                const float denom = n.y;
                if (std::fabs(denom) > 1e-6f)
                {
                    const float y =
                        p0.y - (n.x * (pos.x - p0.x) + n.z * (pos.z - p0.z)) / denom;

                    const Vector3 hitPos(pos.x, y, pos.z);
                    const Vector3 d = hitPos - obb.pos;

                    const float lx = Vector3::Dot(d, ax);
                    const float lz = Vector3::Dot(d, az);

                    const float kEdgeEps = 0.02f;
                    const bool inside =
                        (std::fabs(lx) <= (obb.radius.x - kEdgeEps)) &&
                        (std::fabs(lz) <= (obb.radius.z - kEdgeEps));

                    if (inside)
                    {
                        outHit.hit      = true;
                        outHit.y        = y;
                        outHit.pos      = hitPos;
                        outHit.normal   = n;
                        outHit.source   = GroundSource::Collider;
                        outHit.collider = refCol;
                        return true;
                    }
                }
            }
        }
    }

    // (B) Terrain
    GroundHit terrainHit;
    if (phys->GetGroundHitAt(pos, terrainHit))
    {
        outHit = terrainHit;
        return true;
    }

    // (C) Collider fallback
    GroundHit anyHit;
    if (phys->GetNearestGroundHitAtXZ(pos, anyHit))
    {
        const float kMaxAbove = 0.20f;
        if (anyHit.y <= refY + kMaxAbove)
        {
            outHit = anyHit;
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// Grid rebuild（重い原因：毎フレーム Rebuild を止める）
//------------------------------------------------------------------------------
void GroundConformSpriteComponent::RebuildGridIfNeeded()
{
    Actor* owner = GetOwner();
    if (!owner) return;

    auto* app = owner->GetApp();
    if (!app) return;

    auto* renderer = app->GetRenderer();
    if (!renderer) return;

    // 条件が整ってないなら作らない
    if (mWidth <= 0.0f || mDepth <= 0.0f)
    {
        mGridVAO.reset();
        return;
    }

    const Vector3 ownerPos = owner->GetPosition();
    const Vector3 ownerXZ(ownerPos.x, 0.0f, ownerPos.z);

    const bool movedXZ =
        (std::fabs(ownerXZ.x - mPrevOwnerPos.x) > 0.01f) ||
        (std::fabs(ownerXZ.z - mPrevOwnerPos.z) > 0.01f);

    const bool sizeChanged =
        (std::fabs(mWidth  - mPrevWidth)  > 0.001f) ||
        (std::fabs(mDepth  - mPrevDepth)  > 0.001f) ||
        (mGridDiv != mPrevDiv);

    //========================================================
    // GroundPose の変化でも再構築する（停止中の追従）
    //  ※「傾いてるか」ではなく「前回から変化したか」で判定する
    //========================================================
    bool groundChanged = false;

    const ColliderComponent* curCol    = nullptr;
    float   curY                       = mBaseY;
    Vector3 curNormal                  = Vector3::UnitY;

    if (auto* grav = owner->GetComponent<GravityComponent>())
    {
        if (grav->HasGroundPose())
        {
            const auto& gp = grav->GetGroundPose();
            curCol    = gp.collider;
            curY      = gp.y;
            curNormal = gp.normal;

            // (1) 乗ってる床が変わった
            if (curCol != mPrevGroundCol)
            {
                groundChanged = true;
            }

            // (2) 地面Yが前回から変わった
            if (std::fabs(curY - mPrevBaseY) > 0.01f) // 少し緩め（0.005f→0.01f）
            {
                groundChanged = true;
            }

            // (3) 法線が前回から変わった（角度差で判定）
            // dot が 1 に近いほど同じ方向。0.9995 は約 1.8度差。
            const float dotN = Vector3::Dot(curNormal, mPrevBaseNormal);
            if (dotN < 0.9995f)
            {
                groundChanged = true;
            }
        }
        else
        {
            // GroundPose 無い状態に変わったなら再構築扱い
            if (mPrevGroundCol != nullptr)
            {
                groundChanged = true;
            }
        }
    }

    // 初回 or 変更時のみ再構築
    if (mGridVAO && !movedXZ && !sizeChanged && !groundChanged)
    {
        return;
    }

    mPrevOwnerPos = ownerXZ;
    mPrevWidth = mWidth;
    mPrevDepth = mDepth;
    mPrevDiv = mGridDiv;

    // 中心の地面を取る（貼り付けの基準）
    GroundHit baseHit;
    if (!SampleGroundAtXZ(Vector3(ownerPos.x, 0.0f, ownerPos.z), baseHit))
    {
        std::cerr << "[GroundConform] no ground at XZ: "
                  << ownerPos.x << "," << ownerPos.z << std::endl;
        mHasBase = false;
        mGridVAO.reset();
        return;
    }

    mHasBase = true;
    mBaseY = baseHit.y;

    //========================================================
    // ★GroundPoseキャッシュ更新（次回の差分判定用）
    //========================================================
    {
        if (auto* grav = owner->GetComponent<GravityComponent>())
        {
            if (grav->HasGroundPose())
            {
                const auto& gp = grav->GetGroundPose();
                mPrevGroundCol  = gp.collider;
                mPrevBaseY      = gp.y;
                mPrevBaseNormal = gp.normal;
            }
            else
            {
                mPrevGroundCol  = nullptr;
                mPrevBaseY      = mBaseY;
                mPrevBaseNormal = Vector3::UnitY;
            }
        }
        else
        {
            mPrevGroundCol  = nullptr;
            mPrevBaseY      = mBaseY;
            mPrevBaseNormal = Vector3::UnitY;
        }
    }

    // グリッド頂点数
    const int div = (mGridDiv < 1) ? 1 : mGridDiv;
    const int vxCount = div + 1;
    const int vzCount = div + 1;

    const int numVerts = vxCount * vzCount;
    const int numTris  = div * div * 2;
    const int numIdx   = numTris * 3;

    std::vector<float> verts;
    verts.resize(static_cast<size_t>(numVerts) * 8);

    std::vector<unsigned int> indices;
    indices.resize(static_cast<size_t>(numIdx));

    const float halfW = 0.5f * mWidth  * mOffsetScale;
    const float halfD = 0.5f * mDepth  * mOffsetScale;

    const float yaw = mYaw;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);

    const Vector3 offset = mOffsetPosition;

    auto LocalToWorldXZ = [&](float lx, float lz) -> Vector3
    {
        const float rx = lx * c + lz * s;
        const float rz = -lx * s + lz * c;

        Vector3 w;
        w.x = ownerPos.x + offset.x + rx;
        w.z = ownerPos.z + offset.z + rz;
        w.y = 0.0f;
        return w;
    };

    // 頂点生成
    for (int iz = 0; iz < vzCount; ++iz)
    {
        const float tz = static_cast<float>(iz) / static_cast<float>(div);
        const float lz = (tz * 2.0f - 1.0f) * halfD;

        for (int ix = 0; ix < vxCount; ++ix)
        {
            const float tx = static_cast<float>(ix) / static_cast<float>(div);
            const float lx = (tx * 2.0f - 1.0f) * halfW;

            const int vi = iz * vxCount + ix;
            const int base = vi * 8;

            Vector3 w = LocalToWorldXZ(lx, lz);

            GroundHit h;
            Vector3 n = Vector3::UnitY;
            float gy = mBaseY;

            if (SampleGroundAtXZ(w, h))
            {
                gy = h.y;
                n  = h.normal;

                float dy = gy - mBaseY;
                dy = Math::Clamp(dy, -mMaxDeltaFromCenter, mMaxDeltaFromCenter);
                gy = mBaseY + dy;
            }

            w.y = gy + mGroundLift;

            verts[base + 0] = w.x;
            verts[base + 1] = w.y;
            verts[base + 2] = w.z;

            verts[base + 3] = n.x;
            verts[base + 4] = n.y;
            verts[base + 5] = n.z;

            verts[base + 6] = tx;
            verts[base + 7] = 1.0f - tz;
        }
    }

    int ii = 0;
    for (int iz = 0; iz < div; ++iz)
    {
        for (int ix = 0; ix < div; ++ix)
        {
            const unsigned int i0 = static_cast<unsigned int>(iz * vxCount + ix);
            const unsigned int i1 = static_cast<unsigned int>(iz * vxCount + (ix + 1));
            const unsigned int i2 = static_cast<unsigned int>((iz + 1) * vxCount + ix);
            const unsigned int i3 = static_cast<unsigned int>((iz + 1) * vxCount + (ix + 1));

            indices[ii++] = i0;
            indices[ii++] = i1;
            indices[ii++] = i2;

            indices[ii++] = i1;
            indices[ii++] = i3;
            indices[ii++] = i2;
        }
    }

    mGridVAO = std::make_shared<VertexArray>(
        verts.data(),
        static_cast<unsigned int>(numVerts),
        indices.data(),
        static_cast<unsigned int>(numIdx)
    );
}

//------------------------------------------------------------------------------
// 新パス：RenderQueue に積む
//------------------------------------------------------------------------------
void GroundConformSpriteComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible)
    {
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer || !mShader)
    {
        return;
    }

    if (!mTexture)
    {
        return;
    }

    PreDraw();

    if (!mGridVAO)
    {
        return;
    }

    // ----------------------------------------------------------
    // Payload（Billboard tint/alpha 用）
    // ----------------------------------------------------------
    BillboardPayload bp {};
    bp.color = mTint;   // 無ければ Vector3(1,1,1) でOK
    bp.alpha = mAlpha;  // 無ければ 1.0f でOK
    const uint32_t payloadIndex = queue.PushBillboardPayload(bp);

    RenderItem it {};
    it.pass      = RenderPass::World;
    it.layer     = mLayer;
    it.drawOrder = mDrawOrder;

    it.type     = RenderItemType::Billboard;
    it.dispatch = GetDispatch(it.type);

    it.topology     = PrimitiveTopology::Triangles;
    it.geometry.ptr = mGridVAO.get();
    it.indexCount   = static_cast<int>(mGridVAO->GetNumIndices());

    it.shader = renderer->GetShaderHandle("Unlit");

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    it.viewProj = view * proj;
    it.world    = Matrix4::Identity;

    it.blend      = (mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha);
    it.depthTest  = true;
    it.depthWrite = false;
    it.cull       = CullMode::Back;
    it.frontFace  = FrontFace::CCW;

    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    it.payloadIndex = payloadIndex;

    queue.Push(it);
}

} // namespace toy
