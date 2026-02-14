#include "TestApp.h"
#include "Render/RenderBackendState.h"

VKTest::VKTest()
: toy::Application()
{
    GetAssetManager()->SetAssetsPath("ToyGame/Assets/VKTest/");
    toy::RenderBackendState::Get().Set(toy::RenderBackendType::Vulkan);
}

VKTest::~VKTest()
{
}

void VKTest::InitGame()
{
    
    auto a = CreateActor<toy::Actor>();
    auto sp = a->CreateComponent<toy::SpriteComponent>();
    auto tex = GetAssetManager()->GetTexture("youkai_kappa.png");
    sp->SetTexture(tex);
    
    auto islandActor = CreateActor<toy::Actor>();
    auto islandMesh = islandActor->CreateComponent<toy::MeshComponent>();
    islandMesh->SetMesh(GetAssetManager()->GetMesh("island.x"));
    
}

static float r = 0.0f;
static float g = 0.0f;
static float b = 0.5f;

void VKTest::UpdateGame(float deltaTime)
{
    auto renderer = GetRenderer();
    r += 0.01f;
    if (r > 1.0f) r = 0.0f;

    renderer->SetClearColor(Vector3(r, g, b));
}

void VKTest::ShutdownGame()
{

    
}
