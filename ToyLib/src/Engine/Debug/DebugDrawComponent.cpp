#include "Engine/Debug/DebugDrawComponent.h"
#include "Engine/Runtime/InputSystem.h"

#include "Engine/Debug/DebugDraw.h"
#include "Engine/Debug/DebugDrawSystem.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

#include "Render/IRenderer.h"
#include "Render/RenderQueue.h"
#include "Render/RenderItem.h"

#include "Asset/Geometry/VertexArray.h"

#include <vector>

namespace toy
{

DebugDrawComponent::DebugDrawComponent(Actor* owner,
                                       int drawOrder,
                                       VisualLayer layer)
    : VisualComponent(owner, drawOrder, layer)
{
    mPipelineName = "UnlitWire";
    mIsVisible = false;
}

void DebugDrawComponent::PreDraw()
{
    DebugDrawSystem* sys = DebugDraw::GetSystem();
    if (!sys)
    {
        mVertexArray.reset();
        return;
    }

    const auto& lines = sys->GetLines();
    if (lines.empty())
    {
        mVertexArray.reset();
        return;
    }

    // ----------------------------------------------------------
    // 1 line = 2 verts
    // ----------------------------------------------------------
    const unsigned int numVerts   = static_cast<unsigned int>(lines.size() * 2);
    const unsigned int numIndices = numVerts;

    std::vector<float> verts;
    std::vector<float> norms;
    std::vector<float> uvs;
    std::vector<unsigned int> indices;

    verts.reserve(numVerts * 3);
    norms.reserve(numVerts * 3);
    uvs.reserve(numVerts * 2);
    indices.reserve(numIndices);

    unsigned int vi = 0;

    for (const DebugLine& line : lines)
    {
        // A
        verts.push_back(line.a.x);
        verts.push_back(line.a.y);
        verts.push_back(line.a.z);

        norms.push_back(0.0f);
        norms.push_back(0.0f);
        norms.push_back(1.0f);

        uvs.push_back(0.0f);
        uvs.push_back(0.0f);

        indices.push_back(vi++);

        // B
        verts.push_back(line.b.x);
        verts.push_back(line.b.y);
        verts.push_back(line.b.z);

        norms.push_back(0.0f);
        norms.push_back(0.0f);
        norms.push_back(1.0f);

        uvs.push_back(0.0f);
        uvs.push_back(0.0f);

        indices.push_back(vi++);
    }

    mVertexArray = std::make_shared<VertexArray>(
        numVerts,
        verts.data(),
        norms.data(),
        uvs.data(),
        numIndices,
        indices.data()
    );
}

void DebugDrawComponent::ProcessInput(const struct InputState& state)
{
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_F5) == EPressed)
    {
        mIsVisible = !mIsVisible;
    }
}


void DebugDrawComponent::GatherRenderItems(RenderQueue& q)
{
    if (!mIsVisible)
    {
        return;
    }

    Actor* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    Application* app = owner->GetApp();
    if (!app)
    {
        return;
    }

    toy::IRenderer* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    PreDraw();

    if (!mVertexArray)
    {
        return;
    }

    // ----------------------------------------------------------
    // 今は 1 draw / 1 color ではなく、shader 側で固定色を使う想定だと
    // 色ごとの分離が必要になる。
    //
    // まずは最低限、白固定で通すか、
    // DebugPayload 1個を代表色として使う。
    //
    // ここでは白固定で最小実装にする。
    // ----------------------------------------------------------
    DebugPayload dp {};
    dp.color = Vector3(1.0f, 1.0f, 1.0f);
    dp.color = GetOwner()->GetApp()->GetRenderer()->GetWireColor();
    dp.alpha = 1.0f;

    const uint32_t payloadIndex = q.PushDebugPayload(dp);

    RenderItem it {};
    it.pass      = RenderPass::World;
    it.layer     = GetLayer();
    it.drawOrder = GetDrawOrder();

    it.type     = RenderItemType::Debug;
    it.dispatch = GetDispatch(it.type);

    it.geometry.ptr = mVertexArray.get();
    it.topology     = PrimitiveTopology::Lines;
    it.vertexCount  = static_cast<int>(mVertexArray->GetNumVerts());
    it.indexCount   = static_cast<int>(mVertexArray->GetNumIndices());

    it.pipeline = renderer->GetPipelineHandle(mPipelineName);

    // DebugDraw はすでにワールド座標で積んでいるので Identity
    it.world    = Matrix4::Identity;
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();

    it.blend      = toy::BlendMode::Alpha;
    it.depthTest  = true;
    it.depthWrite = true;
    it.cull       = CullMode::None;
    it.frontFace  = FrontFace::CCW;

    it.payloadIndex = payloadIndex;

    q.Push(it);
}


} // namespace toy::kit
