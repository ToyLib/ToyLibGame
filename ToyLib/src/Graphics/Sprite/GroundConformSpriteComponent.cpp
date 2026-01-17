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
    auto* phys  = owner->GetApp()->GetPhysWorld();
    if (!phys) return false;

    // 基準（地面優先/上限用）
    float refY = mBaseY;
    const ColliderComponent* refCol = nullptr;

    if (auto* grav = owner->GetComponent<GravityComponent>())
    {
        if (grav->HasGroundPose())
        {
            refY = grav->GetGroundY();
        }
        refCol = grav->GetGroundCollider(); // ★ここは grav がいる時だけ
    }

    //========================================
    // (A) 乗ってる床 collider を優先
    //========================================
    if (refCol)
    {
        const Cube aabb = refCol->GetBoundingVolume()->GetWorldAABB();

        // ★床の端のチラつきがあるなら 0.03〜0.06 に上げるのが効く
        const float kEdgeEps = 0.02f;
        const bool insideXZ =
            (pos.x >= aabb.min.x + kEdgeEps) && (pos.x <= aabb.max.x - kEdgeEps) &&
            (pos.z >= aabb.min.z + kEdgeEps) && (pos.z <= aabb.max.z - kEdgeEps);

        if (insideXZ)
        {
            outHit.hit      = true;
            outHit.y        = aabb.max.y;
            outHit.pos      = Vector3(pos.x, outHit.y, pos.z);
            outHit.normal   = Vector3::UnitY;
            outHit.source   = GroundSource::Collider;
            outHit.collider = refCol;
            return true;
        }
        // outside ならフォールバックへ
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
    //     ここを入れるなら “refYより上は捨てる” を追加するのが肝
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

    // ownerのXZが動いたら更新（Yは無視してOK）
    const Vector3 ownerPos = owner->GetPosition();
    const Vector3 ownerXZ(ownerPos.x, 0.0f, ownerPos.z);

    const bool movedXZ =
        (std::fabs(ownerXZ.x - mPrevOwnerPos.x) > 0.01f) ||
        (std::fabs(ownerXZ.z - mPrevOwnerPos.z) > 0.01f);

    const bool sizeChanged =
        (std::fabs(mWidth  - mPrevWidth)  > 0.001f) ||
        (std::fabs(mDepth  - mPrevDepth)  > 0.001f) ||
        (mGridDiv != mPrevDiv);

    // 初回 or 変更時のみ再構築（慎重運用）
    if (mGridVAO && !movedXZ && !sizeChanged)
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

    // VertexArray(スプライト用)は 1頂点あたり 8 float（pos xyz + normal xyz + uv xy）
    std::vector<float> verts;
    verts.resize(static_cast<size_t>(numVerts) * 8);

    std::vector<unsigned int> indices;
    indices.resize(static_cast<size_t>(numIdx));

    // 生成範囲：中心基準で width/depth
    const float halfW = 0.5f * mWidth  * mOffsetScale;
    const float halfD = 0.5f * mDepth  * mOffsetScale;

    // ローカルXZ → ワールドXZ（Yaw + offset）
    const float yaw = mYaw;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);

    const Vector3 offset = mOffsetPosition;

    auto LocalToWorldXZ = [&](float lx, float lz) -> Vector3
    {
        // Yaw回転（Y軸）
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

                // ガタつき抑制：中心との差をクランプ
                float dy = gy - mBaseY;
                dy = Math::Clamp(dy, -mMaxDeltaFromCenter, mMaxDeltaFromCenter);
                gy = mBaseY + dy;
            }

            w.y = gy + mGroundLift;

            // pos
            verts[base + 0] = w.x;
            verts[base + 1] = w.y;
            verts[base + 2] = w.z;

            // normal（Unlitなら使わないけど、フォーマット互換用に入れておく）
            verts[base + 3] = n.x;
            verts[base + 4] = n.y;
            verts[base + 5] = n.z;

            // uv
            verts[base + 6] = tx;
            verts[base + 7] = 1.0f - tz;
        }
    }

    // index生成（通常グリッド）
    int ii = 0;
    for (int iz = 0; iz < div; ++iz)
    {
        for (int ix = 0; ix < div; ++ix)
        {
            const unsigned int i0 = static_cast<unsigned int>(iz * vxCount + ix);
            const unsigned int i1 = static_cast<unsigned int>(iz * vxCount + (ix + 1));
            const unsigned int i2 = static_cast<unsigned int>((iz + 1) * vxCount + ix);
            const unsigned int i3 = static_cast<unsigned int>((iz + 1) * vxCount + (ix + 1));

            // (i0, i2, i1)
            indices[ii++] = i0;
            indices[ii++] = i1;
            indices[ii++] = i2;

            // (i1, i2, i3)
            indices[ii++] = i1;
            indices[ii++] = i3;
            indices[ii++] = i2;
        }
    }

    // VertexArray 作成（GL_STATIC_DRAW前提なので作り直し）
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
