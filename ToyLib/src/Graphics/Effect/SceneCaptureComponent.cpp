#include "Graphics/Effect/SceneCaptureComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderTarget.h"
#include "Asset/Material/Texture.h"

#include "glad/glad.h"

namespace toy {

SceneCaptureComponent::SceneCaptureComponent(Actor* owner)
    : Component(owner)
    , mView(Matrix4::Identity)
    , mProj(Matrix4::Identity)
{
}

void SceneCaptureComponent::Init(const Desc& desc)
{
    mDesc = desc;
    mDesc.width  = (mDesc.width  > 0) ? mDesc.width  : 1;
    mDesc.height = (mDesc.height > 0) ? mDesc.height : 1;

    mRT = std::make_shared<RenderTarget>();
    mRT->Create(mDesc.width, mDesc.height);

    mAcc = 0.0f;
}

std::shared_ptr<Texture> SceneCaptureComponent::GetColorTexture() const
{
    return mRT ? mRT->GetColorTexture() : nullptr;
}

void SceneCaptureComponent::SetViewProj(const Matrix4& view, const Matrix4& proj)
{
    mView = view;
    mProj = proj;
}

void SceneCaptureComponent::Update(float deltaTime)
{
    if (!mDesc.enabled || !mRT)
    {
        return;
    }
    if (mDesc.updateHz <= 0.0f)
    {
        Capture();
        return;
    }

    const float interval = 1.0f / mDesc.updateHz;
    mAcc += deltaTime;
    if (mAcc >= interval)
    {
        mAcc = 0.0f;
        Capture();
    }
}

void SceneCaptureComponent::Capture()
{
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }
    // ---- Actor transform から camera basis を作る ----
    const Matrix4 world = GetOwner()->GetWorldTransform();

    // ToyLibの軸に合わせて：例) 前方 = +Z, 上 = +Y
    Vector3 camPos = world.GetTranslation();
    const Vector3 camFwd = -1.0f * world.GetZAxis();   // -Z が前
    const Vector3 camUp  = world.GetYAxis();

    camPos = camPos - camFwd * 10.0f;
    const Vector3 target = camPos + camFwd * 100.0f;

    mView = Matrix4::CreateLookAt(camPos, target, camUp);

    // ---- Proj（RTサイズから aspect を作る）----
    const float w = (float)mRT->GetWidth();
    const float h = (float)mRT->GetHeight();

    mProj = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mDesc.fov),
        mDesc.width,
        mDesc.height,
        0.1f,
        1000.0f
    );


    renderer->DrawToRenderTarget(mRT, mView, mProj, mDesc.drawUI);

}

} // namespace toy
