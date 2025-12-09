#include "ToyLib.h"
#include "ToyKit.h"
#include <string>
#include <memory>


class GameRPG : public toy::Application
{
public:
    GameRPG();
    ~GameRPG();
protected:
    void InitGame() override;
    void LoadData();
    void UpdateGame(float deltaTime) override;
    void ShutdownGame() override;
private:
    class toy::TextSpriteComponent* mTextComp;
    std::unique_ptr<class OutdoorStage> mStage;
};
