#include "Skirmisher.h"

namespace {

std::unique_ptr<Skirmisher> MakeSkirmisherWithBehavior(toy::Application* app, const Vector3& position,
                                                         const toy::kit::HumanoidDesc& bodyDesc,
                                                         std::unique_ptr<toy::kit::IBehavior> behavior)
{
    auto skirmisher = std::make_unique<Skirmisher>(app, std::move(behavior), bodyDesc);
    skirmisher->GetBody().SetPosition(position);
    return skirmisher;
}

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Skirmisher> MakeSkirmisher(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                            const toy::kit::HumanoidDesc& bodyDesc,
                                            const toy::kit::FleeBehaviorDesc& behaviorDesc)
{
    auto behavior = std::make_unique<toy::kit::FleeBehavior>(behaviorDesc);
    behavior->SetTarget(target);
    return MakeSkirmisherWithBehavior(app, position, bodyDesc, std::move(behavior));
}

//-----------------------------------------------------------------------------
std::unique_ptr<Skirmisher> MakeSkirmisher(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                            const toy::kit::HumanoidDesc& bodyDesc,
                                            const toy::kit::ChaseBehaviorDesc& behaviorDesc)
{
    auto behavior = std::make_unique<toy::kit::ChaseBehavior>(behaviorDesc);
    behavior->SetTarget(target);
    return MakeSkirmisherWithBehavior(app, position, bodyDesc, std::move(behavior));
}
