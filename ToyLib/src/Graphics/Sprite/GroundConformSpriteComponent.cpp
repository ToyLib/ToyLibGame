#include "Graphics/Sprite/GroundConformSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Physics/PhysWorld.h"
#include "Physics/GravityComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Asset/Geometry/VertexArray.h"

#include "glad/glad.h"

#include <vector>
#include <cmath>


namespace toy {

GroundConformSpriteComponent::GroundConformSpriteComponent(Actor* owner,
                                                           int drawOrder,
                                                           VisualLayer layer)
    : FootSpriteComponent(owner, drawOrder, layer)
{
    // FootSprite のまま運用（Unlit前提・tint/alpha等）
    
}

void GroundConformSpriteComponent::PreDraw()
{
    RebuildGridIfNeeded();
}

Matrix4 GroundConformSpriteComponent::BuildWorldMatrix() const
{
    // ここが重要：
    // GroundConform は “頂点がすでにワールド座標” なので、
    // ワールド行列は Identity を返す（余計な SRT を掛けない）。
    return Matrix4::Identity;
}


bool GroundConformSpriteComponent::SampleGroundAtXZ(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    auto* owner = GetOwner();
    auto* phys  = owner ? owner->GetApp()->GetPhysWorld() : nullptr;
    if (!owner || !phys) return false;

    // 基準（地面優先/上限用）
    float refY = mBaseY;
    const ColliderComponent* refCol = nullptr;

    if (auto* grav = owner->GetComponent<GravityComponent>())
    {
        if (grav->HasGroundPose())
        {
            const auto& gp = grav->GetGroundPose();
            refY   = gp.y;
            refCol = gp.collider; // GroundPose に統一
        }
    }

    //========================================
    // (A) 乗ってる床 collider を優先（OBB上面を平面サンプル）
    //========================================
    if (refCol)
    {
        const auto* bv = refCol->GetBoundingVolume();
        if (bv)
        {
            auto obbPtr = bv->GetOBB();
            if (obbPtr)
            {
                const OBB& obb = *obbPtr;

                // axis 正規化
                Vector3 ax = obb.axisX;
                Vector3 ay = obb.axisY;
                Vector3 az = obb.axisZ;

                if (ax.LengthSq() > Math::NearZeroEpsilon) ax.Normalize(); else ax = Vector3::UnitX;
                if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize(); else ay = Vector3::UnitY;
                if (az.LengthSq() > Math::NearZeroEpsilon) az.Normalize(); else az = Vector3::UnitZ;

                // 上向き法線に統一
                Vector3 n = ay;
                if (Vector3::Dot(n, Vector3::UnitY) < 0.0f)
                {
                    n *= -1.0f;
                }

                // 上面平面上の1点
                const Vector3 p0 = obb.pos + n * obb.radius.y;

                // 垂直線分 (pos.x,*,pos.z) と平面の交点 y
                const float denom = n.y;
                if (std::fabs(denom) > 1e-6f)
                {
                    const float y =
                        p0.y - (n.x * (pos.x - p0.x) + n.z * (pos.z - p0.z)) / denom;

                    // 上面矩形内か（OBBローカルX/Z）
                    const Vector3 hitPos(pos.x, y, pos.z);
                    const Vector3 d = hitPos - obb.pos;

                    const float lx = Vector3::Dot(d, ax);
                    const float lz = Vector3::Dot(d, az);

                    // 端のチラつき回避：内側に寄せる
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
        // inside 判定できなかったらフォールバックへ
    }

    //========================================
    // (B) refCol が無い or 外に出た → terrain 優先
    //========================================
    GroundHit terrainHit;
    if (phys->GetGroundHitAt(pos, terrainHit))
    {
        outHit = terrainHit;
        return true;
    }

    //========================================
    // (C) collider フォールバック（必要になったら）
    //========================================
    GroundHit anyHit;
    if (phys->GetNearestGroundHitAtXZ(pos, anyHit))
    {
        const float kMaxAbove = 0.20f; // 0.1〜0.3 で調整
        if (anyHit.y <= refY + kMaxAbove)
        {
            outHit = anyHit;
            return true;
        }
    }

    return false;
}

void GroundConformSpriteComponent::RebuildGridIfNeeded()
{
    Actor* owner = GetOwner();
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
    // NEW: GroundPose の変化でも再構築する（停止中の追従）
    //========================================================
    bool groundChanged = false;
    if (auto* grav = owner->GetComponent<GravityComponent>())
    {
        if (grav->HasGroundPose())
        {
            const auto& gp = grav->GetGroundPose();

            // y変化（mBaseY は前回中心地面）
            if (std::fabs(gp.y - mBaseY) > 0.005f)
            {
                groundChanged = true;
            }

            // normal変化：前回 baseHit.normal を保持してないので、簡易に “傾き量” で判定
            // （より厳密にしたいなら mPrevBaseNormal をメンバ追加）
            const float tilt = 1.0f - Vector3::Dot(gp.normal, Vector3::UnitY);
            if (tilt > 0.0005f) // ほんの少しでも傾いていたら更新したい場合
            {
                // 「傾いた状態が続く」だけで毎回更新になるのが嫌なら
                // mPrevBaseNormal をメンバにして Dot で差分判定にしてね
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
// Draw override（FootSpriteComponent の Draw を “VAOだけ差し替え”）
//------------------------------------------------------------------------------
void GroundConformSpriteComponent::Draw()
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

    // 毎フレーム呼ばれる PreDraw 内で必要なら再構築
    PreDraw();

    if (!mGridVAO)
    {
        return;
    }
    
    

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, mIsBlendAdd ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);

    mShader->SetActive();

    // ★ワールド頂点なので Identity
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", Matrix4::Identity);

    const bool useTex = (mTexture != nullptr);
    if (useTex)
    {
        mTexture->SetActive(0);
        mShader->SetTextureUniform("uTexture", 0);
    }

    // FootSprite互換（Unlit）
    mShader->SetIntUniform("uUseTint", 1);
    mShader->SetIntUniform("uUseTexture", useTex ? 1 : 0);
    mShader->SetVectorUniform("uTint", mTint);
    mShader->SetFloatUniform("uAlpha", mAlpha);
    mShader->SetVectorUniform("uDiffuseColor", mDiffuseColor);
    mGridVAO->SetActive();

    
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mGridVAO->GetNumIndices()),
                   GL_UNSIGNED_INT,
                   nullptr);

    renderer->AddDrawCall();
    renderer->AddDrawObject();

    PostDraw();

}

} // namespace toy
