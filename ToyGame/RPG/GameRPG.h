#include "ToyLib.h"
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
    std::unique_ptr<class toy::WeatherManager> mWeather;
    class toy::TextSpriteComponent* mTextComp;

    std::unique_ptr<class OutdoorStage> mStage;
};
