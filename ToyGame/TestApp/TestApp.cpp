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

    auto lm = GetRenderer()->GetLightingManager();
    toy::DirectionalLight dirLight;
    dirLight.Position = Vector3(1.0f, 20.0f, -1.0f);
    dirLight.Target = Vector3(0.0f, 0.0f, 0.0f);
    dirLight.DiffuseColor = Vector3(0.4f, 0.0f, 0.0f);
    lm->SetDirectionalLight(dirLight);
    lm->SetAmbientColor(Vector3(0.5f, 0.5f, 0.5f));

    
    auto a = CreateActor<toy::Actor>();
    auto sp = a->CreateComponent<toy::SpriteComponent>();
    auto tex = GetAssetManager()->GetTexture("youkai_kappa.png");
    sp->SetTexture(tex);
    
    mAct = CreateActor<toy::Actor>();
    auto mesh = mAct->CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(GetAssetManager()->GetMesh("brick.x"));
    mesh->SetLocalScale(0.3f);
    mAct->SetPosition(Vector3(0.0, 0.0f, 0.0f));
    mesh->SetToonRender(false);
    mesh->SetContourFactor(1.1f);
    
}

static float r = 0.0f;
static float g = 0.0f;
static float b = 0.5f;
static float ang = 30.0f;

void VKTest::UpdateGame(float deltaTime)
{
    auto renderer = GetRenderer();
    r += 0.01f;
    if (r > 1.0f) r = 0.0f;

    renderer->SetClearColor(Vector3(r, g, b));
    
    
    ang += 2.0f;
    Quaternion q = Quaternion(Vector3(0.0f, 1.0f, 0.0f), Math::ToRadians(ang));
    mAct->SetRotation(q);
}

void VKTest::ShutdownGame()
{

    
}
