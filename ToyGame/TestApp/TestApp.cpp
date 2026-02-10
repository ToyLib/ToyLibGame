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

float r = 0.0f;

void VKTest::UpdateGame(float deltaTime)
{
    auto renderer = GetRenderer();
    r += 0.01f;
    if (r > 1.0f) r = 0.0f;
    renderer->SetClearColor(Vector3(r, 0.0f, 0.5f));
}

void VKTest::ShutdownGame()
{

    
}
