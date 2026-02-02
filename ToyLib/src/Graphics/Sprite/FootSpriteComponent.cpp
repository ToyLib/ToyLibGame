#include "Graphics/Sprite/FootSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

// ★ Ground pose を取るため（ある場合だけ使う）
#include "Physics/GravityComponent.h"

namespace toy {

//------------------------------------------------------------------------------
// 地面法線に合わせる回転（Yaw込み）
//------------------------------------------------------------------------------
static Matrix4 BuildAlignToGround(const Vector3& groundNormal, float yawRad)
{
    Vector3 up = groundNormal;
    if (up.LengthSq() <= Math::NearZeroEpsilon)
    {
        up = Vector3::UnitY;
    }
    up.Normalize();
    
    // yaw から “ワールド水平の前” を作る（+Z基準）
    Vector3 fwd(Math::Sin(yawRad), 0.0f, Math::Cos(yawRad));
    
    // 前ベクトルを地面平面に射影して直交化
    fwd = fwd - up * Vector3::Dot(fwd, up);
    if (fwd.LengthSq() <= Math::NearZeroEpsilon)
    {
        // up とほぼ平行になったら別軸で作る
        fwd = Vector3::Cross(Vector3::UnitX, up);
    }
    fwd.Normalize();
    
    Vector3 right = Vector3::Cross(up, fwd);
    if (right.LengthSq() <= Math::NearZeroEpsilon)
    {
        right = Vector3::UnitX;
    }
    right.Normalize();
    
    // row-vector: 行に軸を詰める
    Matrix4 m = Matrix4::Identity;
    m.SetXAxis(right);
    m.SetYAxis(up);
    m.SetZAxis(fwd);
    return m;
}

//------------------------------------------------------------------------------
// ctor
//------------------------------------------------------------------------------
FootSpriteComponent::FootSpriteComponent(Actor* owner, int drawOrder, VisualLayer layer)
: VisualComponent(owner, drawOrder, layer)
{
    mIsVisible = true;
    
    // Unlit（Phong互換uniform名がある前提）
    // ※新パスでは shader handle に積むだけなので、ここで取っておく
    if (auto* r = owner->GetApp()->GetRenderer())
    {
        mShader = r->GetShader("Unlit");
    }
}

void FootSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}

//------------------------------------------------------------------------------
// World行列（元の実装と同等）
//------------------------------------------------------------------------------
Matrix4 FootSpriteComponent::BuildWorldMatrix() const
{
    Vector3 pos = GetOwner()->GetPosition();
    
    const GravityComponent* grav = GetOwner()->GetComponent<GravityComponent>();
    
    //========================================
    // (1) 基準XZを “足OBB下面中心” に寄せる
    //========================================
    if (grav && grav->HasFootBottom())
    {
        const Vector3 b = grav->GetFootBottomPos();
        pos.x = b.x;
        pos.z = b.z;
    }
    
    //========================================
    // (2) Yは groundY にスナップ
    //   ※現状 GravityComponent に SmoothGroundPose API が無いので
    //     HasGroundPose()/GetGroundPose() のみを使う
    //========================================
    if (mSnapToGround && grav && grav->HasGroundPose())
    {
        pos.y = grav->GetGroundPose().y;
    }
    
    // offset + lift
    pos += mOffsetPosition;
    pos.y += mGroundLift;
    
    // scale（XY quad）
    Matrix4 scale = Matrix4::CreateScale(
                                         mWidth * mOffsetScale,
                                         mDepth * mOffsetScale,
                                         1.0f);
    
    // XY quad を地面に寝かせる（XY → XZ）
    Matrix4 rotLay = Matrix4::CreateRotationX(Math::ToRadians(90.0f));
    
    // slope alignment（yaw込みで作るので rotY は不要）
    Matrix4 rot = Matrix4::Identity;
    
    if (mAlignToGround && grav && grav->HasGroundPose())
    {
        rot = BuildAlignToGround(grav->GetGroundPose().normal, mYaw);
    }
    else
    {
        // 従来どおり水平yawだけ
        rot = Matrix4::CreateRotationY(mYaw);
    }
    
    Matrix4 trans = Matrix4::CreateTranslation(pos);
    
    // row-vector: S * R * T
    //  - rotLay: 板を寝かせる
    //  - rot   : yaw or slope+ yaw
    return scale * rotLay * rot * trans;
}

//------------------------------------------------------------------------------
// 新パス：RenderQueue に積む
//------------------------------------------------------------------------------
void FootSpriteComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible)
        return;
    
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer || !mShader)
        return;
    
    auto vao = renderer->GetSpriteQuad();
    if (!vao)
        return;
    
    PreDraw();
    
    RenderItem it;
    it.pass      = RenderPass::World;
    it.layer     = mLayer;          // Effect3D をそのまま通す
    it.drawOrder = mDrawOrder;
    
    // 3D板ポリなので Billboard 扱い（メッシュ扱いでもOKだが texture/unlit を分けやすい）
    it.type      = RenderItemType::Billboard;
    it.dispatch  = GetDispatch(it.type);
    
    // geometry
    it.topology   = PrimitiveTopology::Triangles;
    it.geometry   = renderer->GetSpriteQuadHandle(); // 既存共通quad
    it.indexCount = 6;
    
    // shader
    it.shader = renderer->GetShaderHandle("Unlit");
    
    // transforms（row-vector 規約：view * proj）
    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    it.viewProj = view * proj;
    it.world    = BuildWorldMatrix();
    
    // render state（元の用途＝足元影/リングを想定）
    it.blend      = (mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha);
    it.depthTest  = true;
    it.depthWrite = true;                 // ←足元板は基本 OFF（必要なら外から変更する設計でもOK）
    it.cull       = CullMode::None;        // 両面見せるほうが安全
    it.frontFace  = FrontFace::CCW;
    
    // texture
    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;
    
    // ※Unlit 側の tint/alpha 等を “RenderItem に持たせてない” 前提なので、
    //   透明はテクスチャのαを使う運用（元の挙動を壊さない最小移行）
    //   どうしても tint/alpha を使いたい場合は RenderItem 拡張 or Material 化で後で対応。
    
    queue.Push(it);
    
    PostDraw();
}

} // namespace toy
