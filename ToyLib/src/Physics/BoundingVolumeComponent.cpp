#include "Physics/BoundingVolumeComponent.h"
#include "Graphics/Effect/WireframeComponent.h"
#include "Graphics/Mesh/MeshComponent.h"
#include "Engine/Core/Actor.h"
#include "Asset/Geometry/Polygon.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Mesh.h"
#include "Engine/Render/Shader.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"
#include "Engine/Debug/DebugWireframeComponent.h"

#include <vector>
#include <algorithm>


namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
// ・AABB / OBB / Polygon を初期化
// ・デバッグモード時はワイヤーフレーム用コンポーネントを生成
//------------------------------------------------------------------------------
BoundingVolumeComponent::BoundingVolumeComponent(Actor* a)
    : Component(a)
{
    mBoundingBox = std::make_shared<Cube>();
    mObb         = std::make_shared<OBB>();    
    // デバッグ時のみワイヤーフレームを生成して可視化
    if (GetOwner()->GetApp()->GetEnableDebug())
    {
        mWireframe = std::make_unique<DebugWireframeComponent>(GetOwner(), 1000);
        Vector3 color = GetOwner()->GetApp()->GetRenderer()->GetWireColor();
        mWireframe->SetColor(color);
    }
}

//------------------------------------------------------------------------------
// デストラクタ
//------------------------------------------------------------------------------
BoundingVolumeComponent::~BoundingVolumeComponent()
{
}

//------------------------------------------------------------------------------
// OnUpdateWorldTransform
// ・アクターのワールド変換が更新されたタイミングで呼ばれる。
// ・OBB の中心・軸・半径・バウンディングスフィア半径を再計算。
//------------------------------------------------------------------------------
// row-vector (v * M) 前提の版
void BoundingVolumeComponent::OnUpdateWorldTransform()
{
    const Vector3 posW = GetOwner()->GetPosition();
    const float   sc   = GetOwner()->GetScale();

    const Quaternion q = GetOwner()->GetRotation();
    const Matrix4    R = Matrix4::CreateFromQuaternion(q);

    // ローカルAABB center / extent
    const Vector3 localCenter = (mBoundingBox->min + mBoundingBox->max) * 0.5f;
    const Vector3 localExtent = (mBoundingBox->max - mBoundingBox->min) * 0.5f;

    // uniform scale 前提
    const Vector3 centerScaled = localCenter * sc;

    // row-vector: v' = v * R
    const Vector3 centerRot = Vector3::Transform(centerScaled, R);
    mObb->pos = posW + centerRot;

    //==========================================================
    // ★軸は「基底を回す」方式に統一（ここが重要）
    //==========================================================
    mObb->axisX = Vector3::Transform(Vector3::UnitX, R);
    mObb->axisY = Vector3::Transform(Vector3::UnitY, R);
    mObb->axisZ = Vector3::Transform(Vector3::UnitZ, R);

    if (mObb->axisX.LengthSq() > Math::NearZeroEpsilon) mObb->axisX.Normalize();
    else mObb->axisX = Vector3::UnitX;

    if (mObb->axisY.LengthSq() > Math::NearZeroEpsilon) mObb->axisY.Normalize();
    else mObb->axisY = Vector3::UnitY;

    if (mObb->axisZ.LengthSq() > Math::NearZeroEpsilon) mObb->axisZ.Normalize();
    else mObb->axisZ = Vector3::UnitZ;

    // extent (uniform scale)
    mObb->radius = Vector3(
        fabsf(localExtent.x) * sc,
        fabsf(localExtent.y) * sc,
        fabsf(localExtent.z) * sc
    );

    mRadius = mObb->radius.Length();
}
//------------------------------------------------------------------------------
// ComputeBoundingVolume（VA から生成）
// ・複数 VertexArray のポリゴン群からローカル AABB を計算。
// ・その後、デバッグ用の VAO とポリゴン配列を生成する。
//------------------------------------------------------------------------------
void BoundingVolumeComponent::ComputeBoundingVolume(
    const std::vector<std::shared_ptr<VertexArray>>& va)
{
    // ★ここは「一回だけ」初期化する
    mBoundingBox->min = Vector3(+Math::Infinity, +Math::Infinity, +Math::Infinity);
    mBoundingBox->max = Vector3(-Math::Infinity, -Math::Infinity, -Math::Infinity);

    bool any = false;

    for (const auto& v : va)
    {
        if (!v) continue;

        const auto& polygons = v->GetPolygons();
        for (const auto& poly : polygons)
        {
            any = true;

            mBoundingBox->min.x = std::min({ mBoundingBox->min.x, poly.a.x, poly.b.x, poly.c.x });
            mBoundingBox->max.x = std::max({ mBoundingBox->max.x, poly.a.x, poly.b.x, poly.c.x });

            mBoundingBox->min.y = std::min({ mBoundingBox->min.y, poly.a.y, poly.b.y, poly.c.y });
            mBoundingBox->max.y = std::max({ mBoundingBox->max.y, poly.a.y, poly.b.y, poly.c.y });

            mBoundingBox->min.z = std::min({ mBoundingBox->min.z, poly.a.z, poly.b.z, poly.c.z });
            mBoundingBox->max.z = std::max({ mBoundingBox->max.z, poly.a.z, poly.b.z, poly.c.z });
        }
    }

    // ポリゴンが無い場合の保険
    if (!any)
    {
        mBoundingBox->min = Vector3::Zero;
        mBoundingBox->max = Vector3::Zero;
    }

    CreateVArray();
    CreatePolygons();
}
//------------------------------------------------------------------------------
// CreatePolygons
// ・AABB（min/max）から 6 面 × 2 三角形 = 12 ポリゴンを生成。
// ・ローカル空間でのポリゴン情報として PhysWorld などに提供する。
//------------------------------------------------------------------------------
void BoundingVolumeComponent::CreatePolygons()
{
    Vector3 V0(mBoundingBox->min.x, mBoundingBox->min.y, mBoundingBox->min.z);
    Vector3 V1(mBoundingBox->max.x, mBoundingBox->min.y, mBoundingBox->min.z);
    Vector3 V2(mBoundingBox->max.x, mBoundingBox->min.y, mBoundingBox->max.z);
    Vector3 V3(mBoundingBox->min.x, mBoundingBox->min.y, mBoundingBox->max.z);
    Vector3 V4(mBoundingBox->min.x, mBoundingBox->max.y, mBoundingBox->min.z);
    Vector3 V5(mBoundingBox->max.x, mBoundingBox->max.y, mBoundingBox->min.z);
    Vector3 V6(mBoundingBox->max.x, mBoundingBox->max.y, mBoundingBox->max.z);
    Vector3 V7(mBoundingBox->min.x, mBoundingBox->max.y, mBoundingBox->max.z);
    
    // Z- 面（前）
    mPolygons[0]  = { V0, V4, V5 };
    mPolygons[1]  = { V0, V5, V1 };
    
    // X+ 面（右）
    mPolygons[2]  = { V1, V5, V6 };
    mPolygons[3]  = { V1, V6, V2 };
    
    // Z+ 面（背面）
    mPolygons[4]  = { V2, V6, V7 };
    mPolygons[5]  = { V2, V7, V3 };
    
    // X- 面（左）
    mPolygons[6]  = { V3, V7, V4 };
    mPolygons[7]  = { V3, V4, V0 };
    
    // Y+ 面（上）
    mPolygons[8]  = { V4, V7, V6 };
    mPolygons[9]  = { V4, V6, V5 };
    
    // Y- 面（下）
    mPolygons[10] = { V3, V0, V1 };
    mPolygons[11] = { V3, V1, V2 };
}

