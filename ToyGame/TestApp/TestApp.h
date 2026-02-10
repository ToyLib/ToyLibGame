#include "ToyLib.h"
#include "ToyKit.h"


class VKTest : public toy::Application
{
public:
    VKTest();
    ~VKTest();
    void InitGame() override;
    void UpdateGame(float deltaTime) override;
    void ShutdownGame() override;

};
