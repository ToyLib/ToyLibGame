#include "TestApp.h"

VKTest::VKTest()
: toy::Application()
{
}

VKTest::~VKTest()
{
}

void VKTest::InitGame()
{
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