//------------------------------------------------------------------------------
// ComputeBoundingVolume（min/max 直接指定）
// ・エディタや JSON から AABB を直指定したい場合に使用。
//------------------------------------------------------------------------------
void BoundingVolumeComponent::ComputeBoundingVolume(const Vector3& min, const Vector3& max)
{
    
    mBoundingBox->min = Vector3(+Math::Infinity, +Math::Infinity, +Math::Infinity);
    mBoundingBox->max = Vector3(-Math::Infinity, -Math::Infinity, -Math::Infinity);
    
    mBoundingBox->min = min;
    mBoundingBox->max = max;
    
    CreateVArray();
    CreatePolygons();
}

//------------------------------------------------------------------------------
// AdjustBoundingBox
// ・既存の AABB に対して、位置オフセットとスケールを適用して再構成。
// ・「読み込んだモデルの当たり判定をちょっとだけ広げる/ずらす」用途など。
//------------------------------------------------------------------------------
void BoundingVolumeComponent::AdjustBoundingBox(const Vector3& pos, const Vector3& sc)
{
    // 平行移動
    mBoundingBox->max += pos;
    mBoundingBox->min += pos;
    
    // 各軸でスケール
    mBoundingBox->max.x *= sc.x;
    mBoundingBox->min.x *= sc.x;
    mBoundingBox->max.y *= sc.y;
    mBoundingBox->min.y *= sc.y;
    mBoundingBox->max.z *= sc.z;
    mBoundingBox->min.z *= sc.z;
    
    CreateVArray();
    CreatePolygons();
}

