#include "KitPrefab/StaticObjectPlacement.h"
#include "KitPrefab/StaticObject.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
std::unique_ptr<StaticObject> MakeStaticObject(toy::Application* app, const StaticObjectPlacement& placement)
{
    auto obj = std::make_unique<StaticObject>(app, placement.desc);
    obj->SetPosition(placement.position);
    obj->SetRotation(placement.rotation);
    return obj;
}

//-----------------------------------------------------------------------------
std::vector<std::unique_ptr<StaticObject>> MakeStaticObjects(toy::Application* app,
                                                              const std::vector<StaticObjectPlacement>& placements)
{
    std::vector<std::unique_ptr<StaticObject>> objects;
    objects.reserve(placements.size());

    for (const auto& placement : placements)
    {
        objects.push_back(MakeStaticObject(app, placement));
    }
    return objects;
}

} // namespace toy::kit