//------------------------------------------------------------------------------
// CreateVArray
// ・AABB をもとにボックス用頂点バッファを作り、デバッグ用 VAO を生成。
// ・WireframeComponent に渡すことで境界ボックスを可視化する。
//------------------------------------------------------------------------------
void BoundingVolumeComponent::CreateVArray()
{
    Vector3 v0(mBoundingBox->min.x, mBoundingBox->min.y, mBoundingBox->min.z);
    Vector3 v1(mBoundingBox->max.x, mBoundingBox->min.y, mBoundingBox->min.z);
    Vector3 v2(mBoundingBox->max.x, mBoundingBox->min.y, mBoundingBox->max.z);
    Vector3 v3(mBoundingBox->min.x, mBoundingBox->min.y, mBoundingBox->max.z);

    Vector3 v4(mBoundingBox->min.x, mBoundingBox->max.y, mBoundingBox->min.z);
    Vector3 v5(mBoundingBox->max.x, mBoundingBox->max.y, mBoundingBox->min.z);
    Vector3 v6(mBoundingBox->max.x, mBoundingBox->max.y, mBoundingBox->max.z);
    Vector3 v7(mBoundingBox->min.x, mBoundingBox->max.y, mBoundingBox->max.z);

    // 12 edges = 24 vertices
    float verts[] =
    {
        // bottom
        v0.x,v0.y,v0.z, 0,0,0, 0,0,   v1.x,v1.y,v1.z, 0,0,0, 0,0,
        v1.x,v1.y,v1.z, 0,0,0, 0,0,   v2.x,v2.y,v2.z, 0,0,0, 0,0,
        v2.x,v2.y,v2.z, 0,0,0, 0,0,   v3.x,v3.y,v3.z, 0,0,0, 0,0,
        v3.x,v3.y,v3.z, 0,0,0, 0,0,   v0.x,v0.y,v0.z, 0,0,0, 0,0,

        // top
        v4.x,v4.y,v4.z, 0,0,0, 0,0,   v5.x,v5.y,v5.z, 0,0,0, 0,0,
        v5.x,v5.y,v5.z, 0,0,0, 0,0,   v6.x,v6.y,v6.z, 0,0,0, 0,0,
        v6.x,v6.y,v6.z, 0,0,0, 0,0,   v7.x,v7.y,v7.z, 0,0,0, 0,0,
        v7.x,v7.y,v7.z, 0,0,0, 0,0,   v4.x,v4.y,v4.z, 0,0,0, 0,0,

        // vertical
        v0.x,v0.y,v0.z, 0,0,0, 0,0,   v4.x,v4.y,v4.z, 0,0,0, 0,0,
        v1.x,v1.y,v1.z, 0,0,0, 0,0,   v5.x,v5.y,v5.z, 0,0,0, 0,0,
        v2.x,v2.y,v2.z, 0,0,0, 0,0,   v6.x,v6.y,v6.z, 0,0,0, 0,0,
        v3.x,v3.y,v3.z, 0,0,0, 0,0,   v7.x,v7.y,v7.z, 0,0,0, 0,0,
    };

    if (mWireframe)
    {
        mWireframe->SetVertexArray(
            std::make_shared<VertexArray>(
                verts,
                24,
                nullptr,
                0
            )
        );
    }
}

//------------------------------------------------------------------------------
// GetWorldAABB
// ・スケールと位置を反映した「ワールド空間の AABB」を返す。
// ・回転は無視されるので、ざっくりとした広めの当たり判定として利用。
//------------------------------------------------------------------------------
static Vector3 AbsVec(const Vector3& v)
{
    return Vector3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

Cube BoundingVolumeComponent::GetWorldAABB() const
{
    Cube worldBox;
    if (!mObb) return worldBox;

    const Vector3 ex = mObb->axisX * mObb->radius.x;
    const Vector3 ey = mObb->axisY * mObb->radius.y;
    const Vector3 ez = mObb->axisZ * mObb->radius.z;

    // ワールド座標軸に対するAABB半径（各成分の絶対値を足す）
    const Vector3 aabbExt =
        AbsVec(ex) + AbsVec(ey) + AbsVec(ez);

    worldBox.min = mObb->pos - aabbExt;
    worldBox.max = mObb->pos + aabbExt;

    return worldBox;
}
void BoundingVolumeComponent::ComputeFromMeshComponent(const MeshComponent* meshComp)
{
    if (!meshComp) return;

    auto mesh = meshComp->GetMesh();
    if (!mesh) return;

    // 1) 素のローカル AABB をメッシュの VA から計算
    ComputeBoundingVolume(mesh->GetVertexArray());

    // 2) MeshComponent が持っているローカル補正を一回だけ焼き込む
    const Vector3 offset = meshComp->GetLocalPosition(); // 使わないなら Zero でもOK
    const float   s      = meshComp->GetLocalScale();    // or Vector3 if非一様

    AdjustBoundingBox(offset, Vector3(s, s, s));
}

} // namespace toy
